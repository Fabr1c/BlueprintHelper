# BlueprintHelper Codex Plugin

This is a Codex-compatible package for BlueprintHelper.

For normal user setup, run the repository-root installer:

```powershell
.\install.ps1
```

See `INSTALL.md` at the repository root for Codex Desktop, CLI, lifecycle MCP, and UE engine-install options.

## Contents

- `.codex-plugin/plugin.json` is the Codex plugin manifest.
- `.codex/agents/` contains the Codex subagent definitions used by the BlueprintHelper workflow.
- `skills/blueprint-helper/SKILL.md` is the Codex-facing workflow entry.
- `skills/blueprint-helper/references/` mirrors the BlueprintHelper agent references from the canonical `BlueprintHelper/Resources/AgentGuide` docs.
- `assets/blueprint-helper.svg` is the local plugin icon referenced by the manifest.

## Runtime Model

The active Agent-facing transport for ordinary TaskSpec reads and writes is the BlueprintHelper CLI. The global MCP endpoint is an allowlist and should expose only:

```text
mcp__blueprint_helper__blueprint_open_editor
mcp__blueprint_helper__blueprint_close_editor
mcp__blueprint_helper__blueprint_developer_exec_console_command
```

Only the Main Agent may call MCP lifecycle tools. `mcp__blueprint_helper__blueprint_developer_exec_console_command` is developer-only for local BlueprintHelper test orchestration and is not an ordinary Agent asset workflow tool. Subagents must not call MCP tools.

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

## CLI Setup

Build the CLI when needed:

```powershell
cd <BLUEPRINTHELPER_ROOT>\AgentFaceService\task-core
npm install
npm run build

cd <BLUEPRINTHELPER_ROOT>\AgentFaceService\cli
npm install
npm run build
```

Use either `bh` on PATH or the built CLI entry:

```powershell
bh blueprint_get_runtime_profile --json "{}" --select status,summary
node <BLUEPRINTHELPER_ROOT>\AgentFaceService\cli\build\cli\index.js blueprint_get_runtime_profile --json "{}" --select status,summary
```

## Global MCP Allowlist

Build the MCP package when needed:

```powershell
cd <BLUEPRINTHELPER_ROOT>\AgentFaceService\mcp
npm install
npm run build
```

Install the MCP allowlist globally:

```powershell
node <BLUEPRINTHELPER_ROOT>\CodexPlugin\scripts\install-global-mcp.cjs
```

Plugin-local MCP is not the normal Codex entry. Do not register or call ordinary BlueprintHelper read/write tools through MCP. Use MCP only for editor open/close plus developer-only exec command; use CLI for ordinary reads, diagnostics, TaskSpec preview, write-session requests, execute, and result lookup. Do not add or run tests for deprecated MCP ordinary tools.

For editor-asset writes, keep the workflow TaskSpec-first. Prefer `BlueprintHelper/Resources/AgentGuide/Templates/` for copy-and-edit JSON inputs.
