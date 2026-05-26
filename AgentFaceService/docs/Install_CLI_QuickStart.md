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

The installer builds the Agent runtime, links `bh`, registers the repository local marketplace through the official Codex plugin install entry, installs `blueprint-helper@blueprint-helper-local`, installs Codex subagents and the MCP allowlist entry, writes the project agent profile when project and UE root are known, and creates default user preference files only when they are missing.

交互式安装会在安装 Codex subagents 或 Claude sideAgents 时显示模型/思考等级表单。Codex 只显示推荐组合 `gpt-5.4-mini / high`、`gpt-5.3-codex-spark / xhigh` 和 `gpt-5.4 / high`；Claude 只显示推荐组合 `haiku / high` 与 `sonnet / high`。非交互安装自动使用推荐默认值，`task-worker` 默认更强模型。

Interactive install shows model/reasoning forms when installing Codex subagents or Claude sideAgents. Codex only shows the recommended `gpt-5.4-mini / high`, `gpt-5.3-codex-spark / xhigh`, and `gpt-5.4 / high` profiles; Claude only shows the recommended `haiku / high` and `sonnet / high` profiles. Non-interactive install uses the recommended defaults automatically, with a stronger default model for `task-worker`.

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

After `npm link`, the installer removes npm-generated `bh.ps1` / `blueprinthelper-cli.ps1` shims when matching `.cmd` launchers exist. This avoids PowerShell ExecutionPolicy blocking `bh`. If an older install still resolves `bh` to a `.ps1` file, rerun the root installer or call `bh.cmd`.

## 3. Manual Project Agent Profile Fallback

Use this only when the installer could not discover a unique `.uproject` or you intentionally passed `-SkipProjectProfile`.

Store UE version-specific configuration in the project profile:

```json
{
  "environment": {
    "ue_version": "5.6",
    "ue_engine_dir": "<UE_ENGINE_ROOT>"
  }
}
```

Save this file as `<ProjectDir>/.blueprinthelper/agent-profile.json`. The root installer writes this automatically when `-ProjectFile` and `-EngineRoot` are supplied.

Bridge connectivity environment variables:

```powershell
$env:BRIDGE_HOST = "127.0.0.1"
$env:BRIDGE_PORT = "54321"
```

Project `.uproject` paths should not be stored globally. Agents discover the target `.uproject` from the current workspace; explicit `project_file` remains available for ambiguous workspaces.

## 4. Start Unreal Editor

Either start Unreal Editor normally with the project, or use the global MCP lifecycle tools after the project agent profile has `environment.ue_engine_dir`. Compatibility for `blueprint_open_editor` / `blueprint_close_editor` also uses the global MCP lifecycle tools, not CLI lifecycle aliases:

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
bh blueprint_get_runtime_profile --json "{}" --select status,summary
bh blueprinthelper_read_context --file .\read-spec.json --select status,summary,artifacts.full_result
bh task preview --file .\task_spec.json --select status,preview_id,summary,artifacts.full_result
bh task execute --file .\task_spec.json --select status,task_run_id,summary
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
bh blueprint_get_runtime_profile --json "{}" --select status,summary
```

ReadContext capability discovery is local to task-core and does not touch UE assets:

```powershell
'{}' | bh blueprinthelper_read_context_capabilities --stdin --select status,artifacts.full_result
```

Expected bridge facts:

```json
{
  "status": "completed",
  "summary": {
    "bridge": {
      "reachable": true
    },
    "write_permission": {
      "enabled": true
    }
  }
}
```

## 7. First Safe Asset Read

Read compact Blueprint asset context:

```powershell
bh blueprinthelper_read_context --file .\read_blueprint_summary.json --select status,summary,artifacts.full_result
```

Read a graph as Markdown:

```powershell
bh blueprinthelper_read_context --file .\read_eventgraph_logic_md.json --select status,summary,artifacts.full_result
```

Read the project-authored function/event chain from a known Blueprint entry:

```powershell
bh blueprinthelper_read_function_chain_context --file .\function_chain.json --select status,summary,artifacts.full_result
```

Minimal `function_chain.json`:

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

The result is a compact `FunctionChainContext.v1` index. Follow `custom_logic_refs[]` with scoped `blueprinthelper_read_context` calls when the function body is needed.

Copy matching inputs from `AgentFaceService/agent-guide/Templates/read/` and edit placeholders instead of embedding complex JSON in PowerShell.

## 8. Safe Write Checklist

For ordinary Agent editor-asset mutations, use the TaskSpec-first flow:

- Confirm the Bridge is reachable.
- Run `bh blueprint_get_runtime_profile --json "{}" --select status,summary`.
- Run `bh blueprinthelper_read_context --file .\read-spec.json --select status,summary,artifacts.full_result`.
- Produce `BlueprintHelper.TaskSpec.v1` with exact `asset_path`, target graph when relevant, allowed scope, resource references, failure policy, `validation.should_compile`, and `validation.should_save`.
- Do not submit TaskPlan directly; it is produced by the canonical AgentFace task-core TypeScript compiler.
- Run `bh task preview --file .\task_spec.json --select status,preview_id,summary,artifacts.full_result` and stop on blocked / failed preview.
- Run `bh task execute --file .\task_spec.json --select status,task_run_id,summary` only after preview passes.
- Let UE Task Runtime handle TaskPlan execution, compile/save policy, transaction grouping, rollback, and diagnostics.

Low-level legacy/internal/debug/expert commands are documented in [CLI_Tools_API_Reference.md](CLI_Tools_API_Reference.md), but they are not part of the supported Agent entry.
