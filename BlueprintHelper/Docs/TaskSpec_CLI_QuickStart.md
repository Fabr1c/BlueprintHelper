# BlueprintHelper CLI QuickStart

This guide documents the BlueprintHelper CLI entry for shell-capable Agents.

Task write mainline: Agent -> BlueprintHelper CLI -> task-core -> Python Task Compiler -> Bridge preview/execute -> UE Task Runtime -> Existing Capability Clusters.

## Purpose

Use the CLI when an Agent can run shell commands and should avoid large escaped JSON output. The CLI is the current Agent-facing surface in shell-capable environments. It keeps Agent stdout compact, supports selected-field output, and still preserves TaskSpec-first writes, Python compilation, Bridge preview, and UE Task Runtime execution.

CLI is the only supported Agent entry. Any remaining MCP endpoint is deprecated compatibility/debug transport only.

## Prerequisites

- Complete the Bridge and project profile setup before running CLI commands.
- Build `task-core` and the CLI package so the CLI entry exists under `AgentFaceService/cli/build/cli/`.
- Start Unreal Editor with BlueprintHelper loaded and the Bridge reachable.
- Prepare a `BlueprintHelper.TaskSpec.v1` file such as `.\task_spec.json`.

## Build

```powershell
cd <PLUGIN_ROOT>\AgentFaceService\task-core
npm install
npm run build

cd <PLUGIN_ROOT>\AgentFaceService\cli
npm install
npm run build
```

## Direct Tool Name Invocation

The primary CLI protocol supports stable BlueprintHelper direct command names:

```powershell
bh <tool_name> [--file params.json | --json "{...}" | --stdin] [--select field[,field...]] [--format summary|json|full]
```

Examples:

```powershell
bh blueprint_get_runtime_profile --json "{}" --select status,summary
bh blueprinthelper_preview_task --file .\task_spec.json --select status,preview_id,summary,artifacts.full_result
bh blueprinthelper_execute_task --file .\task_spec.json --select status,task_run_id,summary
bh blueprint_open_editor --json "{ \"project_file\": \"<ABSOLUTE_UPROJECT_FILE>\", \"wait_timeout_ms\": 120000 }" --select status,summary
```

The direct CLI registry is the current non-frozen Agent-facing tool surface. Frozen legacy/expert tools are not re-exposed through CLI, even if `--expert` is passed.

## Preview

Run preview first. This compiles `BlueprintHelper.TaskSpec.v1`, then uses the Bridge preview and UE-side validation path.

```powershell
node <PLUGIN_ROOT>\AgentFaceService\cli\build\cli\index.js task preview --file .\task_spec.json --format summary
```

Use `--format summary` for normal Agent loops so the shell returns compact text plus artifact paths instead of large escaped JSON blobs.

The grouped command remains a convenience alias for the direct tool-name protocol:

```powershell
node <PLUGIN_ROOT>\AgentFaceService\cli\build\cli\index.js blueprinthelper_preview_task --file .\task_spec.json --select status,preview_id,artifacts.full_result
```

## Execute

Execute only after preview passes. This uses the same Python Task Compiler handoff, Bridge task execution, and UE Task Runtime execution path as `blueprinthelper_execute_task`.

```powershell
node <PLUGIN_ROOT>\AgentFaceService\cli\build\cli\index.js task execute --file .\task_spec.json --format summary
```

## Read Full Result

Use the artifact paths returned by summary output for follow-up inspection. Use `--format json` only when the Agent truly needs the full JSON in context.

## CallFunction Notes

For graph writes, `call_function.name` may be a native name, display name, owner-qualified native name such as `/Script/Engine.KismetSystemLibrary:PrintString`, or an explicit component/member call such as `DoorPanel.AddAngularImpulseInDegrees` for append-owned graph writes. Preview resolves the function part inside UE and blocks ambiguous names with `ambiguous_function_call`; repair those by using an owner-qualified native name. Explicit component/member calls are still limited to append-owned graph writes until merge-owned graph target wiring lands.

## Select Fields

Use `--fields` or `--select` to return only the fields the Agent needs in stdout. Field paths can address top-level fields or nested fields with dot notation. Fields that are not selected are omitted, so routine Agent loops do not need to carry `ok`, `schema`, or the full normalized ToolResult envelope.

```powershell
node <PLUGIN_ROOT>\AgentFaceService\cli\build\cli\index.js task execute --file .\task_spec.json --fields status,task_run_id,summary,artifacts.full_result
```

Example projected output:

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

## Rules

- Keep writes TaskSpec-first. Any CLI write should start from `BlueprintHelper.TaskSpec.v1`.
- Preview before execute.
- The CLI is the Agent-facing transport layer; it does not replace the Python Task Compiler or UE Task Runtime.
- The CLI is not a raw Bridge write surface for ordinary asset writes.
- For schema and boundary details, use [TaskSpec_TaskPlan_Contract_20260504.md](../Resources/Docs/TaskSpec_TaskPlan_Contract_20260504.md).

