# GraphLayout Semantic Visual Editing And Native Preview Design

Date: 2026-06-01

Status: design only. This document intentionally does not contain an executable implementation plan.

## 背景

GraphLayout 当前已经拆分为五个语义布局场景：

- `LinearExecChain`
- `PureDataSubgraph`
- `NodeInputCluster`
- `MultiExecOutput`
- `Occupancy`

Layout 页面中已经存在可拖拽的语义配置画布，以及 RuleSet JSON / SettingRow 编辑入口。后续新增的箭头、中文 Hover Tips、原生 GraphPanel 预览，都应基于同一套语义模型，而不是在 Slate 绘制层分别硬编码。

当前已记录的前置问题是：部分语义页拖拽后切换页面再切回来会恢复默认值。源码证据显示，五个语义页目前主要从 RuleSet 标量重新构建 `RoleCenters`，只有 `RoleOverview` 会读写 `EditorCanvasRoleCenters`。因此语义页缺少 scene-scoped visual state，不能作为 Preview 的可靠输入。

## 已确认需求

1. 可拖拽语义布局中的关系线需要渲染小箭头，明确流向。
2. 可拖拽语义布局中的 Node Hover 需要显示中文 Tips。
3. Layout 页面需要新增 Preview 模式。
4. Preview 不读取当前打开的真实 `UEdGraph`。
5. Preview 基于当前选中的语义和当前拖拽 RuleSet，生成内置固定复杂 sample graph。
6. Preview 使用 UE 原生 `SGraphEditor` / transient `UEdGraph` 渲染，而不是 Slate 自绘假图。
7. Preview 中当前可拖拽配置区域完全切换成 GraphPanel，并提供“返回编辑”按钮。
8. Preview 模式下隐藏五个语义切换按钮。
9. Preview 不能出现阻塞式卡顿；切换到 GraphPanel shell 和创建 sample/layout 必须拆开。
10. Worker 只能处理纯数据 descriptor / snapshot / layout plan；`UEdGraph`、K2 node、Pin link、`SGraphEditor` 刷新必须由 GameThread 分帧 materialize。

## 非目标

- 不在 Preview 中修改当前打开的真实 Blueprint graph。
- 不移动真实资产节点，不 mark dirty，不 save。
- 不把 sample graph 作为项目资产保存。
- 不把 runtime layout 算法复制到 UI widget。
- 不新增 TaskSpec / GraphWrite / Review schema 字段表达 layout。
- 不把五个语义重新混回单一 `PatternLayout`。
- 不在 `SBlueprintHelperLayoutRuleEditor` 中堆积 preview lifecycle、graph 构建、solver 编排逻辑。

## 设计原则

### 单一语义模型

箭头、Hover Tips、拖拽持久化、Preview 都应消费同一套 semantic scene model。

该模型至少需要表达：

- scene id，例如 `pure_data_subgraph`。
- scene display name。
- role nodes。
- role node 中文说明。
- scene edge list。
- edge direction。
- default role centers。
- persisted role centers。
- scene 到 RuleSet solver 参数的投影规则。

UI 可以展示和编辑模型，但不应拥有模型语义判断。

### Runtime Source Of Truth

`FRuleSet` / RuleSet JSON 仍然是 GraphLayout runtime 的配置来源。scene visual state 应作为 RuleSet 的 editor-facing 扩展保存，而不是另建 UI 本地配置文件。

建议概念结构：

```json
{
  "editor_canvas": {
    "scenes": {
      "linear_exec_chain": {
        "role_centers": {
          "EventEntry": { "x": 92, "y": 126 },
          "ExecNode": { "x": 452, "y": 126 }
        }
      },
      "pure_data_subgraph": {
        "role_centers": {
          "VariableInput": { "x": 96, "y": 106 },
          "OperatorOrCompare": { "x": 356, "y": 150 },
          "PureFunction": { "x": 656, "y": 194 }
        }
      }
    }
  }
}
```

旧的 `editor_canvas.role_centers` 不应继续保留为兼容读取、迁移输入或 RoleOverview 的旁路来源。实现时应移除该旧字段的 import/export/runtime 消费路径，避免新语义模型继续依赖 legacy visual state。已经存在旧字段的项目配置应被新 schema 覆盖或由用户重新保存生成，不做自动迁移。

### Legacy Removal Boundary

依据 `AGENT.md` 的架构要求，已确认不做旧字段兼容：

- 不保留 `editor_canvas.role_centers`。
- 不保留 RoleOverview 对 `EditorCanvasRoleCenters` 的特殊读写语义。
- 不把旧字段自动迁移到 `editor_canvas.scenes`。
- 不让 Runtime 继续直接消费旧 `EditorCanvasRoleCenters` map。

新的唯一 visual state 入口是 `editor_canvas.scenes.<scene>.role_centers`，再由 GraphLayout semantic scene adapter 投影到 solver 参数或 normalized anchors。

## 组件边界

### LayoutRuleEditor Widget

`SBlueprintHelperLayoutRuleEditor` 负责：

- 当前语义页选择。
- Edit / Preview 模式切换。
- 按钮、状态文本、错误展示。
- 将拖拽事件转发给 semantic scene model。
- 将 Preview 请求转发给 PreviewService。

它不负责：

- 创建 sample graph 的具体节点。
- 运行 solver。
- 管理 worker job 细节。
- 直接维护 preview materialization 队列。
- 在 UI 内重写 layout 算法。

### Semantic Scene Model / Adapter

新增 GraphLayout UI-facing model/adapter，负责：

- 提供五个 scene 的 node metadata 和 edge metadata。
- 根据 scene id 返回默认 role centers。
- 读写 scene-scoped persisted role centers。
- 把 scene role centers 投影回 `FRuleSet` 现有字段，例如 `ExecColumnSpacing`、`InputPinRowSpacing`、`PureInputOffsetX`、`BranchRowSpacing`、`CollisionPaddingX/Y`。
- 在需要时为 runtime solver 提供 normalized anchors。

该层是解决拖拽切页丢失 bug 的共同前置。

### Preview Service

新增 PreviewService / PreviewBuilder，负责：

- 接收当前 scene id 和当前 RuleSet JSON。
- 校验 RuleSet。
- 创建 preview job。
- 管理 job id、取消、完成、错误状态。
- 将 worker 产出的纯数据 preview result 交给 GameThread materializer。

PreviewService 不直接依赖 Layout 页面内部 widget 结构。

### Sample Graph Factory

每个语义固定一套内置复杂 sample graph。Factory 输出纯数据 descriptor，而不是直接在 worker 创建 `UEdGraph`。

descriptor 应描述：

- sample nodes。
- node semantic role。
- node title / intended K2 class or factory key。
- pins。
- links。
- initial logical positions。
- graph-level notes or expected focus area。

### Preview Materializer

GameThread materializer 负责把 preview descriptor 分帧落成 transient `UEdGraph`：

- 创建 transient graph。
- 分帧创建 K2 nodes。
- 分帧创建或重建 pins。
- 分帧连接 pins。
- 写入 solver target positions。
- 通知 `SGraphEditor` 刷新。

Materializer 必须支持取消。用户点击“返回编辑”后，未完成 materialization 不应继续向已丢弃的 preview graph 写入节点。

## 编辑态行为

Edit 模式显示当前可拖拽语义画布。

五个语义按钮在 Edit 模式下可见：

- `Linear Exec`
- `Pure Data`
- `Input Cluster`
- `Multi Exec`
- `Occupancy`

拖拽节点时：

1. 更新当前 scene 的 role centers。
2. 通过 adapter 更新 RuleSet JSON 中对应 scene state。
3. 同步投影到 solver 参数。
4. 保存 RuleSet JSON。
5. 刷新 SettingRow 和 canvas footer。

切换语义页时：

1. 先提交当前 scene。
2. 加载目标 scene 的 persisted centers。
3. 如果没有 persisted centers，使用目标 scene default centers。
4. 不再从共享标量无损反推 UI 原始位置。

## 箭头设计

可拖拽 canvas 的关系线改为读取 scene edge list。

每条 edge 至少包含：

- from role。
- to role。
- edge category，例如 exec、data、collision。
- display color。
- direction。

绘制时：

- 线条仍连接两个 role node center 或 node 边界附近。
- 在 `to role` 侧绘制小箭头。
- 箭头只表达语义流向，不写入 RuleSet。
- Exec 与 Data 可以沿用现有颜色区分，箭头颜色与线一致。

## 中文 Hover Tips 设计

每个 role node 的 Tips 从 scene metadata 读取。

Tips 内容应包含：

- 中文角色名称。
- 该节点在当前语义中的含义。
- 拖动它会影响哪些布局关系。

示例：

- `Data Leaf`：数据叶子节点，例如变量 Get 或 literal。拖动会影响数据输入链最左侧节点相对消费节点的位置。
- `Data Transform`：纯数据转换或聚合节点，例如 MakeArray、运算、纯函数。拖动会影响纯数据子图内部层级与间距。
- `Branch Row`：多 Exec 出口后的分支行锚点。拖动会影响分支 row 与主 row 的垂直间距。

Tips 是编辑辅助，不进入 solver。

## Preview 模式交互

点击 `预览` 后：

1. 当前可拖拽配置区域立即切换为 Preview shell。
2. 五个语义切换按钮隐藏。
3. 显示“返回编辑”按钮。
4. 显示当前语义名和 loading 状态。
5. 主线程创建空 transient preview graph 或预览承载容器。
6. PreviewService 启动异步 job。

Preview shell 不等待 sample graph 创建完成才显示。

Preview ready 后：

- 同一区域显示只读 `SGraphEditor`。
- `SGraphEditor` 渲染 transient `UEdGraph` 中的真实 UE 节点、Pin 和连线。
- 用户点击“返回编辑”后，取消 job、释放 transient graph、恢复当前语义的 draggable canvas。

Preview error 时：

- 保持 Preview shell。
- 显示错误原因。
- 用户可以返回编辑。

## 异步状态机

Preview 状态建议为：

```text
Edit
  -> PreviewLoadingShell
  -> PreviewBuildingData
  -> PreviewMaterializingGraph
  -> PreviewReady
  -> PreviewError
  -> Cancelled
```

### PreviewLoadingShell

GameThread 立即切 UI，不做重活。

### PreviewBuildingData

Worker thread 处理纯数据：

- 解析 RuleSet 副本。
- 读取 scene descriptor。
- 构建 sample graph descriptor。
- 构建 `FGraphSnapshot` 或等价纯数据 snapshot。
- 调用不依赖 UObject 的 GraphLayout solver。
- 输出 preview materialization plan。

Worker 不允许创建或修改 UObject。

### PreviewMaterializingGraph

GameThread 分帧执行：

- 每帧创建有限数量节点。
- 每帧连接有限数量 pins。
- 每帧写入有限数量坐标。
- 最后刷新 `SGraphEditor`。

帧预算可复用或对齐现有 RuleSet apply 参数，例如 `MaxNodesPerFrame`、`MaxMillisecondsPerFrame`，但 Preview 不应修改真实 graph。

### Cancelled

以下行为应取消 job：

- 用户点击返回编辑。
- 用户关闭 Layout 页面。
- RuleSet 被外部重新导入。
- Widget 析构。

取消后，旧 job 的完成回调必须检查 job id，避免向新 preview graph 或已销毁 widget 写入。

## 固定 Sample Graph

### LinearExecChain

样例包含：

- Event entry。
- Reset / Init 类调用节点。
- Set Variable。
- PrintString。
- Delay 或 async-like node。

目标：

- 验证 Exec 主干水平对齐。
- 验证列间距。
- 验证非分支线性链路连续排布。

### PureDataSubgraph / DataFlow

样例包含：

- 多个 Variable Get。
- 运算或比较节点。
- Pure function。
- MakeArray 或 MakeStruct。
- 一个消费节点的数据 input pin。

目标：

- 验证纯数据链。
- 验证同时有 data input 和 data output 的纯节点。
- 验证聚合节点与 leaves 的相对位置。

### NodeInputCluster

样例包含：

- 一个 Exec consumer。
- 多个 input pins。
- 每个 input pin 连接不同长度的 pure data chain。
- 至少一个 MakeArray / operator / pure function 形成 envelope。

目标：

- 验证 data cluster 优先级。
- 验证 cluster height budget。
- 验证 input pin row spacing。
- 验证 consumer 左侧空间占用。

### MultiExecOutput

样例包含：

- Event entry。
- Branch / Sequence / ForEachLoop 类多 Exec output 节点。
- 主出口、分支出口、Completed 出口。
- 每个出口后接不同长度 Exec 链。

目标：

- 验证任意多 Exec output 节点的 branch-like row。
- 验证每个 output downstream 第一个节点作为 row anchor。
- 验证分支 row 与主 row 间距。

### Occupancy

样例包含：

- 预置 blocker nodes。
- 待布局 Exec chain。
- 待布局 data cluster。
- fallback row candidate。

目标：

- 验证 collision padding。
- 验证 fallback row。
- 验证避让步进。
- 验证节点不会互相覆盖。

## 数据流

```text
User drag in Edit mode
  -> SemanticSceneModel updates scene role centers
  -> Adapter projects centers to RuleSet parameters
  -> RuleSet JSON saved

User clicks Preview
  -> Editor switches to Preview shell immediately
  -> PreviewService starts job
  -> Worker builds sample descriptor and layout plan from pure data
  -> GameThread materializes transient UEdGraph in frames
  -> SGraphEditor displays read-only preview graph
```

## 错误处理

RuleSet 无法解析：

- 不启动 worker。
- Preview shell 显示 RuleSet validation error。

Sample descriptor 构建失败：

- job 进入 `PreviewError`。
- 保留返回编辑入口。

Materialization 失败：

- 停止后续分帧。
- 清理 transient graph。
- 显示错误。

用户取消：

- job id 标记 cancelled。
- 后续 worker result 或 GameThread continuation 必须丢弃。

## 验证方向

本设计阶段不定义可执行计划，但后续实现应至少覆盖：

- RuleSet JSON roundtrip：`editor_canvas.scenes.<scene>.role_centers` 不丢失。
- Scene persistence：每个语义页拖拽后切页再切回仍保留位置。
- Adapter projection：scene centers 能稳定投影到 solver 参数。
- Arrow metadata：每个 scene edge 都有 from/to/direction。
- Tooltip metadata：每个 scene role node 都有中文 Tips。
- Preview job cancellation：返回编辑后旧 job 不再写 graph。
- Preview materialization：GameThread 分帧创建 transient graph。
- Real Editor E2E：拖拽、切页、预览、返回编辑、重新预览均不阻塞 UI，且 GraphPanel 显示原生节点。

## 后续执行文档需要确认的设计边界

- Preview materializer 的每帧预算是否直接复用 RuleSet apply budget，还是新增 preview-only budget。
- Sample graph descriptor 中 K2 node factory key 的最小集合。
- Preview shell 是否需要显示 progress，例如 `12/40 nodes materialized`。
