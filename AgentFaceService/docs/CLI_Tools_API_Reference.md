# BlueprintHelper CLI Tools API Reference / CLI 工具 API 参考

Document version / 文档版本: `2026-05-17`

## 中文

本参考与当前实现对齐：CLI 是普通 Agent 执行 TaskSpec、ReadSpec、diagnostics、debug summary、write-session 和 result query 的支持入口。MCP 限制为 `blueprint_open_editor` 和 `blueprint_close_editor` lifecycle 入口；CLI lifecycle alias 只作为兼容/手动 fallback。

### 架构

```text
Agent -> CLI command -> task-core -> Python Task Compiler / Read Router -> Bridge preview/execute/read -> UE Task Runtime -> Existing UE capability clusters
```

普通 Agent 只编写 `BlueprintHelper.TaskSpec.v1`，不直接提交 `TaskPlan`，也不默认使用 legacy low-level direct tools。

### 入口规则

- 支持的 TaskSpec/read/debug summary 能力应通过 `bh <tool_name>` 可达。
- Agent-owned Editor lifecycle 应使用全局 MCP allowlist。
- CLI lifecycle alias `bh open_editor` / `bh close_editor` 和 direct `blueprint_open_editor` / `blueprint_close_editor` 是兼容/手动 fallback，不是普通资产工作流工具。
- CLI 写入必须经过 TaskSpec validation、preview 和 UE Task Runtime。
- Raw Bridge write command 不是公开 Agent surface。
- 废弃 MCP 普通工具不是 fallback，也不是普通 Agent 可选入口。

通用命令形态：

```powershell
bh <tool_name> [--file params.json | --json "{...}" | --stdin] [--select field[,field...]] [--format summary|json|full]
```

PowerShell-safe input rule: `--file` and `--stdin` are the stable JSON input paths. Inline `--json` is acceptable for `{}` and very small literals, but generated JSON should be piped:

```powershell
$json | bh blueprinthelper_read_context --stdin --format full
```

### 支持命令面

默认 Agent-facing commands:

```text
blueprint_get_runtime_profile
blueprinthelper_diagnostics
blueprinthelper_diagnostics_runtime
blueprinthelper_read_agent_guide
blueprinthelper_read_context
blueprinthelper_read_context_capabilities
blueprinthelper_read_task_context
blueprinthelper_read_reference_context
blueprinthelper_read_function_chain_context
blueprinthelper_preview_task
blueprinthelper_request_write_session
blueprinthelper_execute_task
blueprinthelper_get_task_result
blueprinthelper_get_debug_case
blueprinthelper_list_debug_cases
blueprinthelper_export_debug_bundle
blueprinthelper_query_review_records
```

Lifecycle compatibility commands:

```text
blueprint_open_editor
blueprint_close_editor
```

分组 CLI commands:

```text
bh task preview --file <bare-task-spec.json>
bh task execute --file <bare-task-spec.json>
bh task result --id <task_run_id>
bh context read --file <read-task-context.json>
bh bridge ping
bh bridge call --command <read_only_bridge_command>
```

`blueprinthelper_apply_review_action` 是 plugin-development/internal 命令，不属于普通 Agent 工作流。

### 返回形态

CLI 通常向 stdout 输出精简的 `BlueprintHelper.CliResult.v1`。`artifacts.full_result` 使用精简的 `BlueprintHelper.CliFullResult.v1`，不输出 nested ToolResult schema、trace id、raw `bridge_result`、重复 `target_assets` 或重复 nested `task_run_id`。需要 raw Bridge/trace 诊断时使用 `--expert`，并读取 `artifacts.debug_result`。

示例：

```json
{
  "status": "executed",
  "task_run_id": "task_cli_001",
  "summary": {
    "target_assets": ["/Game/BP_Player"],
    "planned_steps": 1,
    "modified": true
  },
  "artifacts": {
    "full_result": ".blueprinthelper/cli-runs/task_cli_001/result.json"
  }
}
```

使用 `--select` 或 `--fields` 保持 stdout 最小。大型上下文、raw payload 和 debug artifact 应留在 artifact 文件中。

UE Bridge 长等待会向 `stderr` 输出 keep-alive 提示；`stdout` 保留给最终 JSON。看到 `waiting for UE Bridge response` 时应继续等待，除非 CLI 退出。

### TaskSpec 命令

TaskSpec-first 写入循环：

```text
bh blueprint_get_runtime_profile
-> bh blueprinthelper_read_task_context or bh blueprinthelper_read_context
-> author BlueprintHelper.TaskSpec.v1
-> bh blueprinthelper_preview_task
-> bh blueprinthelper_request_write_session when write_permission is disabled
-> bh blueprinthelper_execute_task
-> bh blueprinthelper_get_task_result when needed
```

Tool-name task commands 使用 `task_spec` wrapper：

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1"
  }
}
```

Grouped commands 使用 bare TaskSpec：

```powershell
bh task preview --file .\task_spec.json
bh task execute --file .\task_spec.json
```

### CallFunction 解析

TaskSpec `body.statements[]` 可以使用 GraphStatement short form `kind: "call"` + `target`，也可以使用 legacy-compatible `kind: "call_function"` + `name`。解析发生在 preview/execute 阶段，由 UE 侧 GraphWrite CallFunctionResolver 完成。

允许的 call target：

- Native function name，例如 `PrintString`。
- Blueprint display name，例如 `Print String`。
- Owner-qualified native name，例如 `/Script/Engine.KismetSystemLibrary:PrintString`。

| Error code | 含义 | 修复 |
|---|---|---|
| `ambiguous_function_call` | 多个 graph-usable functions 匹配 | 使用 owner-qualified native name |
| `function_call_not_found` | 没有匹配的 graph-usable function | 读取 task context 后选择可用函数 |
| `explicit_member_call_not_supported` | 当前 graph write 路径不支持该 component/member target | 只在 append-owned graph writes 中使用 `Object.Function`，或改用支持字段 |

### ReadSpec 命令

Read command 使用根对象 `BlueprintHelper.ReadSpec.v1`：

```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "blueprint_logic",
  "target": {
    "asset_path": "/Game/Blueprints/BP_Door",
    "target_type": "graph",
    "target_name": "EventGraph"
  },
  "view": {
    "format": "logic_json"
  }
}
```

图表大小未知时先用 `logic_json`，不要直接 whole-graph `logic_md`。需要稳定 owned-block anchors 时也使用 `logic_json`。ReadSpec 不再支持 `view.format=summary`；非 logic 读取直接省略 `view.format`。

ReadContext capability discovery uses a separate local command and does not read UE assets:

```powershell
'{}' | bh blueprinthelper_read_context_capabilities --stdin --select status,artifacts.full_result
```

The full artifact payload schema is `ReadContextCapabilities.v1`. `asset_types`, `formats`, and `read_type_ids` are full sets; `read_types[]` lists only unsupported asset types and unsupported formats for each read type.

### Function Chain Context

`blueprinthelper_read_function_chain_context` 用于从入口 function/event/custom event 查找下一层项目自定义 Blueprint 逻辑。它返回 compact index；需要函数体时再使用 `blueprinthelper_read_context`。

```json
{
  "asset_path": "/Game/BP_PlayerController",
  "target_type": "custom_event",
  "target_name": "Input_Fire",
  "graph_name": "EventGraph",
  "max_depth": 3,
  "include_data_dependencies": true,
  "expand_cross_asset": true
}
```

规则：

- `target_type` 为 `function`、`event` 或 `custom_event`。
- 不要发送 `target_guid`、`entry`、`target`、`query` 或 owner fields。
- 结果 schema 为 `FunctionChainContext.v1`。
- Engine/trusted plugin/native utility calls 只进入 summary counts。

### 主要 Schema

| Schema | Owner | Purpose / 用途 |
|---|---|---|
| `BlueprintHelper.ReadSpec.v1` | Agent | Generic read request / 通用读取请求 |
| `ReadContextCapabilities.v1` | task-core local read service | Compact ReadContext capability matrix |
| `FunctionChainContext.v1` | Bridge read service | Compact custom logic call-chain index / 调用链索引 |
| `BlueprintHelper.TaskSpec.v1` | Agent | Semantic task specification / 语义任务描述 |
| `BlueprintHelper.TaskPlan.v1` | task-core / Python compiler | Compiler-owned execution plan / 编译器生成执行计划 |
| `BlueprintHelper.TaskRunJournal.v1` | UE Task Runtime | Task execution journal / 执行日志 |
| `BlueprintHelper.CliResult.v1` | CLI stdout | Compact Agent summary / Agent 精简摘要 |
| `BlueprintHelper.CliFullResult.v1` | CLI full artifact | Compact full artifact / 精简 full artifact |
| `BlueprintHelper.CliDebugResult.v1` | CLI expert debug artifact | Raw Bridge and trace diagnostics / expert 诊断 |

### 写入授权

- 交互式写入使用 `blueprinthelper_request_write_session`。
- Unreal Editor 会显示 accept/reject prompt。
- 授权归属于正在运行的 Editor/Bridge，限定 scope 和 lifetime。
- `scope` 为 `project` 或 `asset_list`；`asset_list` 需要 `asset_paths`。
- 普通交互式写入中，Agent 不得请求、注入或转发 `BLUEPRINTHELPER_BRIDGE_TOKEN`、`auth_token`、`auth_session`。

### Legacy / Internal

Legacy/internal/debug/expert commands 可能仍存在于内部 transport 后面，但不属于普通 Agent surface，也不作为 Agent-facing fallback 暴露。

## English

This reference matches the current implementation: the CLI is the supported entry for ordinary Agent TaskSpec, ReadSpec, diagnostics, debug-summary, write-session, and result-query work. MCP is restricted to the `blueprint_open_editor` and `blueprint_close_editor` lifecycle entries; CLI lifecycle aliases are compatibility/manual fallback only.

### Architecture

```text
Agent -> CLI command -> task-core -> Python Task Compiler / Read Router -> Bridge preview/execute/read -> UE Task Runtime -> Existing UE capability clusters
```

Ordinary Agents author `BlueprintHelper.TaskSpec.v1` only. They do not submit `TaskPlan` directly and do not use legacy low-level direct tools as their default workflow.

### Entry Rule

- Every supported CLI-facing TaskSpec/read/debug summary capability should be reachable through `bh <tool_name>`.
- Agent-owned Editor lifecycle should use the global MCP allowlist.
- CLI lifecycle aliases `bh open_editor` / `bh close_editor` and direct `blueprint_open_editor` / `blueprint_close_editor` are compatibility/manual fallback entries, not ordinary asset workflow tools.
- CLI write commands must pass through TaskSpec validation, preview, and UE Task Runtime.
- Raw Bridge write commands are not part of the public Agent surface.
- Deprecated MCP ordinary tools are not fallback entries. Do not use, test, or restore them.

Canonical shell form:

```powershell
bh <tool_name> [--file params.json | --json "{...}" | --stdin] [--select field[,field...]] [--format summary|json|full]
```

PowerShell-safe input rule: prefer `--file` or `--stdin`; inline `--json $json` can arrive at Node with quotes stripped.

### Supported Command Surface

Default Agent-facing commands:

```text
blueprint_get_runtime_profile
blueprinthelper_diagnostics
blueprinthelper_diagnostics_runtime
blueprinthelper_read_agent_guide
blueprinthelper_read_context
blueprinthelper_read_context_capabilities
blueprinthelper_read_task_context
blueprinthelper_read_reference_context
blueprinthelper_read_function_chain_context
blueprinthelper_preview_task
blueprinthelper_request_write_session
blueprinthelper_execute_task
blueprinthelper_get_task_result
blueprinthelper_get_debug_case
blueprinthelper_list_debug_cases
blueprinthelper_export_debug_bundle
blueprinthelper_query_review_records
```

Lifecycle compatibility commands:

```text
blueprint_open_editor
blueprint_close_editor
```

Grouped CLI commands:

```text
bh task preview --file <bare-task-spec.json>
bh task execute --file <bare-task-spec.json>
bh task result --id <task_run_id>
bh context read --file <read-task-context.json>
bh bridge ping
bh bridge call --command <read_only_bridge_command>
```

`blueprinthelper_apply_review_action` is a plugin-development/internal command and is not part of ordinary Agent workflows.

### Return Shape

The CLI usually prints compact `BlueprintHelper.CliResult.v1` summaries to stdout. `artifacts.full_result` uses the compact `BlueprintHelper.CliFullResult.v1` shape and omits nested ToolResult schemas, trace ids, raw `bridge_result`, duplicate `target_assets`, and duplicate nested `task_run_id`. Use `--expert` only when raw Bridge/trace diagnostics are needed, then read `artifacts.debug_result`.

Example:

```json
{
  "status": "executed",
  "task_run_id": "task_cli_001",
  "summary": {
    "target_assets": ["/Game/BP_Player"],
    "planned_steps": 1,
    "modified": true
  },
  "artifacts": {
    "full_result": ".blueprinthelper/cli-runs/task_cli_001/result.json"
  }
}
```

Use `--select` or `--fields` for the smallest possible stdout payload. Large context, raw payloads, and debug artifacts belong in artifact files.

Long UE Bridge waits emit keep-alive hints to `stderr`; `stdout` is reserved for final JSON. Continue waiting when you see `waiting for UE Bridge response` unless the CLI exits.

### TaskSpec Commands

TaskSpec-first write loop:

```text
bh blueprint_get_runtime_profile
-> bh blueprinthelper_read_task_context or bh blueprinthelper_read_context
-> author BlueprintHelper.TaskSpec.v1
-> bh blueprinthelper_preview_task
-> bh blueprinthelper_request_write_session when write_permission is disabled
-> bh blueprinthelper_execute_task
-> bh blueprinthelper_get_task_result when needed
```

Tool-name task commands use a `task_spec` wrapper:

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1"
  }
}
```

Grouped commands use a bare TaskSpec:

```powershell
bh task preview --file .\task_spec.json
bh task execute --file .\task_spec.json
```

### CallFunction Resolution

TaskSpec `body.statements[]` entries can use the GraphStatement short form `kind: "call"` with `target`, or the legacy-compatible `kind: "call_function"` with `name`. Resolution happens during preview/execute in the UE-side GraphWrite CallFunctionResolver.

Allowed call targets:

- Native function name, for example `PrintString`.
- Blueprint display name, for example `Print String`.
- Owner-qualified native name, for example `/Script/Engine.KismetSystemLibrary:PrintString`.

| Error code | Meaning | Repair |
|---|---|---|
| `ambiguous_function_call` | Multiple graph-usable functions match | Use owner-qualified native name |
| `function_call_not_found` | No graph-usable function matches | Read task context and choose an available function |
| `explicit_member_call_not_supported` | Current graph write path does not support this component/member target | Use `Object.Function` only with append-owned graph writes, or model the target through supported fields |

### ReadSpec Commands

Read commands use root object `BlueprintHelper.ReadSpec.v1`:

```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "blueprint_logic",
  "target": {
    "asset_path": "/Game/Blueprints/BP_Door",
    "target_type": "graph",
    "target_name": "EventGraph"
  },
  "view": {
    "format": "logic_json"
  }
}
```

Use `logic_json` before whole-graph `logic_md` when graph size is unknown. Use `logic_json` for stable owned-block anchors. ReadSpec no longer supports `view.format=summary`; non-logic reads omit `view.format`.

ReadContext capability discovery uses a separate local command and does not read UE assets:

```powershell
'{}' | bh blueprinthelper_read_context_capabilities --stdin --select status,artifacts.full_result
```

The full artifact payload schema is `ReadContextCapabilities.v1`. `asset_types`, `formats`, and `read_type_ids` are full sets; `read_types[]` lists only unsupported asset types and unsupported formats for each read type.

### Function Chain Context

`blueprinthelper_read_function_chain_context` follows an entry function/event/custom event to find the next project-authored Blueprint logic entries. It returns a compact index; use `blueprinthelper_read_context` for full bodies.

```json
{
  "asset_path": "/Game/BP_PlayerController",
  "target_type": "custom_event",
  "target_name": "Input_Fire",
  "graph_name": "EventGraph",
  "max_depth": 3,
  "include_data_dependencies": true,
  "expand_cross_asset": true
}
```

Rules:

- `target_type` is `function`, `event`, or `custom_event`.
- Do not send `target_guid`, `entry`, `target`, `query`, or owner fields.
- Result schema is `FunctionChainContext.v1`.
- Engine/trusted plugin/native utility calls are summarized by count only.

### Primary Schemas

| Schema | Owner | Purpose / 用途 |
|---|---|---|
| `BlueprintHelper.ReadSpec.v1` | Agent | Generic read request / 通用读取请求 |
| `ReadContextCapabilities.v1` | task-core local read service | Compact ReadContext capability matrix |
| `FunctionChainContext.v1` | Bridge read service | Compact custom logic call-chain index / 调用链索引 |
| `BlueprintHelper.TaskSpec.v1` | Agent | Semantic task specification / 语义任务描述 |
| `BlueprintHelper.TaskPlan.v1` | task-core / Python compiler | Compiler-owned execution plan / 编译器生成执行计划 |
| `BlueprintHelper.TaskRunJournal.v1` | UE Task Runtime | Task execution journal / 执行日志 |
| `BlueprintHelper.CliResult.v1` | CLI stdout | Compact Agent summary / Agent 精简摘要 |
| `BlueprintHelper.CliFullResult.v1` | CLI full artifact | Compact full artifact |
| `BlueprintHelper.CliDebugResult.v1` | CLI expert debug artifact | Raw Bridge and trace diagnostics |

### Write Authorization

- Interactive writes use `blueprinthelper_request_write_session`.
- Unreal Editor shows an accept/reject prompt.
- Approval belongs to the running Editor/Bridge for the approved scope and lifetime.
- `scope` is `project` or `asset_list`; include `asset_paths` for `asset_list`.
- Agents must not request, inject, or forward `BLUEPRINTHELPER_BRIDGE_TOKEN`, `auth_token`, or `auth_session` for ordinary interactive writes.

### Legacy / Internal

Legacy/internal/debug/expert commands may still exist behind internal transport, but they are not part of the ordinary Agent surface and are not exposed as Agent-facing fallbacks.
