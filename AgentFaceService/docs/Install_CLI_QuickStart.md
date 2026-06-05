# BlueprintHelper CLI QuickStart

This guide connects a local Unreal Editor project, the BlueprintHelper Bridge, the BlueprintHelper CLI, and an AI agent.

Task orchestration mainline:

```text
Agent -> BlueprintHelper CLI -> AgentFace task-core TypeScript compiler -> Bridge preview/execute -> UE Task Runtime -> Existing Capability Clusters
```

TaskSpec compiler ownership: AgentFace task-core TypeScript compiler is the canonical production compiler. Legacy fallback and parity-gate paths are retired; new TaskSpec capabilities must be implemented and tested in TS first.

## Prerequisites

- Unreal Engine 5.3 or newer.
- A UE project that can compile editor plugins.
- Node.js and npm.
- BlueprintHelper installed under the project `Plugins` directory.
- A terminal that can run Windows PowerShell commands.

Path placeholders used in this guide:

```text
Plugin: <PLUGIN_ROOT>
Project file: discovered from the workspace when possible; pass explicit `project_file` only when lifecycle tooling cannot infer it safely
```

## 1. Install The Plugin

For normal first-run setup, use the repository-root installer:

```cmd
cd <PLUGIN_ROOT>
.\install.cmd -ProjectFile <Project.uproject> -EngineRoot <UE root>
```

The installer builds the Agent runtime, links `bh`, registers the repository local marketplace through the official Codex plugin install entry, installs `blueprint-helper@blueprint-helper-local`, installs Codex subagents and the MCP allowlist entry, writes the project profile when project and UE root are known, creates `.blueprinthelper/AgentWorkFlow.md`, refreshes project-root `AGENTS.md` / `CLAUDE.md` markers, and creates default user preference files only when they are missing.

交互式安装优先使用 Node.js 内置终端交互。安装 Codex subagents 或 Claude sideAgents 时，三个 agent 会以表格显示，并把模型与思考等级拆成独立字段。非交互安装自动使用推荐默认值，`task-worker` 默认更强模型。

Interactive install prefers Node.js built-in terminal prompts. When Codex subagents or Claude sideAgents are selected, the three agents are shown in a table with separate model and reasoning fields. Non-interactive install uses the recommended defaults automatically, with a stronger default model for `task-worker`.

Place the plugin at:

```text
<YourProject>\Plugins\BlueprintHelper
```

Open the project in Unreal Editor, enable BlueprintHelper if needed, and rebuild the project if prompted.

If you use an Unreal `BuildPlugin` package, keep the sibling `AgentFaceService` package available separately. UE packaging does not compile or include `AgentFaceService/cli` or `AgentFaceService/task-core`.

## 2. Installer Variants

Use the root installer for setup and rebuilds. Do not duplicate per-package Node build commands in Agent-facing docs.

```cmd
.\install.cmd -ProjectFile <Project.uproject> -EngineRoot <UE root>
.\install.cmd -ProjectFile <Project.uproject> -EngineRoot <UE root> -RunDiagnostics
```

`install.cmd` opens the interactive installer when launched without arguments and passes arguments through to the underlying PowerShell installer when supplied. Prefer this entry in Agent-facing docs so users do not need to run `.ps1` directly.

The repository root keeps only `.cmd` user script entry points. The underlying PowerShell and Node implementation scripts live under `InstallScripts/`. Use `uninstall.cmd` to remove the global CLI link, plugin entries, installed subagents, and lifecycle MCP config.

After `npm link`, the installer removes npm-generated `bh.ps1` / `blueprinthelper-cli.ps1` shims when matching `.cmd` launchers exist. This avoids PowerShell ExecutionPolicy blocking `bh`. If an older install still resolves `bh` to a `.ps1` file, rerun the root installer or call `bh.cmd`.

## 3. Manual Project Profile Fallback

Use this only when the installer could not discover a unique `.uproject` or you intentionally passed `-SkipProjectProfile`.

Store UE version-specific configuration in `<ProjectDir>/.blueprinthelper/project-profile.json` only when this fallback is necessary. The root installer writes this automatically when `-ProjectFile` and `-EngineRoot` are supplied. Agent workflow rules should live in `<ProjectDir>/.blueprinthelper/AgentWorkFlow.md`; project-root `AGENTS.md` / `CLAUDE.md` should only keep the managed BlueprintHelper entry that points to that workflow document.

Bridge connectivity environment variables:

```powershell
$env:BRIDGE_HOST = "127.0.0.1"
$env:BRIDGE_PORT = "54321"
```

Project `.uproject` paths should not be stored globally. Agents discover the target `.uproject` from the current workspace; explicit `project_file` remains available for ambiguous workspaces.

## 4. Start Unreal Editor

Either start Unreal Editor normally with the project, or use the global MCP lifecycle tools after the project profile has `environment.ue_engine_dir`. Compatibility for `blueprint_open_editor` / `blueprint_close_editor` also uses the global MCP lifecycle tools, not CLI lifecycle aliases:

```text
mcp__blueprint_helper__blueprint_open_editor
mcp__blueprint_helper__blueprint_close_editor
```

Do not use plugin-local MCP or deprecated MCP ordinary tools as proof of Agent lifecycle or asset-workflow behavior. The normal Agent-owned lifecycle path is the global MCP allowlist server.

Do not start or close Unreal Editor through CLI lifecycle aliases. `bh open_editor`, `bh close_editor`, `blueprint_open_editor`, and `blueprint_close_editor` are not Agent lifecycle execution paths; if lifecycle MCP is unavailable, report `lifecycle_mcp_unavailable`.

Bridge smoke check:

```powershell
Test-NetConnection 127.0.0.1 -Port 54321
```

If the port is not open, wait for the editor to finish loading, confirm the plugin is enabled, and check the Unreal output log.

## 5. Run The CLI

The CLI is the supported Agent entry for ordinary TaskSpec writes, reads, diagnostics, debug summaries, write-session requests, and result queries. MCP is restricted to editor open/close lifecycle in ordinary Agent workflows; lifecycle compatibility uses the global MCP lifecycle tools rather than CLI aliases, and CLI lifecycle invocation is blocked. Deprecated MCP ordinary tools are not fallback paths.

Examples:

```powershell
bh blueprint_get_runtime_profile --json "{}" --select <fields>
bh blueprinthelper_read_context --file <copied-template.json> --select <fields>
bh task preview --file <copied-template.json> --select <fields>
bh task execute --file <copied-template.json> --select <fields>
```

See [TaskSpec_CLI_QuickStart.md](TaskSpec_CLI_QuickStart.md) for command syntax and output rules.

PowerShell can corrupt inline JSON strings before the CLI receives them. For anything beyond `{}`, prefer `--file` or pipe JSON to `--stdin`:

```powershell
$json | bh blueprinthelper_read_context --stdin --format full
```

When a UE-bound command waits on the Bridge, the CLI emits keep-alive hints to `stderr` and keeps `stdout` reserved for the final JSON result. Agents should keep waiting on `waiting for UE Bridge response` hints unless the CLI exits.

## 6. Minimal Verification

Repository verification:

```cmd
.\install.cmd -ProjectFile <Project.uproject> -EngineRoot <UE root> -RunDiagnostics
```

Editor connection verification:

```powershell
bh blueprint_get_runtime_profile --json "{}" --select <fields>
```

ReadContext capability discovery is local to task-core and does not touch UE assets:

```powershell
'{}' | bh blueprinthelper_read_context_capabilities --stdin --select <fields>
```

Expected bridge facts are a completed status, reachable Bridge summary, and current write-permission summary.

## 7. First Safe Asset Read

Read compact Blueprint asset context:

```powershell
bh blueprinthelper_read_context --file <copied-template.json> --select <fields>
```

Read a graph as Markdown:

```powershell
bh blueprinthelper_read_context --file <copied-template.json> --select <fields>
```

Read the project-authored function/event chain from a known Blueprint entry:

```powershell
bh blueprinthelper_read_function_chain_context --file <copied-template.json> --select <fields>
```

Use current CLI discovery to locate the FunctionChain request template. The result is a compact index; use current CLI help/discovery for exact result fields.

Use the current CLI discovery output or per-command help to prepare JSON input, then pass it with `--file` instead of embedding complex JSON in PowerShell.

## 8. Safe Write Checklist

For ordinary Agent editor-asset mutations, use the TaskSpec-first flow:

- Confirm the Bridge is reachable.
- Run `bh blueprint_get_runtime_profile --json "{}" --select <fields>`.
- Run `bh blueprinthelper_read_context --file <copied-template.json> --select <fields>`.
- Produce the TaskSpec from the current CLI-discovered template, filling only the task-specific placeholders required by that template.
- Do not submit TaskPlan directly; it is produced by the canonical AgentFace task-core TypeScript compiler.
- Run `bh task preview --file <copied-template.json> --select <fields>` and stop on blocked / failed preview.
- Run `bh task execute --file <copied-template.json> --select <fields>` only after preview passes.
- Let UE Task Runtime handle TaskPlan execution, compile/save policy, transaction grouping, rollback, and diagnostics.

Low-level legacy/internal/debug/expert commands are not part of the supported Agent entry. Use current CLI discovery for supported ordinary Agent flows.
