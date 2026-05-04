# BlueprintHelper TaskPlan GraphWrite 能力侧公共协议

日期：2026-05-04  
范围：TaskSpec / TaskPlan / GraphWrite capability / 公共字段 / 公共禁止项  
状态：架构变更后同步稿  
说明：本文件只记录跨工具簇公共规则；具体 capability 参数见各工具簇文档。

架构同步说明：本目录原名 Agent→MCP by cluster。TaskSpec 架构确认后，普通 Agent 不再直接选择这些底层 GraphWrite 命令；Agent 面向任务级 MCP 工具提交 `BlueprintHelper.TaskSpec.v1`，Python / MCP Task Compiler 将其编译为 `BlueprintHelper.TaskPlan.v1`，UE Task Runtime 再按 TaskPlan step 调用这些既有 capability。本目录字段保留为 TaskPlan step args、debug / expert 工具和测试入口资料。

---

## 1. 分层模型

普通 Agent-facing MCP Tool 输入收敛到任务级工具，不直接暴露本目录的 typed arguments。  
本目录描述 Task Compiler / UE Task Runtime 对既有 capability 的结构化调用字段，并通过公共 mixin 复用字段。

不采用：

```ts
{
  schema: "BlueprintHelper.Command.v1",
  operation: "replace_blueprint_graph",
  request_id: "...",
  trace_id: "...",
  payload: { ... }
}
```

旧直调工具采用：

```ts
replace_blueprint_graph({
  asset_path: "/Game/BP/BP_Door",
  target: { target_type: "function", target_name: "OpenDoor" },
  replace_scope: "function_body",
  body: { schema: "BlueprintLogicSpec.v1", statements: [] },
  dry_run_mode: "none"
})
```

分层固定为：

```text
Agent → MCP Task Tool：
提交 TaskSpec，必要时先 read_task_context / preview_task。

Python / MCP Task Compiler → TaskPlan：
将 TaskSpec 编译为 TaskPlan steps，并选择 GraphWrite capability。

UE Task Runtime → Existing Capability Clusters：
使用本目录 typed args + schema mixins 调用 Append / Replace / Patch / Merge 等能力。

Existing Capability Clusters → Bridge / UE：
内部 Bridge request / response 作为事实来源。

MCP Task Tool → Agent：
返回 BlueprintHelper.McpToolResult.v1 / Task Error，不默认暴露底层 operation 明细。
```

---

## 2. 写工具公共输入基类

所有 TaskPlan 写入 step / expert 写工具输入必须显式包含：

```ts
interface WriteToolArgsBase {
  asset_path: string
  dry_run_mode: "none" | "quick" | "full"
}
```

### 2.1 asset_path

所有 UE 写工具必须显式传入：

```ts
asset_path: string
```

禁止依赖：

```text
当前打开的 Blueprint
当前选中的资产
当前 Content Browser 选择
当前编辑器焦点图表
```

新版命令侧统一使用 `asset_path`，不作为主协议字段使用：

```text
target_blueprint
target_asset
blueprint_path
blueprint
asset
```

旧 Legacy 工具可短期兼容旧字段，但 Agent Guide / TaskSpec / TaskPlan 主线只使用 `asset_path`。

### 2.2 dry_run_mode

`dry_run` 与旧 `dry_run_mode` 组合已收敛为单字段：

```ts
dry_run_mode: "none" | "quick" | "full"
```

语义：

```text
dry_run_mode="none"：
正式写入请求。工具正式写入前仍必须执行内部 preflight。

dry_run_mode="quick"：
非写入 quick dry_run。返回 status=dry_run / modified=false / data.dry_run。

dry_run_mode="full"：
非写入 full dry_run。返回 status=dry_run / modified=false / data.dry_run。
```

不再使用：

```ts
dry_run?: boolean
dry_run_mode?: "quick" | "full"
```

`dry_run_mode` 对所有写工具必填，不允许省略。

---

## 3. dry_run 与 preflight 的关系

```text
dry_run 是 TaskPlan capability 级非写入预演模式。
preflight 是写工具内部写入前检查阶段。
```

规则：

```text
1. dry_run_mode="quick" 或 "full" 时，工具不写资产。
2. dry_run_mode="none" 时，工具进入正式写入路径。
3. 正式写入前仍必须执行内部 preflight。
4. dry_run passed 不允许正式写入跳过 preflight。
5. 高风险写入可要求 full dry_run；正式写入前也要 full preflight。
6. 第一版不引入 dry_run_proof / preflight_proof。
7. 当前不考虑并发 / 乐观锁 / 资产状态 hash。
```

---

## 4. 禁止安全覆盖字段

TaskPlan 写入 step / expert 写工具输入中禁止出现任何临时安全提权或覆盖字段：

```text
safety_profile
profile
temporary_profile
per_call_profile
one-shot Expert
permission_override
force_write
no_review
no_journal
allow_user_nodes
force_user_nodes
override_ownership_policy
```

规则：

```text
1. Safety Profile 只来自 runtime_profile.active_profile。
2. Agent 不能通过工具参数临时提权。
3. 是否允许影响用户节点，由 runtime profile / Safety Profile / 用户明确目标 / full dry_run 或 full preflight 共同决定。
4. 工具参数与当前 SetupProfile 冲突时，返回 ProfilePolicyViolation 或 profile_policy_violation。
```

---

## 5. 第二层 schema 短命名规则

所有第二层 schema 使用短命名：

```text
xxxx.vx
```

使用：

```ts
schema: "BlueprintLogicSpec.v1"
```

不使用：

```ts
schema: "BlueprintHelper.BlueprintLogicSpec.v1"
```

示例：

```text
BlueprintLogicSpec.v1
AppendBlueprintGraph.v1
AppendBlueprintGraphDryRun.v1
ReplaceBlueprintGraph.v1
ReplaceBlueprintGraphDryRun.v1
AddBlueprintMemberVariable.v1
ReadBlueprintMemberVariables.v1
SetBlueprintMemberDefault.v1
```

顶层 ToolResultBase 可以继续使用全局协议名：

```json
{
  "schema": "BlueprintHelper.McpToolResult.v1"
}
```

---

## 6. 调试字段原则

普通命令输入不包含：

```text
request_id
trace_id
transaction_id
operation
command
payload
auth_token
reason
user_intent
description
note
layout_hint
preserve_comments
```

例外：

```text
block_id 可以作为定位已有 owned block 的 target 字段。
transaction_id 可以作为 rollback_transaction 的目标字段。
```

`verbose?: boolean` 可以由复杂工具按需支持，但它只能影响返回细节，不得改变执行行为、权限、Safety Profile 或 dry_run/preflight 规则。

---

## 7. 图表级目标字段

图表级写工具输入使用：

```ts
target_graph: string
```

返回侧可使用：

```json
"target": {
  "asset_path": "/Game/BP/BP_Door",
  "graph": "EG_PhysicsDoor"
}
```

---

## 8. ID / 引用层级

| 名称 | 层级 | 含义 | 是否等于 UE 节点 |
|---|---|---|---|
| request_id | MCP/Bridge 通信层 | 一次通信 ID | 否 |
| trace_id | 调试/链路追踪层 | 一次调用链追踪 ID | 否 |
| transaction_id | 写操作审计层 | 一次写工具调用的完整改动 | 否 |
| operation_id | Journal 内部层 | transaction 内子操作 | 否 |
| block_id | Graph ownership 层 | 完整 BlueprintHelper-owned 逻辑块 | 否 |
| block_ref | Agent 返回压缩层 | 当前图表内局部 block 引用 | 否 |
| entry_ref | 事件/入口层 | 事件入口 / IA 入口 / Override 入口引用 | 通常对应入口节点，但不等同节点 |
| component_group_id | Component ownership 层 | 一组 owned 组件管理单元 | 否 |
| statement_path | TaskSpec / TaskPlan 定位层 | statement 在 JSON 中的位置 | 否 |
| temp_ref | BlueprintLogicSpec 数据流层 | 输出 Pin 的临时值引用 | 否 |
| node_ref / node_path | LogicJson 读取/定位层 | 节点局部引用 / 完整路径 | 指向节点 |
| pin_ref / pin_path | LogicJson 读取/定位层 | Pin 局部引用 / 完整路径 | 指向 Pin |
| link_ref / link_path | LogicJson 读取/定位层 | Link 局部引用 / 完整路径 | 指向 Link |
| node_guid / pin_guid | UE 原生层 | UE 节点 / Pin GUID | 是 |

第一版不引入 `statement_id`。错误定位使用 `statement_path`；verbose/debug 或 Journal 内部可记录 `statement_path -> generated node_guid / node_path` 映射。
