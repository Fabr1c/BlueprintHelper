# BlueprintHelper

BlueprintHelper is an Unreal Engine editor plugin with a CLI-first TaskSpec transport and a global MCP allowlist companion. It lets an agent inspect and modify Unreal Editor assets through a local Bridge: Blueprint graphs, UMG widgets, DataAssets, DataTables, asset browser operations, compile/save/open commands, PIE commands, and related diagnostics.

BlueprintHelper is not a general source editing API. Use normal repository tools for C++, TypeScript, Python, JSON, config files, code search, build scripts, and documentation edits. The current Agent-facing surface is CLI-first for ordinary asset work; global MCP is retained only for editor open/close lifecycle in ordinary Agent workflows.

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
-> bh task preview
-> bh task execute
-> bh blueprinthelper_get_task_result when needed
```

Existing tool clusters are not removed. They remain as UE Task Runtime capabilities, debug / expert tools, and automation test entry points. See [BlueprintHelper_Hybrid_TaskSpec_TaskPlan_Architecture_20260504.md](../BlueprintHelper/Develop/Plan/BlueprintHelper_Hybrid_TaskSpec_TaskPlan_Architecture_20260504.md).

Agents submit `BlueprintHelper.TaskSpec.v1` only; they do not submit TaskPlan. task-core dispatches to the Python compiler, and UE Task Runtime executes the compiled TaskPlan.

## Version

Current source metadata:

| Component | Current value |
|---|---|
| Unreal plugin `BlueprintHelper.uplugin` | `VersionName` 0.4.4 |
| CLI `AgentFaceService/cli/package.json` | 0.4.4 |
| Shared task core `AgentFaceService/task-core/package.json` | 0.4.4 |
| Global MCP allowlist package `AgentFaceService/mcp/package.json` | 0.4.4 |
| Documentation batch | 2026-05-17 implementation sync: CLI ordinary tools + MCP allowlist |
| Intended UE version | UE 5.3 or newer |

The documentation mainline targets the CLI-first TaskSpec / TaskPlan orchestration architecture, with global MCP restricted to editor open/close lifecycle in ordinary Agent workflows. Treat plugin version, CLI version, MCP package version, and documentation date as separate version sources as documented by the release package.

## Core Capabilities

| Area | Supported |
|---|---|
| Blueprint logic | Read LogicMD, read LogicJson, export raw JSON, import raw JSON, import AgentImportGraph |
| Blueprint structure | List graphs, variables, dispatchers, add/remove variables, add/remove graphs, add dispatchers, delete nodes |
| UMG | Read widget trees, add/remove/move widgets, read/set widget properties |
| Data assets | Read/set editable UObject properties |
| DataTables | Read, add, update, delete rows |
| Editor commands | Compile/save/open assets, create Blueprint, PIE start/stop, undo/redo, console commands |
| Local process commands | Use normal repository tools for source/config/docs work; Editor open/close is handled through global MCP lifecycle tools when Agent-controlled |

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

Run the repository-root installer first:

```powershell
.\install.ps1 -ProjectFile <Project.uproject> -EngineRoot <UE root>
```

Add `-InstallClaudePlugin` when you want the installer to register the Claude marketplace through the official Claude plugin entry and install `blueprint-helper@blueprint-helper-dev`. This also copies the Claude subagent definitions into the user profile.

If no callable Claude plugin CLI is available, the installer prints the official commands to run inside Claude Code:

```text
/plugin marketplace add <BlueprintHelper repository root>\ClaudePlugin
/plugin install blueprint-helper@blueprint-helper-dev
```

Use `-InstallClaudeAgents` only when you want to copy the Claude subagent definitions without installing the Claude plugin.

`/blueprint-helper:setup` is retained only as a compatibility pointer to `install.ps1`; it no longer owns first-run setup.

Detailed manual setup lives in [Docs/Install_CLI_QuickStart.md](../AgentFaceService/docs/Install_CLI_QuickStart.md). CLI command syntax lives in [Docs/TaskSpec_CLI_QuickStart.md](../AgentFaceService/docs/TaskSpec_CLI_QuickStart.md). Copy-and-edit JSON templates live in [AgentFaceService/agent-guide/Templates](../AgentFaceService/agent-guide/Templates/README.md).

## Agent Entry Points

Agents should read these in order:

1. [AGENTS.md](AGENTS.md)
2. [AgentFaceService/agent-guide/00_Agent_Onboarding_Index_20260504.md](../AgentFaceService/agent-guide/00_Agent_Onboarding_Index_20260504.md)
3. [AgentFaceService/docs/CLI_Tools_API_Reference.md](../AgentFaceService/docs/CLI_Tools_API_Reference.md)
4. [Develop/Plan/README.md](../BlueprintHelper/Develop/Plan/README.md)
5. [Docs/TaskSpec_CLI_QuickStart.md](../AgentFaceService/docs/TaskSpec_CLI_QuickStart.md)
6. [Docs/CLI_Tools_API_Reference.md](../AgentFaceService/docs/CLI_Tools_API_Reference.md)

Claude-style agents can also load [skills/blueprint-helper/SKILL.md](skills/blueprint-helper/SKILL.md).

## Claude Plugin Commands

Claude Code discovers plugin commands from the plugin root `commands/` directory.

| Command | Purpose |
|---|---|
| `/blueprint-helper:setup` | Deprecated compatibility pointer to the root `install.ps1` |
| `/blueprint-helper:configure` | Update user preferences and active safety profile after installation |

## User Documentation

| Document | Purpose |
|---|---|
| [Docs/TaskSpec_CLI_QuickStart.md](../AgentFaceService/docs/TaskSpec_CLI_QuickStart.md) | Primary CLI transport for compact TaskSpec execution |
| [Docs/Install_CLI_QuickStart.md](../AgentFaceService/docs/Install_CLI_QuickStart.md) | CLI installation and first-run setup |
| [Docs/CLI_Tools_API_Reference.md](../AgentFaceService/docs/CLI_Tools_API_Reference.md) | Supported CLI command surface and internal/debug compatibility notes |
| [AgentFaceService/agent-guide/](../AgentFaceService/agent-guide/) | Agent task routing and editor-asset workflows |
| [Develop/Plan/README.md](../BlueprintHelper/Develop/Plan/README.md) | Active implementation and verification plan index |
| [Develop/v0.3.8/README.md](../BlueprintHelper/Develop/v0.3.8/README.md) | Sealed v0.3.8 documentation archive |
| [Resources/KnownBugs.md](../BlueprintHelper/Resources/KnownBugs.md) | Known bugs and implementation notes |

## Safe Write Workflow

For ordinary Agent editor-asset mutations, use the TaskSpec-first loop:

1. Confirm Unreal Editor is running, or use the global MCP lifecycle tool after `<ProjectDir>/.blueprinthelper/agent-profile.json` has `environment.ue_engine_dir`.
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
- Retired string-first fields such as `json_text` are no longer emitted or accepted.

### Response Output Rules

- Prefer object `json` over stringified JSON.
- LogicJson and LogicMD reads should report importability explicitly through fields such as `importable` and `schema`.

### MCP Lifecycle / Compatibility Behavior

- `blueprint_open_editor` and `blueprint_close_editor` are global MCP lifecycle companion commands for Agent-owned editor lifecycle. CLI lifecycle aliases are compatibility/manual fallback only.
- Deprecated MCP ordinary tools are not fallback paths; do not add or run tests for them.
- `blueprint_export_to_json` may still return `raw_json_ref` as a resource link in compatibility paths.
- RawJson resource handling remains for historical fixtures and recovery workflows.
- `legacy_text_json` is retired; use structured JSON or resource references.
