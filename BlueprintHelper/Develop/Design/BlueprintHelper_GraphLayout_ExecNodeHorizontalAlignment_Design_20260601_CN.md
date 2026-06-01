# GraphLayout Exec Node Horizontal Alignment Design

Date: 2026-06-01

Status: design only. This document intentionally does not contain an executable implementation plan.

## 背景

GraphLayout 目前已经是独立子系统，而不是 UI 层辅助逻辑。现有链路为：

- `FBlueprintHelperGraphLayoutCoordinator` 负责收集 generated nodes、构建 snapshot、异步调用 solver，并把 `FLayoutPlan` 应用回 graph。
- `FBlueprintHelperGraphLayoutRuleSourceResolver` 从 setting 解析 `GraphLayoutRules.json` 来源。
- `FRuleSet` / `FRuleSetJson` 是 runtime solver 的规则配置面。
- `FClassifier` 根据 class/title/exec pin 规则给节点分类。
- `FSolver::Solve` 根据 `FGraphSnapshot` 与 `FRuleSet` 计算 `FNodePlacement`。

当前 solver 的 exec 布局核心在 `LayoutExecChain()`。它用 node-level successor 列表遍历执行链，并以 `(Column, Row)` 计算目标位置。现有问题是 successor 只保留目标 node id，丢失了“来自哪个 Exec output pin”的信息；同时分支行偏移只在 `Role == BranchControl` 时生效，因此通用的多 Exec 出口节点无法稳定获得分支 row 语义。

## 已确认需求

1. 新增 GraphLayout setting，用于控制 Exec Node 的一键水平对齐行为，默认开启。
2. 行为目标是等价于编辑器中选中 Exec 节点后按 `Q` 的视觉结果：同一 exec row 内节点按统一水平基线对齐。
3. 不能模拟 UI 选择节点或调用编辑器快捷键；必须在 GraphLayout solver / rules 边界内确定性实现。
4. 每个具体 `ExecEntry` 是该执行流的统一锚点。不同事件入口、函数入口、CustomEvent 入口应各自独立成 row group。
5. 只要节点拥有多个 Exec output pin，就按分支类节点处理，不再只依赖 `Branch`、`Switch`、`Sequence` 等 class/title 枚举。
6. 分支类节点之后，每个 Exec output 的第一个 downstream Exec node 是该分支 row 的局部锚点。该 row 后续线性节点沿这个局部锚点对齐。
7. 数据节点、纯函数输入、注释、reroute/knot、Review/TaskSpec/GraphWrite 语义不属于本设计直接改动范围。

## 配置设计

配置的 runtime source of truth 应放在 GraphLayout RuleSet，而不是 UI local setting。

建议新增 RuleSet 字段：

```cpp
bool bAlignExecNodesHorizontally = true;
```

建议 JSON 字段名：

```json
"exec_node_horizontal_alignment_enabled": true
```

设计理由：

- GraphLayout runtime 当前实际消费的是 `FRuleSet`，由 `GraphLayoutRules.json` 或内建 `FRuleSet()` 默认值提供。
- `ui.layout_rule_editor.*` 主要服务 Layout Rule Editor 的默认 UI 展示，不应成为 solver runtime 的第二套配置来源。
- 默认开启应落在 `FRuleSet()` 默认值中，这样没有项目级 `GraphLayoutRules.json` 时也会启用。
- JSON import/export 必须 roundtrip 该字段，避免用户通过规则编辑器保存后丢失配置。

如果后续要在 Settings UI 中暴露该选项，应让 Settings UI 编辑 RuleSet JSON 或 RuleSet editor 默认值，而不是让 solver 同时读 `graph_layout.*` 与 RuleSet 两套开关。

## Pattern 化 Layout 模型

本设计把“水平对齐”定义为：同一 exec row 内的节点共享同一 Y 基线；X 仍由 exec column spacing 推进。它不是 equal spacing，也不是 UI 命令模拟。

这个能力不应继续扩展成单一全局 solver 参数，而应作为 Pattern-based GraphLayout 的第一批场景落地。不同 layout 场景的空间需求不同：

- 数据输入 cluster 主要消耗 height，也可能向左扩展 width。
- 多 Exec output 分支同时消耗 width 与 height。
- 线性 Exec 主干主要消耗 width。

因此 solver 的优先级不应是“Exec 主干先定最终坐标”，而应是：

```text
Exec topology skeleton -> Data Input Cluster height budget -> Branch Row allocation -> Linear Exec placement -> Occupancy apply
```

换句话说，Exec skeleton 先提供拓扑关系和候选 anchor；Data Input Cluster 先提供布局约束和 row height budget；Branch Row 根据这些预算分配 Y；Linear Exec 最后填充到已分配的 row baseline 上。

### Pattern Policy 分类

推荐把 GraphLayout 拆成少量语义 layout policy，而不是每种 UE node 写一个特殊分支：

- `LinearExecChainPolicy`：处理 `ExecEntry -> Node -> Node` 这类线性执行链，负责 column 推进和同 row 对齐。
- `PureDataSubgraphPolicy`：处理没有 exec pin、但拥有纯 data input 与纯 data output 的 data-only 子图，例如 `Make Array`、`Make Struct`、operator、select、pure function。
- `NodeInputClusterPolicy`：处理一个 exec consumer 或 data transform consumer 周围的数据输入 cluster，消费 `PureDataSubgraphPolicy` 给出的 envelope，并把 cluster 锚定到目标 data input pin 附近。
- `MultiExecOutputPolicy`：处理任意多 Exec output 节点，负责为每个 output downstream 分配 branch row anchor。
- `OccupancyPolicy`：最后统一处理 existing node、padding、collision fallback；它不决定语义，只修正可用坐标。

policy registry 的分发依据应来自 topology segment 和 node role，而不是 UI 组件或 TaskSpec 特殊字段。

### ExecEntry Root

Exec row group 的 root 来自现有 root 发现逻辑：

- `EventEntry`
- 或没有 exec input 的 exec-role 节点
- 或 detached exec node fallback

每个 root 分配独立 topology group。root 在早期 pass 中只提供 row group 和执行顺序锚点，不应过早决定最终 Y。最终 Y 应在数据输入 cluster 与分支 row 空间预算完成后再落位；如果 root 经过 occupancy resolver 后被移动，后续节点仍应使用 root 的 resolved row。

### Pure Data Subgraph 与 Data Input Cluster 预算

数据输入 cluster 应在分支 row 与线性 exec 最终坐标之前完成测量。测量目标不是立即移动节点，而是为每个 consumer 生成空间 envelope：

- 输入节点数量、role 与 pin order。
- Pure / Operation / Variable 输入的相对排列方向。
- cluster 需要的最小 width / height。
- cluster 与 consumer 的最小 padding。
- cluster 是否会向上、向下或左侧扩展。

这里不能只把直接连接到 exec node input pin 的节点当作 flat list。真实 Blueprint 中常见的 data-only 中间节点同时拥有纯 data input 与纯 data output，例如：

```text
VariableGet leaves -> Make Array transform/root -> ForEach.Array sink
```

这类节点应按 pure-data DAG 建模：

- `DataLeaf`：只有纯 data output，没有纯 data input，例如 variable get、literal、self。
- `DataTransform` / `DataAggregate`：同时拥有纯 data input 与纯 data output，且没有 exec pin，例如 `Make Array`、`Make Struct`、operator、select、pure function。
- `DataSink`：消费 data 的目标 pin，可以是 exec node 的 input pin，也可以是另一个 pure data transform 的 input pin。

布局时应从 sink 反向构建 data DAG。以 `ForEach.Array` 为例，`Make Array` 是靠近 sink 的 transform root，多个 variable get 是 leaves。`Make Array` 应靠近 `ForEach` 的 `Array` input pin，variable get 按 `Make Array` 的 input pin order 竖向排列在它左侧。

因此 row height budget 应按整个 pure-data subgraph 的包围盒计算，而不是只按直接输入节点数量计算。

每个 exec row 的高度预算应至少包含该 row 上所有 consumer 的 cluster envelope。也就是说，row spacing 不应只等于 `ExecRowSpacing` 或 `BranchRowSpacing`，而应是：

```text
row_height = max(setting_min_row_spacing, max(node_input_cluster_height + row_padding))
```

这样 Pure / Operation 输入较多的节点不会被后续 exec row 或 branch row 压住。

### Branch Split 判定

分支 split 不应只由 `ENodeRole::BranchControl` 决定。布局时应使用拓扑判定：

- 节点属于 exec layout 范围。
- 节点拥有多个 Exec output pin。
- 至少有 downstream exec successor 可参与布局。

`ENodeRole::BranchControl` 仍可作为分类、颜色或 diagnostic 信号，但 solver 的分支 row 语义必须能覆盖任意多 Exec output 节点。

### Exec Edge 信息

现有 `GetExecSuccessors()` 只返回 `TArray<FString>`，并且 `AddUnique` 会抹掉 output pin identity。新模型需要保留 edge metadata：

```text
SourceNodeId
SourceOutputPinId / SourceOutputPinName
SourceOutputOrdinal
TargetNodeId
TargetOrdinalWithinOutput
```

排序应稳定：

1. 按 source node pin 顺序。
2. 同一个 output pin 下按 linked node 顺序。
3. 对重复 target 保持第一次可达路径优先，后续重复 visit 只作为 issue/debug 信号，不重复布局。

### 分支 Row 锚点

普通线性 exec successor 继承当前 row 的 baseline，但该 baseline 应来自完成数据输入 cluster 测量后的 row allocation。

多 Exec output 节点的 downstream 布局规则：

- 每个 Exec output pin 对应一个 branch row slot。
- 该 output pin 的第一个 downstream exec node 是 row anchor。
- row anchor 放在 parent 的下一列，Y 来自 Branch Row allocation。该 allocation 必须先考虑父节点 row、各 output row 的 data input cluster height budget、以及最小 branch row spacing。
- row anchor 如果因为 collision 被推到其他 Y，后续该分支 row 的线性节点继承 anchor resolved row。
- 如果某个 output pin 没有 downstream exec node，不创建 row anchor，也不占用可见 row。

这保证 `Branch`、`Switch`、`Sequence`、loop/gate/multigate 以及任何拥有多个 Exec output 的节点都走同一 row-anchor 模型。

## 与 Occupancy / Existing Node 的关系

GraphLayout 已经通过 `FOccupancyResolver` 避免目标矩形冲突。本设计不绕过它。

关键规则：

- row anchor 的 desired Y 只表达语义 row；最终 TargetPosition 仍由 occupancy resolver 决定。
- downstream 节点必须基于 anchor 的 resolved row 继续布局，避免父节点或分支 anchor 被避让后，后续节点仍停留在旧 row。
- 当 `bMoveExistingNodes == false` 时，existing exec 节点保持当前坐标，但它仍可以作为 row anchor；generated downstream 节点应跟随该 existing anchor 的当前 Y。
- 数据输入节点不应只作为 exec row placement 之后的附属步骤。它们应先通过 `NodeInputClusterPolicy` 测量空间预算，再由 `FDataInputPlacement` 基于最终 consumer target 生成实际 placement。
- Occupancy 不能反向改变语义 row 分配规则；如果碰撞导致某个 anchor 下移，只能把同 row 后续节点带到 resolved row，而不能重算成另一条拓扑链。

## 与 Classifier 的关系

推荐不要把“多 Exec output 即分支”只写成 class/title rule。

原因：

- class/title rule 无法覆盖插件节点、自定义 K2 节点和未来 UE 节点。
- setting 关闭时，solver 应能回到旧布局行为；如果 classifier 永久把多 Exec output 节点改成 `BranchControl`，会让关闭开关的语义不清晰。
- GraphLayout 的 row 语义本质是 exec topology，不是 UI role color。

建议边界：

- `FClassifier` 可继续负责 role、颜色、diagnostic 分类。
- `FSolver` 内新增或复用一个 exec topology helper，根据 snapshot pins 判断 branch split。
- 如需要在 `FLayoutPlan.Classifications` 中暴露该信息，应通过 reason/issue 记录，不能让 role enum 成为唯一 source of truth。

## 非目标

本设计不处理：

- 不调用编辑器 `Q` 命令，不模拟 selection。
- 不改 TaskSpec schema。
- 不改 GraphWrite 生成节点或连接的语义。
- 不改 Review v2 数据模型、DebugBundle、Accept/Reject。
- 不重新设计 `GApplyQueue`、per-graph epoch、save timing 等 GraphLayout 已知 lifecycle 风险。
- 不把 `ui.layout_rule_editor.*` 扩展成 solver 的第二套 runtime setting。

## 验证策略

后续实现必须先写失败测试，再改 production code。

需要覆盖的设计级测试包括：

- `FRuleSet` 默认值开启 `bAlignExecNodesHorizontally`。
- `FRuleSetJson` import/export roundtrip `exec_node_horizontal_alignment_enabled`。
- `NodeInputClusterPolicy` 能根据一个 exec consumer 的 Pure / Operation / Variable 输入测量 cluster height budget。
- `PureDataSubgraphPolicy` 能识别同时拥有纯 data input 与纯 data output、且没有 exec pin 的中间节点，并把它作为 data transform / aggregate 纳入 envelope。
- `Make Array` 类场景中，variable get leaves 应按 `Make Array` input pin order 排在左侧，`Make Array` 应靠近 downstream sink pin，例如 `ForEach.Array`。
- row allocation 使用 `max(setting_min_row_spacing, node_input_cluster_height + padding)`，而不是只使用固定 exec / branch spacing。
- 一个普通 `K2Node_CallFunction` 拥有两个 Exec output pin 时，即使没有命中 `BranchControl` class/title，也按 branch row spacing 分配两个 downstream row anchors。
- setting 关闭时，不启用新的 exec row anchor 行为。
- 分支 row anchor 因 collision 下移后，后续线性节点继承 resolved row。
- `bMoveExistingNodes == false` 时，existing 分支后第一个 downstream exec node 可作为 current-position row anchor，generated 后续节点跟随它。

真实 E2E 验证应在自动化测试之后进行，目标是生成一个包含事件入口、普通 exec 链、多 Exec output 节点、分支后续节点、`Make Array` / pure function / operator data transform、以及多层数据输入 cluster 的 Blueprint graph，然后检查实际节点坐标同时满足：

- 数据输入 cluster 不与相邻 exec row / branch row 重叠。
- pure data transform 位于 data leaves 与 downstream sink 之间，而不是被当作普通 leaf input。
- 多 Exec output 下游节点使用各自 row anchor。
- 线性 Exec 主干按已分配 row baseline 水平对齐。
- existing node pinning 与 occupancy fallback 不破坏上述语义。

## 风险

1. 当前 `GetExecSuccessors()` 丢失 output pin identity，不能直接支持“每个 output 的第一个 downstream exec node”。
2. 当前 `LayoutExecChain()` 用 queue + visited 的简单 BFS；引入 edge metadata 后必须保持 deterministic order，避免同图多次布局结果不稳定。
3. 多 Exec output 节点可能包括 latent、async、macro-expanded 或 plugin K2 节点；pin topology 判定比 class/title 更稳，但测试必须覆盖 generic class。
4. 数据输入 cluster 的 envelope 如果测量不足，会直接污染 branch row 与 exec mainline 的 Y 分配。
5. pure data subgraph 可能形成共享节点或 diamond data dependency；第一阶段应保持 deterministic first-owner 策略，并把共享情况记录为 layout issue，避免重复移动同一节点。
6. Occupancy resolver 可能改变 anchor Y；如果 downstream 仍使用 desired row，会重新出现错行。
7. RuleSet JSON 与 Layout Rule Editor UI defaults 目前存在职责差异；实现时必须避免创建第二套 runtime 默认值。
8. policy 数量过多会让布局规则互相打架，因此第一阶段应只拆 `LinearExecChainPolicy`、`PureDataSubgraphPolicy`、`NodeInputClusterPolicy`、`MultiExecOutputPolicy`、`OccupancyPolicy` 五类，不做 per-node special policy。

## 设计结论

该能力应作为 GraphLayout RuleSet 驱动的 Pattern-based solver 行为实现：

- 默认开启。
- 以 ExecEntry 作为 topology root anchor，而不是最终 Y 的唯一来源。
- 以 Pure Data Subgraph / Data Input Cluster 的 envelope 作为 row height budget 的优先约束。
- 先做 Branch Row allocation，再做 Linear Exec placement。
- 以多 Exec output pin 作为通用分支 split 判定。
- 以每个分支 output 的第一个 downstream exec node 作为 row-local anchor。
- 保持 occupancy、existing node pinning、data input placement 的既有职责边界，但把 data input placement 前置为可测量的 pure-data subgraph / cluster policy。
- 不触碰 UI 快捷键、TaskSpec、GraphWrite、Review。

在该设计下，新增行为符合 GraphLayout 现有 subsystem 边界，也符合“新增能力优先扩展已有 resolver/solver/rules 边界，不为单个场景硬编码特殊分支”的架构要求。
