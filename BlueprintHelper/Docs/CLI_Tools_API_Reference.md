# BlueprintHelper CLI Tools API Reference

Document version: 2026-05-12

This reference is aligned with the current documentation mainline, where CLI is the supported Agent entry.

## Architecture

```text
Agent -> CLI command -> task-core -> Python Task Compiler / Read Router -> Bridge preview/execute/read -> UE Task Runtime -> Existing UE capability clusters
```

Ordinary Agents author `BlueprintHelper.TaskSpec.v1` only. They do not submit `TaskPlan` directly and they do not use legacy low-level direct tools as their default workflow.

## Entry Rule

- Every supported Agent-facing capability must be reachable through `bh <tool_name>`.
- Any remaining MCP endpoint is deprecated internal transport, not a supported Agent workflow.
- CLI write commands must still pass through TaskSpec validation, preview, and UE Task Runtime.
- Raw Bridge write commands are not part of the public Agent surface.

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
blueprinthelper_preview_task
blueprinthelper_request_write_session
blueprinthelper_execute_task
blueprinthelper_get_task_result
blueprinthelper_get_debug_case
blueprinthelper_list_debug_cases
blueprinthelper_export_debug_bundle
blueprint_open_editor
```

Frozen legacy/expert commands are not part of the supported CLI surface. Passing `--expert` does not re-enable removed commands.

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

Task commands wrap TaskSpec under root field `task_spec`:

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1"
  }
}
```

Do not flatten TaskSpec root fields into the command root object.

### CallFunction Resolution

TaskSpec `body.statements[]` entries with `kind: "call_function"` are resolved during preview and execute by the UE-side internal GraphWrite CallFunctionResolver. The CLI does not expose Blueprint action menus, editor right-click context, selected editor state, or K2 spawner concepts to Agents.

Allowed `call_function.name` forms:

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

## Primary Schemas

| Schema | Producer / Owner | Purpose |
|---|---|---|
| `BlueprintHelper.ReadSpec.v1` | Agent | Generic read request |
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
- Agents must not request, inject, or forward `BLUEPRINTHELPER_BRIDGE_TOKEN`, `auth_token`, or `auth_session` for ordinary interactive writes.

## Legacy / Internal Inventory

Legacy/internal/debug/expert commands may still exist behind internal transport and tests, but they are not part of the supported Agent surface. They are kept only for:

- internal Task Runtime lowering targets
- expert/debug recovery
- regression fixtures
- historical compatibility maintenance

Ordinary Agents should not plan around those commands.
