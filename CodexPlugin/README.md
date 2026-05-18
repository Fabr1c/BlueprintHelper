# BlueprintHelper Codex Plugin

This is a Codex-compatible package for BlueprintHelper.

For normal user setup, run the repository-root installer:

```powershell
.\install.ps1
```

See `INSTALL.md` at the repository root for Codex Desktop, CLI, lifecycle MCP, and UE engine-install options.

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

## CLI Entry

Use either `bh` on PATH or the built CLI entry:

```powershell
bh blueprint_get_runtime_profile --json "{}" --select status,summary
node <BLUEPRINTHELPER_ROOT>\AgentFaceService\cli\build\cli\index.js blueprint_get_runtime_profile --json "{}" --select status,summary
```

## Global MCP Lifecycle

Plugin-local MCP is not the normal Codex entry. Do not register or call ordinary BlueprintHelper read/write tools through MCP. Use MCP only for editor open/close lifecycle in ordinary Agent workflows; use CLI for ordinary reads, diagnostics, TaskSpec preview, write-session requests, execute, and result lookup. Deprecated MCP ordinary tools are not fallback paths.

For editor-asset writes, keep the workflow TaskSpec-first. Prefer `AgentFaceService/agent-guide/Templates/` for copy-and-edit JSON inputs.
