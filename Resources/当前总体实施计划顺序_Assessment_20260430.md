# BlueprintHelper 设计文档实施优先级评估与排序

- 文档编号：`Module_BlueprintHelper_ImplementationPriority_Assessment_20260430`
- 适用版本：BlueprintHelper v0.3.0r / UE 5.3+
- 日期：2026-04-30
- 建议路径：`BlueprintHelper/Resources/Plan/Module_BlueprintHelper_ImplementationPriority_Assessment_20260430.md`
- 评估对象：
  - `风险清单_PLevels_20260430.md`
  - `MCP返回结构优化_Design_20260430.md`
  - `Module_BlueprintHelper_MCP_ReturnProtocol_Optimization_20260428.md`
  - `Agent导入Json_Design_20260430.md`
  - `改动预览_FeasibilityRiskBenefit_20260428.md`

---

## 1. 结论

当前 BlueprintHelper 已具备 Agent 通过 MCP 访问和编辑 Unreal Editor 资产的基础能力，但下一阶段不应优先继续扩展更多写工具。

推荐实施原则是：

```text
先消除误写 / 越权 / 半成功风险
再优化 Agent 读取与导入协议
最后建立审阅、回滚和复杂项目能力
```

最终排序如下：

| 优先级 | 模块 / 事项 | 建议阶段 | 结论 |
|---:|---|---|---|
| 1 | P0 隐患修复 | 立即 | 阻断级，必须先做 |
| 2 | 请求校验与写操作安全底座 | 立即 | 所有后续写能力的基础 |
| 3 | P1 写入可靠性修复 | 最近版本 | 降低 Agent 编辑失败率和误写风险 |
| 4 | Bridge / MCP 返回协议优化 | P0 后并行 | 读路径收益高，风险相对低 |
| 5 | MCP 结构化返回承载优化 | 与返回协议合并 | 解决 JSON 二次编码和 token 浪费 |
| 6 | AgentImportGraph v1 | 写入底座完成后 | 降低 Agent 生成 JSON 难度 |
| 7 | Change Review v1 | 事务 / 快照能力后 | 建立用户审阅和回滚闭环 |
| 8 | Patch / Ops 增量修改协议 | Review v1 后 | 需要稳定节点匹配和冲突检测 |
| 9 | P2 复杂项目能力 | 中期 | 分页、job、cancel、资产搜索过滤 |
| 10 | P3 发布与治理 | 持续 | 版本、打包、CI、文档治理 |

---

## 2. 排序依据

本次排序采用四个判断标准：

| 标准 | 权重 | 说明 |
|---|---:|---|
| 是否可能导致错误写入或资产损坏 | 最高 | 直接影响用户项目安全 |
| 是否会导致 Agent 误判成功 | 高 | 会引发后续连锁错误 |
| 是否是其他设计的前置依赖 | 高 | 先做基础设施，后做上层功能 |
| 实施收益 / 风险比 | 中 | 优先做收益高且风险可控的项目 |

因此，安全性和写入确定性高于 token 优化；写入底座高于审阅 UI；读路径优化可以较早实施，因为其风险低于写路径。

---

## 3. 第一优先级：P0 隐患修复

### 3.1 必须立即处理的问题

P0 隐患属于阻断级问题，建议在继续增加高危写工具前全部完成。

| ID | 问题 | 风险 | 建议动作 |
|---|---|---|---|
| P0-01 | `blueprint_export_to_json` scope 与 Bridge 实际识别值不一致 | Agent 读取范围错误 | 统一 scope 枚举，并返回 `effective_scope` |
| P0-02 | 显式 graph 名错误时可能回退 EventGraph | 写错图表 | 显式传 graph 时找不到必须 hard fail |
| P0-03 | Bridge 无认证且暴露高危命令 | 本地越权调用 | 增加 session token，高危命令默认禁用 |
| P0-04 | Bridge 请求解析缺少统一 schema 校验 | 错误请求仍可能执行 | 新增统一请求校验器 |
| P0-05 | Validate 与 Import 的 graph 作用域不一致 | 校验通过但导入失败 | node / link 改为 graph-scoped |
| P0-06 | Import 成功语义不严谨 | Agent 误判成功 | 区分 full success / partial / no-op / failed |

### 3.2 推荐实现顺序

```text
FBlueprintHelperRequestValidator
-> graph hard fail
-> strict import result
-> graph-scoped validate/import
-> export scope 对齐
-> Bridge token / 高危命令禁用
```

原因：

1. 请求校验是最底层入口，应最先做。
2. graph fallback 是最危险的误写风险，应尽快移除。
3. strict import 可以阻止半成功继续扩散。
4. Bridge token 和高危命令禁用收敛本地安全边界。

### 3.3 验收标准

- `target_graph="EventGrph"` 不会写入 `EventGraph`。
- 缺字段、`null`、类型错误请求不会进入业务逻辑。
- 导入 link 失败时默认回滚已创建节点。
- 返回结果明确区分：
  - `full_success`
  - `partial_success`
  - `no_op`
  - `failed`
- 未携带 token 的高危请求返回 `unauthorized` 或 `command_disabled`。

---

## 4. 第二优先级：统一写操作安全底座与 P1 修复

### 4.1 核心目标

建立统一写操作包装层：

```text
FBlueprintHelperScopedAssetMutation
```

建议职责：

```text
Validate
-> Begin Transaction
-> Modify Asset
-> Execute Mutation
-> Compile / Refresh
-> Collect Diagnostics
-> Commit or Rollback
-> MarkPackageDirty
```

### 4.2 应优先纳入统一底座的模块

| 模块 | 当前风险 | 处理目标 |
|---|---|---|
| Blueprint 节点 | title 匹配、Node_i、不稳定 reconstruct | 改用稳定 GUID 和明确节点生命周期 |
| 函数节点 | 同名函数 / owner 不明确 | 增加 owner class、target class、signature |
| Pin 默认值 | 可能被 reconstruct 覆盖 | 调整默认值应用顺序 |
| UMG | Add / Remove / Move 事务不完整 | 每次 Agent 写入可一次 Undo |
| DataTable | 多字段 update 非原子 | 副本校验成功后整体替换 |
| UObject | SetProperty 权限过宽 | 只允许编辑器中可编辑属性 |
| DeleteNodes | 部分失败不严格 | strict 下任意失败整体回滚 |

### 4.3 推荐实现顺序

```text
稳定节点引用 node_guid
-> 函数解析增强
-> Pin 默认值流程修正
-> PinType 失败 hard fail
-> DeleteNodes strict 化
-> UMG 事务化
-> DataTable 原子更新
-> UObject 属性写权限校验
```

### 4.4 验收标准

- 写操作不接受 title 模糊匹配作为最终目标。
- 同名函数候选超过一个时 hard fail。
- 默认值写入失败返回结构化诊断。
- PinType 转换失败不会 fallback 到默认类型。
- DataTable 更新一行多个字段时不存在半更新状态。
- UMG Move 失败后能恢复 old parent、old slot、layout data。
- 一次 Agent 写入对应一次可理解的 Undo / Review 记录。

---

## 5. 第三优先级：Bridge / MCP 返回协议优化

### 5.1 优先级判断

该项应在 P0 后尽快实施，可以与部分 P1 并行。

原因：

1. 主要影响读路径，破坏资产的风险较低。
2. 可以显著降低 Agent 上下文占用。
3. 可以明确 raw JSON、logic JSON、logic MD 的边界。
4. 是后续 AgentImportGraph 和 Change Review 的信息基础。

### 5.2 推荐新增 Bridge 命令

```text
export_logic
```

请求核心字段：

```json
{
  "target_blueprint": "/Game/BP/BP_Player.BP_Player",
  "target_graph": "EventGraph",
  "scope": "single_graph",
  "format": "logic_md | logic_json",
  "detail": "brief | normal | full",
  "include_data_dependencies": true,
  "include_orphans": true,
  "include_node_ids": false,
  "include_positions": false
}
```

### 5.3 MCP 工具建议

| MCP Tool | Bridge Command | 默认格式 | 用途 |
|---|---|---|---|
| `blueprint_get_logic` | `export_logic` | `logic_md` | 快速理解蓝图 |
| `blueprint_get_logic_json` | `export_logic` | `logic_json` | 精确分析 |
| `blueprint_export_raw_json` | `export_to_json` | `raw_json` | 调试、兼容、回放 |

### 5.4 验收标准

- 旧 `export_to_json` 仍返回兼容的 raw JSON。
- `export_logic format=logic_md` 返回 Markdown 摘要。
- `export_logic format=logic_json` 返回结构化逻辑对象。
- `logic_json` 明确 `importable=false`。
- Agent 默认读图不再拉取完整 RawJson。

---

## 6. 第四优先级：MCP 结构化返回承载优化

### 6.1 推荐承载矩阵

| 数据类型 | MCP 承载方式 | 默认 inline | 说明 |
|---|---|---:|---|
| `LogicMd` | `content[].text` | 是 | 给 Agent / 用户快速阅读 |
| `LogicJson` | `structuredContent.logic` | 是 | 给 Agent 精确分析 |
| `RawJson` | `resource_link` / `resources/read` | 否 | 调试、兼容、回放 |
| legacy JSON | `content[].text = JSON.stringify(...)` | 否 | 仅兼容旧客户端 |

### 6.2 推荐模式

```text
summary_text
structured_json
resource_ref
legacy_text_json
```

### 6.3 实施要点

- `LogicJson` 不再作为 stringified JSON 塞进 `content.text`。
- `RawJson` 默认只返回 URI、统计和摘要。
- 旧客户端通过 `responseMode="legacy_text_json"` 或环境变量回退。
- 工具结果包含：
  - `format`
  - `schema`
  - `assetPath`
  - `graph`
  - `stats`
  - `diagnostics`

### 6.4 验收标准

- `content.text` 不再出现大段 `\"format\":\"logic_json\"`。
- `blueprint_get_logic_json` 主数据位于 `structuredContent.logic`。
- `blueprint_export_raw_json` 默认返回 `resource_link`。
- legacy 模式仍能兼容旧调用方。

---

## 7. 第五优先级：AgentImportGraph v1

### 7.1 优先级判断

AgentImportGraph 收益高，但属于写入入口，不能早于 P0/P1 基础完成。

其核心目标是让 Agent 表达蓝图构图意图，而不是生成 UE 节点快照。

### 7.2 第一版范围

只建议实现：

```text
schema: BlueprintHelper.AgentImportGraph
version: 1.0
mode: append
layout: auto / append_right
semantic nodes
exec links
data links
compile: true
save: false
```

暂不建议实现：

```text
patch
复杂 diff merge
完整 dry-run
跨 graph link
依赖当前激活图表的隐式写入
```

### 7.3 推荐最小结构

```json
{
  "schema": "BlueprintHelper.AgentImportGraph",
  "version": "1.0",
  "target_blueprint": "/Game/BP/BP_Player.BP_Player",
  "target_graph": "EventGraph",
  "mode": "append",
  "layout": "auto",
  "nodes": [],
  "links": [],
  "options": {
    "compile": true,
    "save": false
  }
}
```

### 7.4 节点设计原则

Agent 只写语义节点：

```json
{
  "id": "print_hello",
  "kind": "call",
  "function": "/Script/Engine.KismetSystemLibrary:PrintString",
  "inputs": {
    "InString": "Hello from Agent"
  }
}
```

插件负责：

```text
节点实例化
Pin 解析
默认值补全
类型检查
自动布局
编译诊断
```

### 7.5 验收标准

- Agent JSON 不要求 Pos、GUID、完整 Pin 列表。
- `append` 模式可生成 BeginPlay -> PrintString。
- `layout=auto` 能产生可读布局。
- 默认 `save=false`。
- 任意节点、Pin、默认值错误均返回可修复的结构化诊断。
- strict 模式下错误不会留下半生成节点。

---

## 8. 第六优先级：Change Review v1

### 8.1 优先级判断

Change Review 的产品收益很高，应高于继续增加更多写工具，但必须建立在事务、快照和回滚能力之后。

第一版不建议做完整“写入前 dry-run 预览”，而应采用：

```text
Applied Pending Review
```

即：

```text
Agent 写操作
-> 创建审阅会话
-> 记录修改前快照
-> 执行修改
-> 记录修改后快照
-> 面板展示差异
-> 用户 Approve / Reject
```

### 8.2 第一版应包含

| 能力 | 必要性 |
|---|---:|
| 创建审阅会话 | 必须 |
| 写操作自动绑定审阅会话 | 必须 |
| 修改前 / 后资产快照 | 必须 |
| Operation Diff | 必须 |
| Logic Diff | 建议 |
| Open UE Asset Diff | 建议 |
| Approve / Reject | 必须 |
| Reject 回滚或提示手动处理 | 必须 |
| 审阅状态持久化到 Saved | 必须 |
| 自动保存 | 默认不做 |

### 8.3 推荐 Bridge 命令

```text
review_begin_session
review_get_session
review_list_sessions
review_approve_session
review_reject_session
review_open_asset_diff
review_export_summary
```

### 8.4 推荐 MCP 工具

```text
blueprint_review_begin_session
blueprint_review_list_pending
blueprint_review_get_summary
blueprint_review_approve
blueprint_review_reject
blueprint_review_open_diff
```

### 8.5 默认策略

```json
{
  "review_policy": "pending"
}
```

显式绕过审阅才允许：

```json
{
  "review_policy": "bypass"
}
```

自动保存必须显式：

```json
{
  "review_policy": "auto_approve_and_save"
}
```

### 8.6 验收标准

- Agent 添加变量后，面板出现 pending review session。
- 面板显示目标资产、操作类型、变量名和执行状态。
- Reject 后变量恢复到修改前状态。
- 编译失败时 session 标红并显示错误摘要。
- 用户手工修改同一资产后 session 标记 `mixed_changes`，不允许无提示自动回滚。
- Approve & Save 后资产保存，session 状态变为 `approved`。

---

## 9. 第七优先级：Patch / Ops 增量修改协议

### 9.1 为什么不应过早实现

Patch / Ops 很适合 Agent 多轮编辑，但它依赖：

```text
稳定 node id / node_guid
稳定 graph scope
准确 diff
冲突检测
审阅和回滚
```

如果没有这些基础，Patch 会比 append 更危险。

### 9.2 建议进入条件

满足以下条件后再启动：

- AgentImportGraph v1 已稳定。
- Change Review v1 可记录和回滚。
- LogicJson 能稳定表达节点、Pin、变量和依赖。
- 写操作已统一 strict / transaction。
- 节点引用不再依赖 title 或 Node_i。

---

## 10. 第八优先级：P2 复杂项目能力

### 10.1 推荐范围

| 能力 | 作用 |
|---|---|
| Bridge request queue | 避免长命令阻塞后续请求 |
| job id / status query | 支持长任务查询 |
| timeout / cancel 语义 | 避免 Node 超时但 UE 继续写入 |
| 大对象分页 | 支持复杂蓝图、大 WidgetTree、大 DataTable |
| TCP 连接复用 | 降低日志噪声和连接开销 |
| 结构化资产搜索 | 降低大项目选错资产概率 |
| active context 降级 | 避免多编辑器窗口下写错目标 |

### 10.2 推荐顺序

```text
job id / status query
-> request queue / busy 状态
-> timeout abandoned 记录
-> 大对象分页
-> SearchAssets 结构化过滤
-> active context 只读化
-> TCP 连接复用
```

---

## 11. 第九优先级：P3 发布与治理

### 11.1 建议持续处理

| 项目 | 建议 |
|---|---|
| 版本号一致性 | 统一 `.uplugin`、MCPServer package、文档版本 |
| Resources 递归打包 | `Resources/...` 必须进入发布包 |
| 发布包拆分 | source-only / prebuilt / MCPServer 分包 |
| Build.cs 依赖收敛 | Editor 依赖尽量放 PrivateDependency |
| 自动化测试矩阵 | C++ validator、UE Automation、MCP 集成测试 |

### 11.2 最低测试矩阵

```text
缺字段 / null / 错类型
错误 graph hard fail
重复 node id
跨 graph link
pin alias 失败
默认值写入失败
简单图导入
函数图 / 宏图导入
添加变量 / dispatcher
UMG Add / Move / Remove
DataTable update rollback
export_logic
structuredContent
resource_link
review approve / reject
```

---

## 12. 推荐版本路线

```text
v0.3.1
  - P0 修复
  - 请求校验
  - strict import
  - graph hard fail
  - Bridge token
  - 高危命令默认禁用

v0.3.2
  - P1 写入可靠性
  - 统一事务层
  - node_guid 写操作
  - 函数解析增强
  - Pin 默认值流程修正
  - UMG / DataTable / UObject 原子化

v0.3.3
  - export_logic
  - blueprint_get_logic
  - blueprint_get_logic_json
  - blueprint_export_raw_json
  - LogicMd / LogicJson / RawJson 职责拆分

v0.3.4
  - MCP structuredContent
  - RawJson resource_link
  - legacy_text_json 兼容模式
  - stats / diagnostics / schema 元信息

v0.3.5
  - AgentImportGraph v1
  - append 模式
  - auto / append_right layout
  - 语义节点导入
  - 结构化导入错误

v0.3.6
  - Change Review v1
  - Applied Pending Review
  - Operation Diff
  - Logic Diff 初版
  - Approve / Reject
  - Saved session persistence

v0.4.0
  - Patch / Ops
  - Review 深度集成
  - P2 大项目能力
  - 自动化测试矩阵补全
```

---

## 13. 不建议的实施顺序

以下顺序不建议：

### 13.1 先做 Change Review，再修事务

风险：

```text
Review 能看到变化，但 Reject 不可靠
```

如果事务、快照、回滚能力不足，审阅功能会给用户一种“可安全回滚”的错觉。

### 13.2 先做 AgentImportGraph，再修 strict import

风险：

```text
Agent 更容易写入，但失败语义仍不可靠
```

这会扩大半成功、误判成功和错误 graph 写入的影响范围。

### 13.3 先做 Patch，再做 node_guid / review

风险：

```text
Patch 目标不稳定，容易改错已有节点
```

Patch 必须依赖稳定节点引用、冲突检测和审阅能力。

### 13.4 继续增加更多写工具

风险：

```text
工具数量增加会放大当前 P0 / P1 风险
```

在 P0/P1 完成前，新增写工具不是能力扩展，而是风险扩散。

---

## 14. 最终建议

建议采用如下实施策略：

```text
P0 / P1 安全底座
-> 读路径协议优化
-> MCP 结构化承载
-> AgentImportGraph v1
-> Change Review v1
-> Patch / Ops
-> P2 / P3 工程治理
```

一句话总结：

```text
BlueprintHelper 下一阶段的核心不是“让 Agent 能做更多事”，而是“让 Agent 做事时目标明确、失败可诊断、修改可回滚、结果可审阅”。
```

