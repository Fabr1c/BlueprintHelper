# BlueprintHelper Codex Agent Entry

This package contains the Codex-facing BlueprintHelper plugin metadata and skill instructions.

Read `skills/blueprint-helper/SKILL.md` before using BlueprintHelper. The supported Agent-facing entry for ordinary TaskSpec reads and writes is the BlueprintHelper CLI under the sibling `AgentFaceService/cli` package. The MCP endpoint is retained only for editor lifecycle commands.

Use normal repository tools for source files, docs, JSON, config, tests, and build scripts. Use BlueprintHelper CLI only for Unreal Editor assets through the running Editor and Bridge.

For writes, follow the TaskSpec-first loop:

```text
runtime profile -> read task context -> TaskSpec -> preview -> write session if needed -> execute -> result
```

Never request or forward raw Bridge auth tokens. Interactive write approval belongs to the running Editor/Bridge session.
