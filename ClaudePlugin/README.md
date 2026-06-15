# BlueprintHelper

BlueprintHelper is an Unreal Engine editor plugin with a CLI-first TaskSpec transport and a global MCP allowlist companion. It lets an agent inspect and modify Unreal Editor assets through a local Bridge: Blueprint graphs, UMG widgets, DataAssets, DataTables, asset browser operations, compile/save/open commands, PIE commands, and related diagnostics.

BlueprintHelper is not a general source editing API. Use normal repository tools for C++, TypeScript, Python, JSON, config files, code search, build scripts, and documentation edits. The current Agent-facing surface is CLI-first for ordinary asset work; global MCP is retained only for editor open/close lifecycle in ordinary Agent workflows. Agents must not start or close Unreal Editor through CLI lifecycle aliases; if global lifecycle MCP is unavailable, report `lifecycle_mcp_unavailable`.

## TaskSpec-First Architecture

The current architecture uses a CLI-first task orchestration layer. Ordinary Agents author TaskSpec, not low-level Blueprint operation payloads:

```text
Agent -> BlueprintHelper CLI -> AgentFace task-core TypeScript compiler -> Bridge preview/execute/read -> UE Task Runtime -> Existing Capability Clusters
```

The intended default flow is:

```text
bh blueprint_get_runtime_profile
-> bh blueprinthelper_find_assets when the Unreal asset path is unknown
-> bh context read --file <read-spec.json> for the resolved asset or scoped graph context
-> Agent produces bare BlueprintHelper.TaskSpec.v1
-> bh task preview --file <task-spec.json>
-> bh task execute --file <task-spec.json> --preview-token <preview_token>
-> bh task result --id <task_run_id> when needed
```

Existing tool clusters are not removed. They remain as UE Task Runtime capabilities, debug / expert tools, and automation test entry points. See [BlueprintHelper_Hybrid_TaskSpec_TaskPlan_Architecture_20260504.md](../BlueprintHelper/Develop/v0.4.1/ArchivedReference/RetiredPlanDocs_20260517/BlueprintHelper_Hybrid_TaskSpec_TaskPlan_Architecture_20260504.md).

Agents submit `BlueprintHelper.TaskSpec.v1` only; they do not submit TaskPlan. The canonical AgentFace task-core TypeScript compiler owns TaskPlan generation, and UE Task Runtime executes the compiled TaskPlan.

TaskSpec compiler ownership: AgentFace task-core TypeScript compiler is the canonical production compiler. Legacy fallback and parity-gate paths are retired; new TaskSpec capabilities must be implemented and tested in TS first.

## Version

Current source metadata:

| Component | Current value |
|---|---|
| Unreal plugin `BlueprintHelper.uplugin` | `VersionName` 0.6.4 |
| CLI `AgentFaceService/cli/package.json` | 0.6.4 |
| Shared task core `AgentFaceService/task-core/package.json` | 0.6.4 |
| Global MCP allowlist package `AgentFaceService/mcp/package.json` | 0.6.4 |
| Documentation batch | 2026-05-17 implementation sync: CLI ordinary tools + MCP allowlist |
| Intended UE version | UE 5.3 or newer |

The documentation mainline targets the CLI-first TaskSpec / TaskPlan orchestration architecture, with global MCP restricted to editor open/close lifecycle in ordinary Agent workflows. Treat plugin version, CLI version, MCP package version, and documentation date as separate version sources as documented by the release package.

## Core Capabilities

| Area | Supported |
|---|---|
| Blueprint logic | Read LogicFlow, read LogicJson, export raw JSON, import raw JSON, import AgentImportGraph |
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

```cmd
.\install.cmd -ProjectFile <Project.uproject> -EngineRoot <UE root>
```

需要 Claude 插件支持时追加 `-InstallClaudePlugin`。安装器会写入 `%USERPROFILE%\.claude\settings.json`，注册本地 `ClaudePlugin` marketplace，并启用 `blueprint-helper@blueprint-helper-dev`；同时也会复制 Claude subagent 定义到用户目录。

Add `-InstallClaudePlugin` when you want the installer to write `%USERPROFILE%\.claude\settings.json`, register the local `ClaudePlugin` marketplace, and enable `blueprint-helper@blueprint-helper-dev`. This also copies the Claude subagent definitions into the user profile.

Use repository-root `uninstall.cmd` to remove installed Claude plugin entries, Claude sideAgents, Codex companion entries, and CLI links without deleting the source checkout.

交互式安装优先使用 Node.js 内置终端交互。复制 Claude sideAgent 定义前，三个 sideAgent 会以表格显示，模型和思考等级是独立字段；模型选项为 `haiku`、`sonnet`，思考等级为 `high`。非交互安装会自动使用推荐默认值，`task-worker` 为 `sonnet / high`。

Interactive install prefers Node.js built-in terminal prompts. Before copying Claude sideAgent definitions, the three sideAgents are shown in a table with separate model and reasoning fields; model options are `haiku` and `sonnet`, with reasoning `high`. Non-interactive install uses the recommended defaults automatically, with `task-worker` on `sonnet / high`.

安装器不依赖可调用的 Claude 插件 CLI 来完成注册；它会直接更新 `enabledPlugins` 和 `extraKnownMarketplaces`。

The installer does not depend on a callable Claude plugin CLI for this registration; it updates `enabledPlugins` and `extraKnownMarketplaces` directly.

Use `-InstallClaudeAgents` only when you want to copy the Claude subagent definitions without installing the Claude plugin.

`/blueprint-helper:setup` is retained only as a compatibility pointer to the repository-root `install.cmd`; it no longer owns first-run setup.

Detailed manual setup lives in [Docs/Install_CLI_QuickStart.md](../AgentFaceService/docs/Install_CLI_QuickStart.md). CLI command syntax and TaskSpec Template Composer usage live in [Docs/TaskSpec_CLI_QuickStart.md](../AgentFaceService/docs/TaskSpec_CLI_QuickStart.md). Agents should use CLI template discovery and composer output instead of scanning `AgentFaceService/agent-guide/Templates/` directly.

## Agent Entry Points

Agents should read these in order:

1. [AGENTS.md](AGENTS.md)
2. [AgentFaceService/agent-guide/00_Agent_Onboarding_Index.md](../AgentFaceService/agent-guide/00_Agent_Onboarding_Index.md)
3. [AgentFaceService/docs/CLI_Tools_API_Reference.md](../AgentFaceService/docs/CLI_Tools_API_Reference.md)
4. [Develop/Plan/README.md](../BlueprintHelper/Develop/Plan/README.md)
5. [Docs/TaskSpec_CLI_QuickStart.md](../AgentFaceService/docs/TaskSpec_CLI_QuickStart.md)
6. [Docs/CLI_Tools_API_Reference.md](../AgentFaceService/docs/CLI_Tools_API_Reference.md)

Claude-style agents can also load [skills/blueprint-helper/SKILL.md](skills/blueprint-helper/SKILL.md).

## Claude Plugin Commands

Claude Code discovers plugin commands from the plugin root `commands/` directory.

| Command | Purpose |
|---|---|
| `/blueprint-helper:setup` | Deprecated compatibility pointer to the root `install.cmd` |
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

1. Confirm Unreal Editor is running, or use the global MCP lifecycle tool after `<ProjectDir>/.blueprinthelper/project-profile.json` has `environment.ue_engine_dir`.
2. Confirm the Bridge is reachable.
3. Read runtime profile and the required ReadContext payload.
4. Produce an explicit `BlueprintHelper.TaskSpec.v1` from current CLI discovery and composer output.
5. Run preview and stop on schema, semantic, policy, capability, or dry-run blockers.
6. Execute only after preview passes.
7. Let UE Task Runtime manage TaskPlan execution, internal compile/save policy, transaction grouping, rollback, and diagnostics.
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
- LogicJson reads should report importability explicitly through fields such as `importable` and `schema`.

### MCP Lifecycle / Compatibility Behavior

- `blueprint_open_editor` and `blueprint_close_editor` are global MCP lifecycle companion commands for Agent-owned editor lifecycle. Compatibility also uses the global MCP lifecycle tools, not CLI lifecycle aliases. CLI lifecycle invocation is blocked for Agents.
- Deprecated MCP ordinary tools are not fallback paths; do not add or run tests for them.
- `blueprint_export_to_json` may still return `raw_json_ref` as a resource link in compatibility paths.
- RawJson resource handling remains for historical fixtures and recovery workflows.
- `legacy_text_json` is retired; use structured JSON or resource references.
