---
name: blueprinthelper
description: Use when working with Unreal Engine 5 BlueprintHelper MCP, UE5 Blueprint editing, UMG, DataAsset, DataTable, LogicJson, LogicMD, or Blueprint/C++ boundary decisions.
---

# BlueprintHelper

Use this skill for BlueprintHelper MCP tasks and UE5 editor-asset work.

## Required Reading

1. Read `AGENTS.md`.
2. Read `Resources/AgentGuide/00_Agent_Onboarding_Index_20260430.md`.
3. If present, read `.blueprinthelper/agent-profile.json`.
4. If no profile exists, use conservative defaults from `Resources/Setup/Setup_Profile_Schema_20260430.md`.

## Boundary

BlueprintHelper MCP operates Unreal Editor assets. It is not a general file-system, source-editing, or code-search API.

Use normal repository tools for:

- C++, TypeScript, Python, JSON, config, and documentation edits.
- Source search.
- Build scripts.
- AGENTS.md, skills, setup profile, and project instructions.

Use BlueprintHelper MCP for:

- Blueprint graphs, variables, functions, macros, nodes, links, and dispatchers.
- UMG widget trees and properties.
- DataAsset, UObject, and DataTable values.
- Asset browser operations, compile, save, open, PIE, and editor commands.

## Read Strategy

Prefer:

1. `blueprint_get_logic` for fast Markdown review.
2. `blueprint_get_logic_json` for structured analysis and edit planning.
3. `blueprint_export_to_json` only for raw replay, import, link debugging, or exact reconstruction.

Do not pass LogicJson or LogicMD to raw JSON import tools.

## Write Strategy

Before any mutation:

1. Confirm the Unreal Editor Bridge is reachable.
2. Identify exact `asset_path` or `target_blueprint`.
3. Identify exact `target_graph` for graph edits.
4. Read current state.
5. Produce the smallest safe write plan.
6. Run the mutation.
7. Compile, validate, or save according to the profile and user request.

Never rely on the active editor tab for destructive operations unless the user explicitly asks for active-context editing.

## Risk Rules

- High-risk writes require a plan first.
- Deletions require exact targets.
- Raw JSON imports should default to strict rollback.
- Save after mutation only when the user or profile allows it.
- On failure, preserve state and report what happened. Do not retry blindly.

## References

- `references/mcp-tool-boundary.md`
- `references/logicjson-reading-guide.md`
- `references/blueprint-cpp-boundary.md`
- `references/naming-policy.md`
