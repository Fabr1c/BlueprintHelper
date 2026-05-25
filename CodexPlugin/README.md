# BlueprintHelper Codex Plugin

This is a Codex-compatible package for BlueprintHelper.

For normal user setup, run the repository-root installer:

```cmd
.\install.cmd
```

See `INSTALL.md` at the repository root for Codex Desktop, CLI, lifecycle MCP, and UE engine-install options.

交互式安装会在安装 Codex subagents 时显示模型/思考等级表单，只列出推荐组合 `gpt-5.4-mini / high`、`gpt-5.3-codex-spark / xhigh` 和 `gpt-5.4 / high`。

Interactive install shows a Codex subagent model/reasoning form and only lists the recommended `gpt-5.4-mini / high`, `gpt-5.3-codex-spark / xhigh`, and `gpt-5.4 / high` profiles.

The root installer uses the official Codex plugin install entry for this local marketplace:

```text
plugin marketplace add <BLUEPRINTHELPER_ROOT>
plugin install blueprint-helper@blueprint-helper-local
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
mcp__blueprint_helper__blueprint_open_editor
mcp__blueprint_helper__blueprint_close_editor
```

Only the Main Agent may call MCP lifecycle tools. Subagents must not call MCP tools.

## Mandatory Subagent Workflow

When a request involves Blueprint, UMG, DataAsset, DataTable, Bridge/runtime, preview, execute, compile, save, or other UE editor asset work, Codex must use the BlueprintHelper subagent workflow:

```text
Main Agent preflight
-> at most one blueprint-explorer
-> at most one sourcecode-explorer
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
task-worker          Constructs TaskSpec from templates, runs preview/execute, and returns concise diagnostics.
```

Install the subagent definitions globally from the source checkout:

```powershell
node <BLUEPRINTHELPER_ROOT>\CodexPlugin\scripts\install-codex-agents.cjs
```

默认推荐配置：`blueprint-explorer` 使用 `gpt-5.4-mini / high`，`sourcecode-explorer` 使用 `gpt-5.3-codex-spark / xhigh`，`task-worker` 使用 `gpt-5.4 / high`。

Recommended defaults: `blueprint-explorer` uses `gpt-5.4-mini / high`; `sourcecode-explorer` uses `gpt-5.3-codex-spark / xhigh`; `task-worker` uses `gpt-5.4 / high`.

## CLI Entry

Use either `bh` on PATH or the built CLI entry:

```powershell
bh blueprint_get_runtime_profile --json "{}" --select status,summary
node <BLUEPRINTHELPER_ROOT>\AgentFaceService\cli\build\cli\index.js blueprint_get_runtime_profile --json "{}" --select status,summary
```

The root installer removes npm-generated PowerShell `.ps1` shims when `.cmd` launchers are present, so `bh` is not blocked by ExecutionPolicy. For generated JSON in PowerShell, pipe to `--stdin` or use `--file`; avoid inline `--json $json`.

## Global MCP Lifecycle

Plugin-local MCP is not the normal Codex entry. Do not register or call ordinary BlueprintHelper read/write tools through MCP. Use MCP only for editor open/close lifecycle in ordinary Agent workflows; use CLI for ordinary reads, diagnostics, TaskSpec preview, write-session requests, execute, and result lookup. Deprecated MCP ordinary tools are not fallback paths.

For editor-asset writes, keep the workflow TaskSpec-first. Prefer `AgentFaceService/agent-guide/Templates/` for copy-and-edit JSON inputs.
