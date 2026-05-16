# Codex Adapter Notes

These references were ported from the existing `ClaudePlugin` BlueprintHelper agent package. Keep the project rules and CLI-first workflow, but apply these Codex-specific translations:

- Treat `AgentFaceService` as the sibling runtime package that owns `task-core`, `cli`, and the global MCP lifecycle companion.
- Treat copied `ClaudePlugin` path references as historical source-package wording unless they describe current Claude plugin shell files such as `commands/` or `skills/`.
- Treat `Claude` product references as historical source-package wording unless they describe a concrete file path.
- Treat `SideAgent` instructions as optional delegation guidance. In Codex, execute locally unless the user explicitly asks for sub-agents, delegation, or parallel agent work.
- Do not use `AskUserQuestion` references literally in Codex. Ask concise user questions only when local context cannot determine a safe target or workflow.
- Prefer CLI for ordinary TaskSpec/ReadSpec/debug-summary work. Use global MCP only for editor launch/lifecycle, debug, recovery, and commands that need a long-lived host process.

The copied references are included so Codex can reuse the existing BlueprintHelper safety, TaskSpec, read, UMG, DataAsset, DataTable, diagnostics, and validation guidance without duplicating the whole documentation set.
