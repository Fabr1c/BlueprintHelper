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

The installer builds the Agent runtime, links `bh`, writes the Codex local marketplace and enabled plugin entries directly into `config.toml`, installs Codex subagents and the MCP allowlist entry, writes the project profile when project and UE root are known, creates `.blueprinthelper/AgentWorkFlow.md`, refreshes project-root `AGENTS.md` / `CLAUDE.md` markers, and creates default user preference files only when they are missing.

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
.\install.cmd -ProjectFile <Project.uproject> -EngineRoot <UE root> -SkipProjectUbtCompile
.\install.cmd -ProjectFile <Project.uproject> -EngineRoot <UE root> -ProjectEditorTarget <TargetName>
```

`install.cmd` opens the interactive installer when launched without arguments and passes arguments through to the underlying PowerShell installer when supplied. Prefer this entry in Agent-facing docs so users do not need to run `.ps1` directly.

By default, install runs one project UBT compile after plugin registration and optional engine-plugin copy:

```text
Build.bat <ProjectName>Editor Win64 Development -Project=<Project.uproject> -WaitMutex -NoHotReloadFromIDE
```

中文：默认安装会在插件注册和可选的 Engine 插件复制之后，对目标项目执行一次 UBT 编译，用来验证 UE 侧插件可以被项目加载和编译。明确不需要编译时再传 `-SkipProjectUbtCompile`。

English: the installer runs one UBT compile by default after plugin registration and optional engine-plugin copy. Pass `-SkipProjectUbtCompile` only when you intentionally want to skip this verification.

If the project uses a custom Editor target name or has multiple `*Editor.Target.cs` files, pass `-ProjectEditorTarget <TargetName>`. Otherwise the installer auto-detects `<ProjectName>Editor.Target.cs`, a single available `*Editor.Target.cs`, or falls back to `<ProjectName>Editor`.

The repository root keeps only `.cmd` user script entry points. The underlying PowerShell and Node implementation scripts live under `InstallScripts/`. Use `uninstall.cmd` to remove the global CLI link, plugin entries, installed subagents, and lifecycle MCP config.

After `npm link`, the installer removes npm-generated `bh.ps1` / `blueprinthelper-cli.ps1` shims when matching `.cmd` launchers exist. This avoids PowerShell ExecutionPolicy blocking `bh`. If an older install still resolves `bh` to a `.ps1` file, rerun the root installer or call `bh.cmd`.

## 3. Manual Project Profile Fallback

Use this only when the installer could not discover a unique `.uproject` or you intentionally passed `-SkipProjectProfile`.

Store UE version-specific configuration in `<ProjectDir>/.blueprinthelper/project-profile.json` only when this fallback is necessary. The root installer writes this automatically when `-ProjectFile` and `-EngineRoot` are supplied. Agent workflow rules should live in `<ProjectDir>/.blueprinthelper/AgentWorkFlow.md`; project-root `AGENTS.md` / `CLAUDE.md` should only keep the managed BlueprintHelper entry that points to that workflow document.

Bridge connectivity environment variables:

```powershell
$env:BRIDGE_HOST = "127.0.0.1"
$env:BRIDGE_PORT = "32147"
```

The default Bridge port is `32147`, below Windows' default dynamic TCP range. This avoids common Hyper-V / WSL / Docker excluded ranges that can make a port fail even when `netstat` shows no owning process. If a local machine still reserves that port, override both `runtime.bridge.port` and `BRIDGE_PORT` with the same free port.

Project `.uproject` paths should not be stored globally. Agents discover the target `.uproject` from the current workspace; explicit `project_file` remains available for ambiguous workspaces.

## 4. Start Unreal Editor

Either start Unreal Editor normally with the project, or use the global MCP lifecycle tools after the project profile has `environment.ue_engine_dir`. `blueprint_open_editor` / `blueprint_close_editor` are not CLI lifecycle compatibility aliases; Agent-owned editor lifecycle uses the global MCP lifecycle tools:

```text
mcp__blueprint_helper__blueprint_open_editor
mcp__blueprint_helper__blueprint_close_editor
```

Do not use plugin-local MCP or deprecated MCP ordinary tools as proof of Agent lifecycle or asset-workflow behavior. The normal Agent-owned lifecycle path is the global MCP allowlist server.

Do not start or close Unreal Editor through CLI lifecycle aliases. `bh open_editor`, `bh close_editor`, `blueprint_open_editor`, and `blueprint_close_editor` are not Agent lifecycle execution paths; if lifecycle MCP is unavailable, report `lifecycle_mcp_unavailable`.

Bridge smoke check:

```powershell
Test-NetConnection 127.0.0.1 -Port 32147
```

If the port is not open, wait for the editor to finish loading, confirm the plugin is enabled, and check the Unreal output log.

## 5. Run The CLI

The CLI is the supported Agent entry for ordinary TaskSpec writes, reads, diagnostics, debug summaries, write-session requests, and result queries. MCP is restricted to editor open/close lifecycle in ordinary Agent workflows; lifecycle commands are removed from the ordinary CLI surface, and accidental lifecycle CLI calls may return a removed-command hint that points to the global MCP lifecycle tool. Deprecated MCP ordinary tools are not fallback paths.

Examples:

```powershell
bh blueprint_get_runtime_profile --json "{}" --select <fields>
bh context read --file <read-spec.json> --select <fields>
bh task preview --file <generated-task-spec.json> --select <fields>
bh task execute --file <generated-task-spec.json> --select <fields>
```

See [TaskSpec_CLI_QuickStart.md](TaskSpec_CLI_QuickStart.md) for command syntax and output rules.

PowerShell-safe input rule: use `--file` for reusable JSON and `--stdin` for generated JSON. Avoid inline `--json $json` for non-trivial payloads because PowerShell can strip quotes before Node receives the argument.

Current BlueprintHelper JSON readers tolerate one leading UTF-8 BOM on input. The installer still writes project profile JSON as UTF-8 without BOM.

Generated ReadSpec JSON can be piped through the grouped command:

```powershell
$json | bh context read --stdin --format full
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
bh context read --file <read-spec.json> --select <fields>
```

Read graph logic as `logic_flow` or `logic_json` according to the ReadSpec view:

```powershell
bh context read --file <read-spec.json> --select <fields>
```

Read the project-authored function/event chain from a known Blueprint entry:

```powershell
bh blueprinthelper_read_function_chain_context --file <function-chain-request.json> --select <fields>
```

Use current CLI discovery to locate the FunctionChain request template. The result is a compact index; use current CLI help/discovery for exact result fields.

Use the current CLI discovery output or per-command help to prepare JSON input, then pass it with `--file` instead of embedding complex JSON in PowerShell.

TaskSpec write templates are generated through the TaskSpec Template Composer four-layer index:

```powershell
bh tools templates families --workflow preview_execute --format json
bh tools templates write-modes --family graph_write --format json
bh tools templates clusters --family graph_write --format json
bh tools templates operations --family graph_write --cluster generic_ops --write-mode graph.append --format json
bh tools templates quick-access --family graph_write --cluster generic_ops --operation let --write-mode graph.append --format json
bh tools templates quick-access --family graph_write --cluster generic_ops --operation expression --write-mode graph.append --format json
bh tools templates compose --family graph_write --write-mode graph.append --templates "generic_ops.let.default(generic_ops.expression.literal)" --out .tmp\taskspec-template-composer\graph_append.taskspec.json --format json
```

Do not use old tool-id template dispatch or scan template directories to choose TaskSpec files. For GraphWrite, use `quick-access.items[].slot_type` to keep expression templates nested and `quick-access.items[].arg_slots` to fill `template_id(...)` positions. Fill the generated TaskSpec with evidence from ReadContext, then preview and execute that generated file.

## 8. Safe Write Checklist

For ordinary Agent editor-asset mutations, use the TaskSpec-first flow:

- Confirm the Bridge is reachable.
- Run `bh blueprint_get_runtime_profile --json "{}" --select <fields>`.
- Run `bh context read --file <read-spec.json> --select <fields>`.
- Produce the TaskSpec through `bh tools templates ... compose`, filling only the task-specific placeholders required by the generated file.
- Do not submit TaskPlan directly; it is produced by the canonical AgentFace task-core TypeScript compiler.
- Run `bh task preview --file <generated-task-spec.json> --select <fields>` and stop on blocked / failed preview.
- Run `bh task execute --file <generated-task-spec.json> --select <fields>` only after preview passes.
- Let UE Task Runtime handle TaskPlan execution, compile/save policy, transaction grouping, rollback, and diagnostics.

Low-level legacy/internal/debug/expert commands are not part of the supported Agent entry. Use current CLI discovery for supported ordinary Agent flows.
