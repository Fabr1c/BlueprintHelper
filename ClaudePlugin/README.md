# BlueprintHelper

BlueprintHelper is an Unreal Engine editor plugin with a CLI-first Agent transport. It lets an agent inspect and modify Unreal Editor assets through a local Bridge: Blueprint graphs, UMG widgets, DataAssets, DataTables, asset browser operations, compile/save/open commands, PIE commands, and related diagnostics.

BlueprintHelper is not a general source editing API. Use normal repository tools for C++, TypeScript, Python, JSON, config files, code search, build scripts, and documentation edits. The deprecated MCP endpoint is compatibility/debug surface only and is not a supported Agent entry.

## TaskSpec-First Architecture

The current architecture uses a CLI-first task orchestration layer. Ordinary Agents author TaskSpec, not low-level Blueprint operation payloads:

```text
Agent -> BlueprintHelper CLI -> task-core -> Python Task Compiler -> Bridge preview/execute/read -> UE Task Runtime -> Existing Capability Clusters
```

The intended default flow is:

```text
bh blueprint_get_runtime_profile
-> bh blueprinthelper_read_task_context
-> Agent produces BlueprintHelper.TaskSpec.v1
-> bh blueprinthelper_preview_task
-> bh blueprinthelper_execute_task
-> bh blueprinthelper_get_task_result when needed
```

Existing tool clusters are not removed. They remain as UE Task Runtime capabilities, debug / expert tools, and automation test entry points. See [BlueprintHelper_Hybrid_TaskSpec_TaskPlan_Architecture_20260504.md](../BlueprintHelper/Develop/Plan/BlueprintHelper_Hybrid_TaskSpec_TaskPlan_Architecture_20260504.md).

Agents submit `BlueprintHelper.TaskSpec.v1` only; they do not submit TaskPlan. task-core dispatches to the Python compiler, and UE Task Runtime executes the compiled TaskPlan.

## Version

Current source metadata:

| Component | Current value |
|---|---|
| Unreal plugin `BlueprintHelper.uplugin` | `VersionName` 0.3.8 |
| CLI `AgentFaceService/cli/package.json` | 0.3.8 |
| Shared task core `AgentFaceService/task-core/package.json` | 0.3.8 |
| Deprecated MCP package `AgentFaceService/mcp/package.json` | 0.3.8 |
| Documentation batch | 2026-05-12 CLI-first TaskSpec mainline |
| Intended UE version | UE 5.3 or newer |

The documentation mainline targets the CLI-first TaskSpec / TaskPlan orchestration architecture. Treat plugin version, CLI version, deprecated MCP package version, and documentation date as separate version sources until the compatibility matrix is completed.

## Core Capabilities

| Area | Supported |
|---|---|
| Blueprint logic | Read LogicMD, read LogicJson, export raw JSON, import raw JSON, import AgentImportGraph |
| Blueprint structure | List graphs, variables, dispatchers, add/remove variables, add/remove graphs, add dispatchers, delete nodes |
| UMG | Read widget trees, add/remove/move widgets, read/set widget properties |
| Data assets | Read/set editable UObject properties |
| DataTables | Read, add, update, delete rows |
| Editor commands | Compile/save/open assets, create Blueprint, PIE start/stop, undo/redo, console commands |
| Local process commands | Open Unreal Editor, build the Unreal project |

## Boundaries

Use BlueprintHelper CLI / task-core tools for editor assets through the running Unreal Editor.

Do not use BlueprintHelper tools for:

- C++ source edits.
- TypeScript / CLI / task-core source edits.
- Python, JSON, config, or build script edits.
- General file-system search.
- General source-code search.

For repository work, use normal shell and editor tools. For editor assets, use CLI commands after Bridge preflight.

## Quick Install Path

### Claude Code Plugin Install

1. Add the marketplace source and install the plugin:

```text
/plugin marketplace add <your-git-remote-or-local-path>
/plugin install blueprint-helper@blueprint-helper-dev
```

2. Restart Claude Code.

3. Configure the project agent profile.

Store the UE version and engine root in the project agent profile:

```json
{
  "environment": {
    "ue_version": "5.6",
    "ue_engine_dir": "<UE_ENGINE_ROOT>"
  }
}
```

Save this under `<ProjectDir>/.blueprinthelper/agent-profile.json`. Project `.uproject` paths are not stored in Claude global settings. Agents discover the target `.uproject` from the current workspace and pass it as the explicit `project_file` tool argument when launching or building a project.

4. Build the shared task core and CLI packages:

```powershell
cd <PLUGIN_ROOT>\AgentFaceService\task-core
npm install
npm run build

cd <PLUGIN_ROOT>\AgentFaceService\cli
npm install
npm run build
```

5. Start Unreal Editor with the target project and confirm the Bridge is reachable at `127.0.0.1:54321`.

### Manual CLI Setup

1. Put this plugin under a UE project plugin directory, for example `YourProject/Plugins/BlueprintHelper`.
2. Enable the plugin in Unreal Editor.
3. Build the project if Unreal asks for a rebuild.
4. Build the CLI package. If you installed BlueprintHelper from an Unreal `BuildPlugin` package, keep the sibling `AgentFaceService` package available separately; UE packaging does not compile or include `AgentFaceService/cli`.

```powershell
cd <PLUGIN_ROOT>\AgentFaceService\task-core
npm install
npm run build

cd <PLUGIN_ROOT>\AgentFaceService\cli
npm install
npm run build
```

5. Set Bridge connection environment variables when the defaults are not enough:

```powershell
$env:BRIDGE_HOST = "127.0.0.1"
$env:BRIDGE_PORT = "54321"
```

Store the UE engine root in `<ProjectDir>/.blueprinthelper/agent-profile.json` as `environment.ue_engine_dir`; do not put UE version-specific project configuration in global Claude settings.

6. Start Unreal Editor with the project, then call the CLI:

```powershell
node <PLUGIN_ROOT>\AgentFaceService\cli\build\cli\index.js bridge ping
```

For full CLI usage, read [Docs/TaskSpec_CLI_QuickStart.md](../BlueprintHelper/Docs/TaskSpec_CLI_QuickStart.md) and [Docs/Install_CLI_QuickStart.md](../BlueprintHelper/Docs/Install_CLI_QuickStart.md).

## Agent Entry Points

Agents should read these in order:

1. [AGENTS.md](AGENTS.md)
2. [Resources/AgentGuide/00_Agent_Onboarding_Index_20260504.md](../BlueprintHelper/Resources/AgentGuide/00_Agent_Onboarding_Index_20260504.md)
3. [Resources/Docs/TaskSpec_TaskPlan_Contract_20260504.md](../BlueprintHelper/Resources/Docs/TaskSpec_TaskPlan_Contract_20260504.md)
4. [Develop/Plan/README.md](../BlueprintHelper/Develop/Plan/README.md)
5. [Docs/TaskSpec_CLI_QuickStart.md](../BlueprintHelper/Docs/TaskSpec_CLI_QuickStart.md)
6. [Docs/CLI_Tools_API_Reference.md](../BlueprintHelper/Docs/CLI_Tools_API_Reference.md)

Claude-style agents can also load [skills/blueprint-helper/SKILL.md](skills/blueprint-helper/SKILL.md).

## Claude Plugin Commands

Claude Code discovers plugin commands from the plugin root `commands/` directory.

| Command | Purpose |
|---|---|
| `/blueprint-helper:setup` | Initial setup: UE paths, CLI build, Bridge check, first-run preferences, SetupProfile |
| `/blueprint-helper:configure` | Update user preferences and active safety profile after setup |

## User Documentation

| Document | Purpose |
|---|---|
| [Docs/TaskSpec_CLI_QuickStart.md](../BlueprintHelper/Docs/TaskSpec_CLI_QuickStart.md) | Primary CLI transport for compact TaskSpec execution |
| [Docs/Install_CLI_QuickStart.md](../BlueprintHelper/Docs/Install_CLI_QuickStart.md) | CLI installation and first-run setup |
| [Docs/CLI_Tools_API_Reference.md](../BlueprintHelper/Docs/CLI_Tools_API_Reference.md) | Supported CLI command surface and internal/debug compatibility notes |
| [Resources/AgentGuide/](../BlueprintHelper/Resources/AgentGuide/) | Agent task routing and editor-asset workflows |
| [Develop/Plan/README.md](../BlueprintHelper/Develop/Plan/README.md) | Active implementation and verification plan index |
| [Develop/v0.3.8/README.md](../BlueprintHelper/Develop/v0.3.8/README.md) | Sealed v0.3.8 documentation archive |
| [Resources/KnownBugs.md](../BlueprintHelper/Resources/KnownBugs.md) | Known bugs and implementation notes |

## Safe Write Workflow

For ordinary Agent editor-asset mutations, use the TaskSpec-first loop:

1. Confirm Unreal Editor is running, or `<ProjectDir>/.blueprinthelper/agent-profile.json` has `environment.ue_engine_dir` and the target `.uproject` can be passed as `project_file`.
2. Confirm the Bridge is reachable.
3. Read runtime profile and TaskContextPack.
4. Produce an explicit `BlueprintHelper.TaskSpec.v1` with `validation.should_compile` and `validation.should_save`.
5. Run preview and stop on schema, semantic, policy, capability, or dry-run blockers.
6. Execute only after preview passes.
7. Let UE Task Runtime manage TaskPlan execution, `execution_policy.should_compile` / `execution_policy.should_save`, transaction grouping, rollback, and diagnostics.
8. Report task-level results without repeated blind retries.

Do not rely on the currently focused editor tab for destructive operations unless the user explicitly asks for active-context editing.

## Bridge Payload Compatibility (v2.2+)

BlueprintHelper Bridge uses object-first responses. Large raw graph payloads should stay in resource/artifact references instead of routine Agent stdout.

### Response Input Shape

- `payload` carries structured object data when available.
- `json` may carry structured object data for compatibility paths.
- `json_text` is emitted only when a command explicitly requests legacy text JSON.

### Response Output Rules

- Prefer object `json` over stringified JSON.
- LogicJson and LogicMD reads should report importability explicitly through fields such as `importable` and `schema`.

### Deprecated MCP Compatibility Behavior

- `blueprint_export_to_json` may still return `raw_json_ref` as a resource link in deprecated compatibility paths.
- RawJson resource handling remains for historical fixtures and recovery workflows.
- `legacy_text_json` exists only for compatibility with older text-only callers.

