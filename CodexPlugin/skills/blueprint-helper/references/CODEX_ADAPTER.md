# Codex Adapter Notes

These references were ported from the existing `ClaudePlugin` BlueprintHelper agent package. Keep the project rules and CLI-first workflow, but apply these Codex-specific translations:

- Treat `AgentFaceService` as the sibling runtime package that owns `task-core`, `cli`, and the global lifecycle-only MCP companion.
- Treat copied `ClaudePlugin` path references as historical source-package wording unless they describe current Claude plugin shell files such as `commands/` or `skills/`.
- Treat `Claude` product references as historical source-package wording unless they describe a concrete file path.
- Treat `SideAgent` instructions as mandatory for Codex BlueprintHelper editor-asset work. When a request involves Blueprint, UMG, DataAsset, DataTable, Bridge/runtime, preview, execute, compile, save, or UE editor asset writes, the Main Agent must dispatch the configured Codex subagents unless subagent dispatch is unavailable. If unavailable, return `sideagent_unavailable` instead of falling back to local execution.
- Do not use `AskUserQuestion` references literally in Codex. Ask concise user questions only when local context cannot determine a safe target or workflow.
- Prefer CLI for ordinary TaskSpec/ReadSpec/debug-summary work. Use global MCP only for editor launch/lifecycle. The global MCP server should expose only `blueprint_open_editor` and `blueprint_close_editor`.
- Only the Main Agent may use MCP lifecycle tools. `blueprint-explorer`, `sourcecode-explorer`, and `task-worker` must not call any `mcp__blueprint_helper__*` tool.

The copied references are included so Codex can reuse the existing BlueprintHelper safety, TaskSpec, read, UMG, DataAsset, DataTable, diagnostics, and validation guidance without duplicating the whole documentation set.
