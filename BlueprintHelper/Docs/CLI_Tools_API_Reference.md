# BlueprintHelper CLI Tools API Reference

Document version: 2026-05-17

This reference is aligned with the current implementation: the CLI is the supported Agent entry for ordinary TaskSpec, ReadSpec, diagnostics, debug-summary, write-session, and result-query work. MCP is restricted to `blueprint_open_editor`, `blueprint_close_editor`, and developer-only `blueprint_developer_exec_console_command`; the CLI still exposes lifecycle aliases for compatibility and manual fallback.

## Architecture

```text
Agent -> CLI command -> task-core -> Python Task Compiler / Read Router -> Bridge preview/execute/read -> UE Task Runtime -> Existing UE capability clusters
```

Ordinary Agents author `BlueprintHelper.TaskSpec.v1` only. They do not submit `TaskPlan` directly and they do not use legacy low-level direct tools as their default workflow.

## Entry Rule

- Every supported CLI-facing TaskSpec/read/debug summary capability must be reachable through `bh <tool_name>`.
- Agent-owned Editor lifecycle should use the global MCP allowlist tools.
- CLI lifecycle aliases `bh open_editor` / `bh close_editor` and direct `blueprint_open_editor` / `blueprint_close_editor` are compatibility/manual fallback entries, not ordinary asset workflow tools.
- CLI write commands must still pass through TaskSpec validation, preview, and UE Task Runtime.
- Raw Bridge write commands are not part of the public Agent surface.
- Deprecated MCP ordinary tools are not fallback entries. Do not use them, do not write tests for them, and do not run old MCP tool tests.

Canonical shell form:

```powershell
bh <tool_name> [--file params.json | --json "{...}" | --stdin] [--select field[,field...]] [--format summary|json|full]
```

## Supported Command Surface

Default Agent-facing commands:

```text
blueprint_get_runtime_profile
blueprinthelper_diagnostics
blueprinthelper_diagnostics_runtime
blueprinthelper_read_agent_guide
blueprinthelper_read_context
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

Use the global MCP allowlist for normal Agent-owned lifecycle. If the CLI compatibility path is explicitly needed, short aliases `bh open_editor` and `bh close_editor` map to these same lifecycle tool names. The developer-only `blueprint_developer_exec_console_command` is reserved for local BlueprintHelper development/test orchestration and is not an ordinary Agent asset workflow tool.

Internal/plugin-development command not exposed to ordinary Agents:

```text
blueprinthelper_apply_review_action
```

Frozen legacy/expert commands are not part of the supported CLI surface. Passing `--expert` does not re-enable removed commands.

Grouped CLI commands implemented by `AgentFaceService/cli`:

```text
bh task preview --file <bare-task-spec.json>
bh task execute --file <bare-task-spec.json>
bh task result --id <task_run_id>
bh context read --file <read-task-context.json>
bh bridge ping
bh bridge call --command <read_only_bridge_command>
```

`context read` uses the same root shape as `blueprinthelper_read_task_context`. `bridge call` is read-only and sends an empty payload; prefer direct tool names for parameterized reads.

## Common Return Shape

CLI normally prints compact `BlueprintHelper.CliResult.v1` summaries to stdout. The full artifact currently keeps the normalized envelope name `BlueprintHelper.McpToolResult.v1` for compatibility.

Typical summary projection:

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

Use `--select` or `--fields` for the smallest possible stdout payload. Large asset context, raw payloads, and debug artifacts belong in follow-up reads or artifact files, not inline summaries.

Long UE Bridge waits emit keep-alive progress hints to `stderr`; `stdout` remains reserved for the final JSON result. Agents should continue waiting when they see a line like `waiting for UE Bridge response` and should not parse it as command output. CLI-created Bridge clients default to a 10-minute request timeout for Agent workflows; use `BPH_CLI_BRIDGE_REQUEST_TIMEOUT_MS` to tune it. Use `BPH_CLI_WAIT_HINT_INITIAL_MS`, `BPH_CLI_WAIT_HINT_INTERVAL_MS`, or `BPH_CLI_WAIT_HINTS=0` to tune or disable the `stderr` hints.

Copy-and-edit JSON templates live under `BlueprintHelper/Resources/AgentGuide/Templates/`. Prefer those files over inline PowerShell `--json` for complex parameters.

## TaskSpec Commands

The TaskSpec-first write loop is:

```text
bh blueprint_get_runtime_profile
-> bh blueprinthelper_read_task_context or bh blueprinthelper_read_context
-> author BlueprintHelper.TaskSpec.v1
-> bh blueprinthelper_preview_task
-> bh blueprinthelper_request_write_session when write_permission is disabled
-> bh blueprinthelper_execute_task
-> bh blueprinthelper_get_task_result when needed
```

Tool-name task commands prefer wrapping TaskSpec under root field `task_spec`:

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1"
  }
}
```

Grouped commands use a bare TaskSpec file:

```powershell
bh task preview --file .\task_spec.json
bh task execute --file .\task_spec.json
```

Do not add an extra `args` wrapper. Prefer the AgentGuide template files when authoring JSON by hand.

### CallFunction Resolution

TaskSpec `body.statements[]` entries should use the GraphStatement short form `kind: "call"` with `target`, or the legacy-compatible `kind: "call_function"` with `name`. They are resolved during preview and execute by the UE-side internal GraphWrite CallFunctionResolver. The CLI does not expose Blueprint action menus, editor right-click context, selected editor state, or K2 spawner concepts to Agents.

Allowed call target forms:

- Native function name, for example `PrintString`.
- Blueprint display name, for example `Print String`.
- Owner-qualified native function name, for example `/Script/Engine.KismetSystemLibrary:PrintString`.

Preview blocks ambiguous or unsupported calls instead of guessing. Use the resolver error code to repair the TaskSpec:

| Error code | Meaning | Repair |
|---|---|---|
| `ambiguous_function_call` | Multiple graph-usable functions match the name | Use owner-qualified native name |
| `function_call_not_found` | No graph-usable function matches the name | Read task context and choose an available function |
| `explicit_member_call_not_supported` | The current graph write path does not support component/member target prefixes for this strategy | Use `Object.Function` only with append-owned graph writes, or model the target through supported TaskSpec fields |

## ReadSpec Commands

Read commands use `BlueprintHelper.ReadSpec.v1` at the root object:

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
    "format": "summary"
  }
}
```

Use `summary` before whole-graph `logic_md` when graph size is unknown. Use `logic_json` for stable owned-block anchors.

### Function Chain Context

Use `blueprinthelper_read_function_chain_context` when an Agent has an entry function/event/custom event and needs the next project-authored Blueprint logic entries to inspect. The tool returns a compact index only; follow up with `blueprinthelper_read_context` for each returned function/event body.

Root argument shape:

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
- Results use `FunctionChainContext.v1`.
- `custom_logic_refs[]` contains project Blueprint custom functions/events/custom events, including pure functions used as branch conditions or argument sources.
- Engine/trusted plugin/native utility calls are filtered into summary counts only.
- Result refs do not include `node_ref`, `node_path`, owner fields, or raw GUID fields.

## Primary Schemas

| Schema | Producer / Owner | Purpose |
|---|---|---|
| `BlueprintHelper.ReadSpec.v1` | Agent | Generic read request |
| `FunctionChainContext.v1` | Bridge read service | Compact project custom logic call-chain index |
| `BlueprintHelper.TaskSpec.v1` | Agent | Semantic task specification |
| `BlueprintHelper.TaskPlan.v1` | task-core / Python compiler | Compiler-owned structured execution plan |
| `BlueprintHelper.TaskRunJournal.v1` | UE Task Runtime | Task-level execution journal |
| `BlueprintHelper.McpToolResult.v1` | Normalized full artifact | Compatibility envelope name for full results |
| `BlueprintHelper.CliResult.v1` | CLI stdout | Compact Agent-facing summary |

Canonical contract details remain in [TaskSpec_TaskPlan_Contract_20260504.md](../Resources/Docs/TaskSpec_TaskPlan_Contract_20260504.md).

## Write Authorization

- Interactive writes use `blueprinthelper_request_write_session`.
- The running Unreal Editor shows a simple accept/reject prompt.
- Approval belongs to the running Editor/Bridge for the approved scope and lifetime.
- `scope` is `project` or `asset_list`; include `asset_paths` for `asset_list`.
- Agents must not request, inject, or forward `BLUEPRINTHELPER_BRIDGE_TOKEN`, `auth_token`, or `auth_session` for ordinary interactive writes.

## Internal Review Action Shape

`blueprinthelper_apply_review_action` is registered as an expert-only shared-registry tool for plugin development and internal validation. It is not included in ordinary AgentGuide templates and ordinary Agents must not use it as a write recovery path.

Current root shape:

```json
{
  "review_record_id": "review_record_id",
  "action": "accept",
  "target_keys": [
    "optional_target_key"
  ]
}
```

## Legacy / Internal Inventory

Legacy/internal/debug/expert commands may still exist behind internal transport, but deprecated MCP ordinary tools are not part of the supported Agent surface and must not be restored through tests. Internal commands are kept only for:

- internal Task Runtime lowering targets
- expert/debug recovery
- regression fixtures outside the deprecated MCP ordinary tool surface
- historical compatibility maintenance

Ordinary Agents should not plan around those commands.
