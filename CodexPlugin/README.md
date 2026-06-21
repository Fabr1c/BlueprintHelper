# BlueprintHelper Codex Plugin

This is a Codex-compatible package for BlueprintHelper.

For normal user setup, run the repository-root installer:

```cmd
.\install.cmd
```

See `INSTALL.md` at the repository root for Codex Desktop, CLI, lifecycle MCP, and UE engine-install options.

Use repository-root `uninstall.cmd` to remove installed Codex plugin entries, Codex subagents, lifecycle MCP config, and CLI links without deleting the source checkout.

交互式安装优先使用 Node.js 内置终端交互。安装 Codex subagents 时，四个 agent 会以表格显示，模型和思考等级是独立字段；模型选项为 `gpt-5.4-mini`、`gpt-5.3-codex-spark`、`gpt-5.5`、`gpt-5.4`，思考等级为 `high`、`xhigh`。

Interactive install prefers Node.js built-in terminal prompts. When Codex subagents are selected, the four agents are shown in a table with separate model and reasoning fields; model options are `gpt-5.4-mini`, `gpt-5.3-codex-spark`, `gpt-5.5`, and `gpt-5.4`, with reasoning `high` or `xhigh`.

根安装脚本会解析 `C:\Users\<username>` 下的真实 Windows 用户目录，直接写入用户 Codex 配置，将仓库根目录注册为本地 marketplace，并启用 `blueprint-helper@blueprint-helper-local`。

The root installer resolves the real Windows user profile under `C:\Users\<username>`, writes the user Codex config directly, registers the repository root as a local marketplace, and enables `blueprint-helper@blueprint-helper-local`.

```text
C:\Users\<username>\.codex\config.toml
```

## Contents

- `.codex-plugin/plugin.json` is the Codex plugin manifest.
- `agents/` contains the Codex `.toml` subagent definitions and Claude `.md` sideAgent definitions used by the BlueprintHelper workflow.
- `skills/blueprint-helper/SKILL.md` is the Codex-facing workflow entry.
- `skills/blueprint-helper/references/` mirrors the BlueprintHelper agent references from the canonical `AgentFaceService/agent-guide` docs.
- `assets/blueprint-helper.svg` is the local plugin icon referenced by the manifest.

## Runtime Model

The active Agent-facing transport for ordinary TaskSpec reads and writes is the BlueprintHelper CLI. The global MCP endpoint is an allowlist for editor lifecycle only in ordinary Agent workflows:

```text
mcp__blueprint_helper__blueprint_lifecycle_mcp_status
mcp__blueprint_helper__blueprint_open_editor
mcp__blueprint_helper__blueprint_close_editor
mcp__blueprint_helper__blueprint_dismiss_editor_dialogs
mcp__blueprint_helper__blueprint_close_editor_dialogs
```

Only the Main Agent may call MCP lifecycle tools. Subagents must not call MCP tools.

Agents must not start, close, or dismiss Unreal Editor modal dialogs through CLI lifecycle aliases (`bh open_editor`, `bh close_editor`, `blueprint_open_editor`, `blueprint_close_editor`, `blueprint_dismiss_editor_dialogs`, or `blueprint_close_editor_dialogs`). If the global MCP lifecycle tools are unavailable, report `lifecycle_mcp_unavailable` instead of using a CLI fallback.

## Mandatory Subagent Workflow

When a request involves Blueprint, UMG, DataAsset, DataTable, Bridge/runtime, preview, execute, compile, save, or other UE editor asset work, Codex must use the BlueprintHelper subagent workflow:

```text
Main Agent preflight
-> practical C++ plus Blueprint architecture gate
-> at most one blueprint-explorer
-> at most one sourcecode-explorer
-> sourcecode-worker when source implementation is required
-> task-worker template-first TaskSpec construction
-> preview
-> write session when needed
-> execute
-> result filtering
-> Main Agent closed-loop decision
```

Subagents:

```text
blueprint-explorer   Collects Blueprint/UMG/DataAsset/DataTable/editor-asset context.
sourcecode-explorer  Collects repository source-code/schema/template context.
sourcecode-worker    Implements architecture-approved C++/DataAsset/interface source changes and verifies them.
task-worker          Constructs TaskSpec from templates, runs preview/execute, and returns concise diagnostics.
```

Install the subagent definitions globally from the source checkout:

```powershell
node <BLUEPRINTHELPER_ROOT>\CodexPlugin\scripts\install-codex-agents.cjs
```

默认推荐配置：`blueprint-explorer` 使用 `gpt-5.4-mini / high`，`sourcecode-explorer` 使用 `gpt-5.3-codex-spark / xhigh`，`sourcecode-worker` 使用 `gpt-5.5 / xhigh`，`task-worker` 使用 `gpt-5.4 / high`。

Recommended defaults: `blueprint-explorer` uses `gpt-5.4-mini / high`; `sourcecode-explorer` uses `gpt-5.3-codex-spark / xhigh`; `sourcecode-worker` uses `gpt-5.5 / xhigh`; `task-worker` uses `gpt-5.4 / high`.

## CLI Entry

Use either `bh` on PATH or the built CLI entry:

```powershell
bh blueprint_get_runtime_profile --json "{}" --select status,summary
node <BLUEPRINTHELPER_ROOT>\AgentFaceService\cli\build\cli\index.js blueprint_get_runtime_profile --json "{}" --select status,summary
```

The root installer removes npm-generated PowerShell `.ps1` shims when `.cmd` launchers are present, so `bh` is not blocked by ExecutionPolicy. For generated JSON in PowerShell, pipe to `--stdin` or use `--file`; avoid inline `--json $json`.

## Global MCP Lifecycle

Plugin-local MCP is not the normal Codex entry. Do not register or call ordinary BlueprintHelper read/write tools through MCP. Use MCP only for editor open/close/modal-dismiss lifecycle in ordinary Agent workflows; use CLI for ordinary reads, diagnostics, TaskSpec preview, write-session requests, execute, and result lookup. Do not use CLI lifecycle aliases for editor startup/shutdown or modal dismissal. Deprecated MCP ordinary tools are not fallback paths.

For editor-asset writes, keep the workflow TaskSpec-first. Use CLI template discovery and the TaskSpec Template Composer to generate JSON inputs; do not scan `AgentFaceService/agent-guide/Templates/` directly for tool selection.

