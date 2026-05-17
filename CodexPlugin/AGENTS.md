# BlueprintHelper Codex Agent Entry

This package contains the Codex-facing BlueprintHelper plugin metadata, skill instructions, subagent definitions, and lifecycle MCP setup scripts.

Read `skills/blueprint-helper/SKILL.md` before using BlueprintHelper. The supported Agent-facing entry for ordinary TaskSpec reads and writes is the BlueprintHelper CLI under the sibling `AgentFaceService/cli` package. The global MCP endpoint is retained only for the explicit allowlist: editor open, editor close, and developer-only exec command.

Deprecated MCP read/write/debug/task tools are not fallback paths. Do not call them, do not add tests for them, and do not run deleted or legacy test files that validate those deprecated tools.

Use normal repository tools for source files, docs, JSON, config, tests, and build scripts. Use BlueprintHelper CLI only for Unreal Editor assets through the running Editor and Bridge.

## Mandatory Codex Subagents

For any BlueprintHelper editor-asset task, the Main Agent must use the configured Codex subagents:

```text
blueprint-explorer   -> Blueprint/UMG/DataAsset/DataTable context collection
sourcecode-explorer  -> repository source-code/schema/template context collection
task-worker          -> template-first TaskSpec construction, preview, execute, result filtering
```

The Main Agent performs preflight and owns allowed global MCP lifecycle tools. Subagents must not call MCP tools. The developer exec command is for local BlueprintHelper development/test orchestration only, not for ordinary Agent asset workflows.

For writes, follow the TaskSpec-first closed loop:

```text
main preflight -> explorer context -> task-worker TaskSpec -> preview -> write session if needed -> execute -> result -> main-agent next decision
```

For complex CLI inputs, start at `AgentFaceService/agent-guide/Templates/INDEX.md`, choose the category semantic index, copy a matching JSON template, and use `--file`.

Never request or forward raw Bridge auth tokens. Interactive write approval belongs to the running Editor/Bridge session.
