# BlueprintHelper Agent Entry

This repository contains the BlueprintHelper Unreal Engine editor plugin. When an AI / IDE / CLI agent is asked to use this plugin, read this file first, then open the guide index:

```text
Resources/AgentGuide/00_Agent_Onboarding_Index_20260504.md
```

## Non-negotiable boundary

BlueprintHelper MCP is not a general file-system, code-search, or C++ source-editing API. It is an Unreal Editor bridge for operating editor assets: Blueprint graphs, UMG widgets, DataAssets, DataTables, asset browser operations, compile/save/open, PIE/editor commands, and related diagnostics.

Use normal repository tools for:

- C++ / TypeScript / Python / config edits.
- Searching source files.
- Adding documentation files.
- Updating build scripts.
- Writing AGENTS.md / memory / project instructions.

Use BlueprintHelper MCP for:

- Reading or editing existing UE assets through the running Unreal Editor.
- Creating or modifying Blueprint graphs, variables, functions, macros, nodes, links, and event dispatchers.
- Reading or editing UMG widget tree and widget properties.
- Reading or editing UObject / DataAsset properties.
- Reading or editing DataTable rows.
- Compiling, opening, saving, validating, importing, or exporting Blueprint-related editor assets.

## Required preflight before any write operation

1. Confirm the user has a target UE project and the Unreal Editor is running, or the MCP server has `UE_ENGINE_DIR` and `UE_PROJECT_FILE` configured so it can launch the editor.
2. Confirm the Bridge is reachable before calling editor-asset tools.
3. Identify the exact target asset path, for example `/Game/Blueprints/BP_Player`.
4. Identify the exact target graph when editing graph nodes, for example `EventGraph`.
5. Prefer TaskSpec-first writes: read task context -> build `BlueprintHelper.TaskSpec.v1` -> preview task -> execute task -> read task result.
6. Never rely on the currently focused editor tab for destructive operations unless the user explicitly says to operate on the active context.

## Fast path

Open `Resources/AgentGuide/00_Agent_Onboarding_Index_20260504.md` and follow the workflow matching the user request.
