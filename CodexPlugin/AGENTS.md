# BlueprintHelper Codex Agent Entry

This package contains the Codex-facing BlueprintHelper plugin metadata, skill instructions, subagent definitions, and lifecycle MCP setup scripts.

Read `skills/blueprint-helper/SKILL.md` before using BlueprintHelper. The supported Agent-facing entry for ordinary TaskSpec reads and writes is the BlueprintHelper CLI under the sibling `AgentFaceService/cli` package. The global MCP endpoint is retained only for editor open/close lifecycle in ordinary Agent workflows.

Editor lifecycle is MCP-only for Agents. Do not use `bh open_editor`, `bh close_editor`, `blueprint_open_editor`, or `blueprint_close_editor` through CLI to start or close Unreal Editor. If the global MCP lifecycle tools are unavailable, report `lifecycle_mcp_unavailable`.

Deprecated MCP read/write/debug/task tools are not fallback paths for ordinary Agent workflows.

Use normal repository tools for source files, docs, JSON, config, tests, and build scripts. Use BlueprintHelper CLI only for Unreal Editor assets through the running Editor and Bridge.

When the task is ordinary BlueprintHelper plugin usage, do not inspect the BlueprintHelper plugin package or implementation source (`CodexPlugin/`, `ClaudePlugin/`, `AgentFaceService/`, or the UE `BlueprintHelper/` source) to learn how to use it. Use the installed skill instructions, AgentGuide, CLI reference, and templates instead. Reading plugin source is allowed only for explicit BlueprintHelper plugin development, installation repair, or debugging tasks.

## Mandatory Codex Subagents

For any BlueprintHelper editor-asset task, the Main Agent must use the configured Codex subagents:

```text
blueprint-explorer   -> Blueprint/UMG/DataAsset/DataTable context collection
sourcecode-explorer  -> repository source-code/schema/template context collection
task-worker          -> template-first TaskSpec construction, preview, execute, result filtering
```

The Main Agent performs preflight and owns allowed global MCP lifecycle tools. Subagents must not call MCP tools.

For writes, follow the TaskSpec-first closed loop:

```text
main preflight -> explorer context -> task-worker TaskSpec -> preview -> write session if needed -> execute -> result -> main-agent next decision
```

For complex CLI inputs, start at `AgentFaceService/agent-guide/Templates/INDEX.md`, choose the category semantic index, copy a matching JSON template, and use `--file`.

Never request or forward raw Bridge auth tokens. Interactive write approval belongs to the running Editor/Bridge session.
