# BlueprintHelper CLI QuickStart

This guide documents the BlueprintHelper CLI entry for shell-capable Agents.

Task write mainline: Agent -> BlueprintHelper CLI -> task-core -> Python Task Compiler -> Bridge preview/execute -> UE Task Runtime -> Existing Capability Clusters.

## Purpose

Use the CLI when an Agent can run shell commands and should avoid large escaped JSON output. The CLI is the current Agent-facing surface for ordinary TaskSpec writes in shell-capable environments. It keeps Agent stdout compact, supports selected-field output, and still preserves TaskSpec-first writes, Python compilation, Bridge preview, and UE Task Runtime execution.

Editor launch/close, TaskSpec, ReadSpec, diagnostics, and result queries all use the CLI in the current Agent workflow. Older MCP lifecycle wiring is deprecated and should not be the documented mainline for new Agent setup.

## Prerequisites

- Complete the Bridge and project profile setup before running CLI commands.
- Build `task-core` and the CLI package so the CLI entry exists under `AgentFaceService/cli/build/cli/`.
- Start Unreal Editor with BlueprintHelper loaded and the Bridge reachable.
- Prepare a bare `BlueprintHelper.TaskSpec.v1` file such as `.\task_spec.json`, or copy one from `Resources/AgentGuide/Templates/write/`.

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
bh blueprinthelper_preview_task --file .\preview_wrapper.json --select status,preview_id,summary,artifacts.full_result
bh blueprinthelper_execute_task --file .\execute_wrapper.json --select status,task_run_id,summary
bh blueprint_get_runtime_profile --json "{}" --select status,summary
```

The direct CLI registry is the current non-frozen Agent-facing TaskSpec/read/debug summary surface. Frozen legacy/expert tools are not re-exposed through CLI, even if `--expert` is passed. Use `bh open_editor` / `bh close_editor`, or the direct `blueprint_open_editor` / `blueprint_close_editor` names, when an Agent owns editor lifecycle.

## Preview

Run preview first. This compiles `BlueprintHelper.TaskSpec.v1`, then uses the Bridge preview and UE-side validation path.

```powershell
node <PLUGIN_ROOT>\AgentFaceService\cli\build\cli\index.js task preview --file .\task_spec.json --format summary
```

Use `--format summary` for normal Agent loops so the shell returns compact text plus artifact paths instead of large escaped JSON blobs.

The direct tool-name command uses the tool input shape. Prefer the wrapper templates:

```powershell
node <PLUGIN_ROOT>\AgentFaceService\cli\build\cli\index.js blueprinthelper_preview_task --file .\preview_wrapper.json --select status,preview_id,artifacts.full_result
```

## Execute

Execute only after preview passes. This uses the same Python Task Compiler handoff, Bridge task execution, and UE Task Runtime execution path as `blueprinthelper_execute_task`.

```powershell
node <PLUGIN_ROOT>\AgentFaceService\cli\build\cli\index.js task execute --file .\task_spec.json --format summary
```

## Read Full Result

Use the artifact paths returned by summary output for follow-up inspection. Use `--format json` only when the Agent truly needs the full JSON in context.

## Waiting Hints

UE-bound Bridge requests can wait behind editor-side work. When a Bridge call is still pending, the CLI writes progress hints to `stderr` such as:

```text
[BlueprintHelper CLI] waiting for UE Bridge response: command=preview_task_plan elapsed_ms=30000. UE-bound requests are serialized on the editor side; keep waiting unless the CLI exits.
```

Agents should treat these lines as keep-alive/progress messages, not command output. Parse only `stdout` for the final `BlueprintHelper.CliResult.v1` JSON. The default wait-hint cadence is once every 30 seconds, and the default CLI Bridge request timeout is 10 minutes for Agent workflows. Tune it with `BPH_CLI_BRIDGE_REQUEST_TIMEOUT_MS`; tune or disable hints with `BPH_CLI_WAIT_HINT_INITIAL_MS`, `BPH_CLI_WAIT_HINT_INTERVAL_MS`, or `BPH_CLI_WAIT_HINTS=0`.

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
- Use bare TaskSpec files with `task preview` / `task execute`; use `task_spec` wrapper files with `blueprinthelper_preview_task` / `blueprinthelper_execute_task`.
- Prefer `Resources/AgentGuide/Templates/` copy-and-edit JSON templates over inline PowerShell `--json` for complex inputs.
- Preview before execute.
- The CLI is the Agent-facing transport layer; it does not replace the Python Task Compiler or UE Task Runtime.
- The CLI is not a raw Bridge write surface for ordinary asset writes.
- For schema and boundary details, use [TaskSpec_TaskPlan_Contract_20260504.md](../Resources/Docs/TaskSpec_TaskPlan_Contract_20260504.md).

