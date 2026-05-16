# BlueprintHelper Agent Entry

This repository contains the BlueprintHelper Unreal Engine editor plugin. When an AI / IDE / CLI agent is asked to use this plugin, read this file first, then open the guide index:

```text
Resources/AgentGuide/00_Agent_Onboarding_Index_20260504.md
```

## Non-negotiable boundary

BlueprintHelper is not a general file-system, code-search, or C++ source-editing API. The active Agent-facing transport for ordinary TaskSpec writes, reads, diagnostics, debug summaries, write-session requests, and result queries is the BlueprintHelper CLI, which talks to a running Unreal Editor through the local Bridge. Global MCP is retained for editor lifecycle only.

Use normal repository tools for:

- C++ / TypeScript / Python / config edits.
- Searching source files.
- Adding documentation files.
- Updating build scripts.
- Writing AGENTS.md / memory / project instructions.

Use BlueprintHelper CLI / task-core tools for:

- Reading or editing existing UE assets through the running Unreal Editor.
- Creating or modifying Blueprint graphs, variables, functions, macros, nodes, links, and event dispatchers.
- Reading or editing UMG widget tree and widget properties.
- Reading or editing UObject / DataAsset properties.
- Reading or editing DataTable rows.
- Compiling, opening, saving, validating, importing, or exporting Blueprint-related editor assets.

Do not use MCP for normal asset workflows. New Agent workflows should use CLI commands for ordinary asset operations and global MCP lifecycle tools for Agent-owned editor open/close.

## Required preflight before any write operation

1. Confirm the user has a target UE project and the Unreal Editor is running, or use the global MCP lifecycle tool after confirming the project `.blueprinthelper/agent-profile.json` has `environment.ue_engine_dir` configured.
2. Confirm the Bridge is reachable before calling editor-asset tools.
3. Identify the exact target asset path, for example `/Game/Blueprints/BP_Player`.
4. Identify the exact target graph when editing graph nodes, for example `EventGraph`.
5. Prefer TaskSpec-first writes through the CLI: read task context -> build `BlueprintHelper.TaskSpec.v1` -> preview task -> execute task -> read task result.
6. If `write_permission` is disabled, call `blueprinthelper_request_write_session` after preview and before execute. The running Editor shows a simple accept/reject approval dialog; if the user rejects it, stop and report instead of trying another write path.
7. Do not ask for or inject `BLUEPRINTHELPER_BRIDGE_TOKEN`, `auth_token`, or `auth_session` for ordinary interactive writes. The running Editor/Bridge owns the approved scope and lifetime, so delegated SideAgents can use BlueprintHelper CLI/task-core tools after approval without receiving raw session data.
8. Never rely on the currently focused editor tab for destructive operations unless the user explicitly says to operate on the active context.

## Fast path

Open `Resources/AgentGuide/00_Agent_Onboarding_Index_20260504.md` and follow the workflow matching the user request.

For complex JSON inputs, copy a matching template from `Resources/AgentGuide/Templates/` and call the CLI with `--file`.

