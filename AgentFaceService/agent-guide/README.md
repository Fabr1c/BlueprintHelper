# BlueprintHelper AgentGuide

This directory contains shared Agent-facing workflow and contract guidance for
BlueprintHelper.

Recommended entry:

```text
00_Agent_Onboarding_Index.md
```

## Current Boundary

- Runtime CLI discovery is the source of truth for concrete tools and template
  files.
- This guide does not duplicate template content or directory-scanning
  instructions.
- Ordinary UE editor-asset reads and writes use CLI TaskSpec-first workflows.
- Editor open/close is owned by the global MCP lifecycle tools.
- Repository files use normal repository tools.
- Deprecated MCP ordinary tools are not fallback paths.
