---
name: blueprint-helper
description: Use when a user request requires reading, inspecting, creating, modifying, or configuring Unreal Engine Blueprint-related editor assets and BlueprintHelper behavior from Codex.
---

# BlueprintHelper for Codex

## Role

Use this skill when a user asks Codex to work with Unreal Editor assets through BlueprintHelper, including:

- Blueprint graphs, variables, functions, macros, components, class settings, interfaces, nodes, links, and dispatchers.
- UMG widget trees and widget properties.
- UObject, DataAsset, and DataTable values.
- Compile, save, open, PIE, diagnostics, and Bridge/runtime checks related to editor assets.
- BlueprintHelper Codex configuration, safety profile, editor lifecycle policy, and Agent workflow preferences.
- BlueprintHelper Codex configuration, safety profile, editor lifecycle policy, and Agent workflow preferences.
- BlueprintHelper Codex configuration, safety profile, editor lifecycle policy, and Agent workflow preferences.

Do not use BlueprintHelper for normal repository files. Use normal Codex shell and edit tools for C++, TypeScript, Python, JSON, config, docs, tests, build scripts, and source search.

## Entry Rule

The supported Agent-facing entry for ordinary TaskSpec reads and writes is the BlueprintHelper CLI. MCP is retained only as a long-lived companion entry for editor lifecycle commands.

Important: call editor lifecycle commands through the global MCP tools `mcp__blueprint_helper__blueprint_open_editor` and `mcp__blueprint_helper__blueprint_close_editor`. Do not validate lifecycle behavior through plugin-local MCP or one-shot shell MCP clients because the sandbox may reap child editor processes.

## Configure Routing

When the user asks to configure BlueprintHelper for Codex, update safety/profile preferences, or asks for the Codex equivalent of Claude `/blueprint-helper:configure`, use the sibling `blueprint-helper-configure` skill. If that skill is not indexed in the current Codex session, follow `skills/blueprint-helper-configure/SKILL.md` from this plugin package as the fallback configure workflow.


Preferred CLI shape:

```powershell
bh <tool_name> [--file params.json | --json "{...}" | --stdin] [--select field[,field...]] [--format summary|json|full]
```

If `bh` is not on PATH, use the built CLI entry:

```powershell
node <BLUEPRINTHELPER_ROOT>\AgentFaceService\cli\build\cli\index.js <tool_name> [args]
```

Build prerequisites when the CLI is missing:

```powershell
cd <BLUEPRINTHELPER_ROOT>\AgentFaceService\task-core
npm install
npm run build

cd <BLUEPRINTHELPER_ROOT>\AgentFaceService\cli
npm install
npm run build
```

## Required Preflight

Before any editor-asset write:

1. Confirm the target UE project and `.uproject` path when launch/build is needed.
2. Confirm Unreal Editor is running with BlueprintHelper loaded. Prefer MCP `blueprint_open_editor` when the editor must be launched from an Agent workflow; ordinary BlueprintHelper read/write commands still use CLI.
3. Confirm the Bridge is reachable.
4. Identify the exact target asset path, such as `/Game/Blueprints/BP_Player`.
5. Identify the exact graph/function/widget/table scope when applicable.
6. Prefer TaskSpec-first writes: read context, build `BlueprintHelper.TaskSpec.v1`, preview, request write session if needed, execute, then read result.
7. Never request, set, print, or forward `BLUEPRINTHELPER_BRIDGE_TOKEN`, `auth_token`, or `auth_session`.
8. Never rely on the currently focused editor tab for destructive operations unless the user explicitly asks for active-context editing.

## Default Workflow

Use this mainline for ordinary writes:

```text
blueprint_get_runtime_profile
-> blueprinthelper_read_task_context or blueprinthelper_read_context
-> author BlueprintHelper.TaskSpec.v1
-> blueprinthelper_preview_task
-> blueprinthelper_request_write_session when write_permission is disabled
-> blueprinthelper_execute_task
-> blueprinthelper_get_task_result when needed
```

Use compact output for routine loops:

```powershell
bh blueprint_get_runtime_profile --json "{}" --select status,summary
bh blueprinthelper_preview_task --file .\task_spec.json --select status,preview_id,summary,artifacts.full_result
bh blueprinthelper_execute_task --file .\task_spec.json --select status,task_run_id,summary,artifacts.full_result
```

## Supported Agent-Facing Commands

Default Agent-facing commands:

```text
blueprint_get_runtime_profile
blueprinthelper_diagnostics
blueprinthelper_diagnostics_runtime
blueprinthelper_read_agent_guide
blueprinthelper_read_context
blueprinthelper_read_task_context
blueprinthelper_read_reference_context
blueprinthelper_preview_task
blueprinthelper_request_write_session
blueprinthelper_execute_task
blueprinthelper_get_task_result
blueprinthelper_get_debug_case
blueprinthelper_list_debug_cases
blueprinthelper_export_debug_bundle
```

MCP-only editor lifecycle companion commands:

```text
blueprint_open_editor
blueprint_close_editor
```

Frozen legacy, expert, and low-level direct commands are not the normal Agent workflow. If a capability is missing from the supported CLI surface, stop and report the gap unless the request falls inside the explicit MCP editor lifecycle boundary above.

## Read Strategy

- Use `summary` before whole-graph `logic_md` when graph size is unknown.
- If a graph has more than 80 nodes, use scoped reads, block reads, or structured anchors instead of full graph text.
- Use `logic_json` when stable owned-block anchors or importability checks are needed.
- Keep large payloads in artifacts; use `--select` or `--fields` for stdout.

## Codex Collaboration

Codex should execute the needed shell and file operations locally unless the user explicitly asks for sub-agents or parallel delegation. When delegation is explicitly requested, pass a small execution package with one missing tool result or one bounded implementation scope.

Report results in the user's language. Include tool names, key arguments, status, blockers, validation, and the next step when useful.

## References

Read these references as needed:

- `references/CODEX_ADAPTER.md`
- `references/08_User_Preferences.md`
- `references/00_Agent_Onboarding_Index_20260504.md`
- `references/01_Preflight_And_Boundary.md`
- `references/02_TaskSpec_First_Tool_Selection.md`
- `references/03_Runtime_Profile_And_Diagnostics.md`
- `references/04_Tool_Surface_Field_Templates_20260512.md`
- `references/09_SideAgent_Tool_Execution.md`
- `references/04_TaskSpec_Edit_Blueprint_Workflow.md`
- `references/05_Edit_Blueprint_Workflow.md`
- `references/06_UMG_Data_Workflows.md`
- `references/07_Safety_Validation_And_Recovery.md`
