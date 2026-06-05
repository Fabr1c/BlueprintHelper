# BlueprintHelper CLI QuickStart

This guide documents the BlueprintHelper CLI entry for shell-capable Agents.

Task write mainline: Agent -> BlueprintHelper CLI -> AgentFace task-core TypeScript compiler -> Bridge preview/execute -> UE Task Runtime -> Existing Capability Clusters.

TaskSpec compiler ownership: AgentFace task-core TypeScript compiler is the canonical production compiler. Legacy fallback and parity-gate paths are retired; new TaskSpec capabilities must be implemented and tested in TS first.

## Purpose

Use the CLI when an Agent can run shell commands and should avoid large escaped JSON output. The CLI is the current Agent-facing surface for ordinary TaskSpec writes in shell-capable environments. It keeps Agent stdout compact, supports selected-field output, and still preserves TaskSpec-first writes, TypeScript compilation, Bridge preview, and UE Task Runtime execution.

TaskSpec, ReadSpec, diagnostics, debug summaries, write-session requests, and result queries use the CLI in the current Agent workflow. MCP is restricted to editor open/close lifecycle in ordinary Agent workflows. Compatibility for `blueprint_open_editor` / `blueprint_close_editor` uses the global MCP lifecycle tools, not CLI lifecycle aliases.

Deprecated MCP ordinary tools are not an alternate transport or fallback path.

## Prerequisites

- Complete the Bridge and project profile setup before running CLI commands.
- Build `task-core` and the CLI package so the CLI entry exists under `AgentFaceService/cli/build/cli/`.
- Start Unreal Editor with BlueprintHelper loaded and the Bridge reachable.
- Prepare a TaskSpec file by copying the matching template returned by current CLI discovery.

## Direct Tool Name Invocation

The primary CLI protocol supports stable BlueprintHelper direct command names:

```powershell
bh <tool_name> [--file params.json | --json "{...}" | --stdin] [--select field[,field...]] [--format summary|json|full]
```

PowerShell-safe input rule: use `--file` for reusable JSON and `--stdin` for generated JSON. Avoid inline `--json $json` for non-trivial payloads because PowerShell can strip quotes before Node receives the argument.

Examples:

```powershell
bh blueprint_get_runtime_profile --json "{}" --select <fields>
bh blueprinthelper_read_context_capabilities --json "{}" --select <fields>
bh blueprinthelper_read_function_chain_context --file <copied-template.json> --select <fields>
bh blueprinthelper_preview_task --file <copied-template.json> --select <fields>
bh blueprinthelper_execute_task --file <copied-template.json> --select <fields>
```

Generated JSON example:

```powershell
$json | bh blueprinthelper_read_context --stdin --format full
```

Use per-tool help before writing a new input file:

```powershell
bh blueprinthelper_read_context --help
bh blueprinthelper_preview_task --help
bh task preview --help
```

Tool help and current CLI discovery provide concrete template file paths. Prefer copying the returned template, replacing placeholders, and passing the result with `--file` instead of building large JSON directly in the shell.

The direct CLI registry is the current non-frozen Agent-facing TaskSpec/read/debug summary surface. Frozen legacy/expert tools are not re-exposed through CLI, even if `--expert` is passed. Use the global MCP allowlist when an Agent owns editor lifecycle. Do not call `bh open_editor` / `bh close_editor`, or direct CLI `blueprint_open_editor` / `blueprint_close_editor`, as Agent compatibility paths. CLI lifecycle invocation is blocked with `lifecycle_mcp_required`; if lifecycle MCP is unavailable, report `lifecycle_mcp_unavailable`.

## Read Context Capabilities

Use `blueprinthelper_read_context_capabilities` when an Agent needs to discover supported ReadContext read types, asset target types, and formats. This command is local to task-core, does not read UE assets, and does not call Bridge.

```powershell
'{}' | bh blueprinthelper_read_context_capabilities --stdin --select <fields>
```

Use current CLI help/discovery for the result fields to select.

## Preview

Run preview first. This compiles `BlueprintHelper.TaskSpec.v1`, then uses the Bridge preview and UE-side validation path.

```powershell
node <PLUGIN_ROOT>\AgentFaceService\cli\build\cli\index.js task preview --file <copied-template.json> --format summary
```

Use `--format summary` for normal Agent loops so the shell returns compact text plus artifact paths instead of large escaped JSON blobs.

The direct tool-name command uses the tool input shape returned by CLI discovery:

```powershell
node <PLUGIN_ROOT>\AgentFaceService\cli\build\cli\index.js blueprinthelper_preview_task --file <copied-template.json> --select <fields>
```

## Execute

Execute only after preview passes. This uses the same canonical TypeScript compiler handoff, Bridge task execution, and UE Task Runtime execution path as `blueprinthelper_execute_task`.

```powershell
node <PLUGIN_ROOT>\AgentFaceService\cli\build\cli\index.js task execute --file <copied-template.json> --format summary
```

## Read Full Result

Use artifact paths returned by summary output for follow-up inspection. Use current CLI help/discovery for exact result fields. Ordinary Agent output should stay compact and should not expand raw Bridge payloads or internal execution policy details.

Use `--expert` only when raw execution diagnostics are needed. In expert mode the CLI also returns `artifacts.debug_result`, which contains `BlueprintHelper.CliDebugResult.v1` with the raw Bridge result and trace ids. Use `--format json` only when the Agent truly needs the full JSON in stdout.

## Function Chain Reads

Use `blueprinthelper_read_function_chain_context` after reading an entry graph when the next step is to inspect project-authored functions/events reached from that entry. It returns a compact index, not full graph bodies.

Use CLI discovery to locate the current FunctionChain input template. Do not pass GUID or owner fields. Engine and trusted plugin calls are summarized by count; follow returned refs with `blueprinthelper_read_context` when detailed logic is needed.

## Waiting Hints

UE-bound Bridge requests can wait behind editor-side work. When a Bridge call is still pending, the CLI writes progress hints to `stderr` such as:

```text
[BlueprintHelper CLI] waiting for UE Bridge response: command=preview_task_plan elapsed_ms=30000. UE-bound requests are serialized on the editor side; keep waiting unless the CLI exits.
```

Agents should treat these lines as keep-alive/progress messages, not command output. Parse only `stdout` for the final compact CLI JSON; default stdout no longer includes a top-level `BlueprintHelper.CliResult.v1` schema field. The default wait-hint cadence is once every 30 seconds, and the default CLI Bridge request timeout is 10 minutes for Agent workflows. Tune it with `BPH_CLI_BRIDGE_REQUEST_TIMEOUT_MS`; tune or disable hints with `BPH_CLI_WAIT_HINT_INITIAL_MS`, `BPH_CLI_WAIT_HINT_INTERVAL_MS`, or `BPH_CLI_WAIT_HINTS=0`.

## Call Notes

For graph writes, `call.name` may be a native name, display name, owner-qualified native name such as `/Script/Engine.KismetSystemLibrary:PrintString`, or an explicit component/member call such as `DoorPanel.AddAngularImpulseInDegrees` for append-owned graph writes. Preview resolves the function part inside UE and blocks ambiguous names with `ambiguous_function_call`; repair those by using an owner-qualified native name. Explicit component/member calls are still limited to append-owned graph writes until merge-owned graph target wiring lands.

## Select Fields

Use `--fields` or `--select` to return only the fields the Agent needs in stdout. Field paths can address top-level fields or nested fields with dot notation. Fields that are not selected are omitted, so routine Agent loops do not need to carry `ok`, `schema`, or the full normalized ToolResult envelope.

```powershell
node <PLUGIN_ROOT>\AgentFaceService\cli\build\cli\index.js task execute --file <copied-template.json> --fields <fields>
```

## Rules

- Keep writes TaskSpec-first. Any CLI write should start from `BlueprintHelper.TaskSpec.v1`.
- Use CLI-discovered templates to distinguish grouped commands from direct tool-name wrappers.
- Prefer CLI-discovered copy-and-edit JSON files or `--stdin` over inline PowerShell `--json` for complex inputs.
- Preview before execute.
- The CLI is the Agent-facing transport layer; it does not replace the canonical AgentFace task-core TypeScript compiler or UE Task Runtime.
- The CLI is not a raw Bridge write surface for ordinary asset writes.
- Use global MCP allowlist for Agent-owned Editor open/close; compatibility paths for `blueprint_open_editor` / `blueprint_close_editor` are MCP lifecycle paths, not CLI aliases.
- Deprecated MCP ordinary tools are not fallback paths for ordinary Agent workflows.
- For ordinary Agent input shapes, use per-command help and current CLI discovery.
