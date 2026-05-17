---
name: blueprint-helper
description: Use for BlueprintHelper, UE5/Unreal Engine Blueprint asset reads or writes, mandatory Codex subagents, Blueprint graph TaskSpec preview/execute, UMG/DataAsset/DataTable editor assets, Bridge/runtime diagnostics, lifecycle MCP, and BlueprintHelper Codex configuration. Do not use for ordinary repo source files.
---

# BlueprintHelper for Codex

## Role

Use this skill when a user asks Codex to work with Unreal Editor assets through BlueprintHelper, including:

- Blueprint graphs, variables, functions, macros, components, class settings, interfaces, nodes, links, and dispatchers.
- UMG widget trees and widget properties.
- UObject, DataAsset, and DataTable values.
- Compile, save, open, PIE, diagnostics, and Bridge/runtime checks related to editor assets.
- BlueprintHelper Codex configuration, safety profile, and editor lifecycle policy.

Do not use BlueprintHelper for normal repository files. Use normal Codex shell and edit tools for C++, TypeScript, Python, JSON, config, docs, tests, build scripts, and source search.

## Entry Rule

The supported Agent-facing entry for ordinary TaskSpec reads and writes is the BlueprintHelper CLI. The global MCP endpoint is retained only for editor open/close plus the developer-only exec command.

Important: call editor lifecycle commands only through the global MCP tools `mcp__blueprint_helper__blueprint_open_editor` and `mcp__blueprint_helper__blueprint_close_editor`. Do not validate lifecycle behavior through plugin-local MCP or one-shot shell MCP clients because the sandbox may reap child editor processes.

Deprecated MCP ordinary read/write/debug/task tools are forbidden for Agent workflows. Do not use them as fallback, do not add tests for them, and do not run old MCP tool tests. The developer exec command is not a normal asset workflow tool.

## Configure Routing

When the user asks to configure BlueprintHelper safety/profile preferences for Codex, use the sibling `blueprint-helper-configure` skill. If that skill is not indexed in the current Codex session, follow `skills/blueprint-helper-configure/SKILL.md` from this plugin package as the fallback configure workflow.

## CLI Entry

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

Use compact output for routine loops:

```powershell
bh blueprint_get_runtime_profile --json "{}" --select status,summary
bh task preview --file .\task_spec.json --select status,preview_id,summary,artifacts.full_result
bh task execute --file .\task_spec.json --select status,task_run_id,summary,artifacts.full_result
```

For complex JSON, prefer copying templates from `BlueprintHelper/Resources/AgentGuide/Templates/` and calling the CLI with `--file`. If you call the direct tool-name entries `blueprinthelper_preview_task` or `blueprinthelper_execute_task`, use the wrapper templates with root field `task_spec`; if you call grouped `task preview` or `task execute`, use a bare `BlueprintHelper.TaskSpec.v1` file.

## Mandatory Codex Subagent Workflow

When the request involves BlueprintHelper, Unreal Engine Blueprint assets, UMG, DataAsset, DataTable, graph edits, editor asset diagnostics, Bridge/runtime checks, preview, execute, compile, save, or UE editor asset writes, the Main Agent must use the Codex subagent workflow.

Do not expose this mandatory subagent workflow as a configure-time preference. Do not fall back to local Main Agent execution for BlueprintHelper editor-asset work. If Codex cannot dispatch subagents, stop and report `sideagent_unavailable`.

Configured subagents:

```text
blueprint-explorer   -> collects Blueprint/UMG/DataAsset/DataTable/editor-asset context
sourcecode-explorer  -> collects repository source-code/schema/template context
task-worker          -> constructs template-first TaskSpec, runs preview/execute, filters diagnostics
```

### Main Agent ownership

The Main Agent owns:

- user intent and clarification questions;
- target asset, graph, widget, table, or object scope confirmation;
- safety decisions and write boundary decisions;
- project/editor preflight;
- Bridge/runtime availability checks;
- global MCP editor lifecycle tools;
- final user response;
- closed-loop decisions after subagent results.

Only the Main Agent may call:

```text
mcp__blueprint_helper__blueprint_open_editor
mcp__blueprint_helper__blueprint_close_editor
```

Subagents must not call MCP tools.

### Preflight before dispatch

Before dispatching BlueprintHelper subagents:

1. Identify the target UE project and `.uproject` path when editor launch/build may be required.
2. Confirm BlueprintHelper CLI is available: `bh` or the built CLI entry.
3. Check runtime profile with CLI:
   `bh blueprint_get_runtime_profile --json "{}" --select status,summary`
4. Confirm Bridge connectivity with CLI diagnostics/runtime profile.
5. If the editor must be launched or closed, use only the global MCP lifecycle tools.
6. Never request, set, print, or forward `BLUEPRINTHELPER_BRIDGE_TOKEN`, `auth_token`, or `auth_session`.
7. Never rely on the currently focused editor tab for destructive operations unless the user explicitly asks for active-context editing.

### Explorer dispatch

For each user request, dispatch at most:

- one `blueprint-explorer`;
- one `sourcecode-explorer`.

Dispatch `blueprint-explorer` when Blueprint/UMG/DataAsset/DataTable/editor asset context is needed.

Dispatch `sourcecode-explorer` when source-code, schema, CLI, C++, TypeScript, config, tests, or template context is needed.

The explorers return compact context to the Main Agent. They do not write, preview, execute, launch/close the editor, call MCP, or request write sessions.

### Task worker dispatch

After collecting enough context, the Main Agent builds a task package for `task-worker`.

The task package must include:

```yaml
user_goal: "<what the user wants>"
target_asset_path: "<UE asset path>"
target_graph_or_scope: "<graph/function/event/widget/table/object scope>"
operation_mode: "create_new | modify_existing | inspect_only | validate_only"
required_operations: []
blueprint_context_summary: "<from blueprint-explorer>"
source_context_summary: "<from sourcecode-explorer or none>"
safety_profile: "<runtime profile safety>"
write_policy: "<write permission/session policy>"
allowed_tools: []
template_hint: "<preferred template path or search target>"
stop_conditions: []
```

`task-worker` must prefer templates from `BlueprintHelper/Resources/AgentGuide/Templates/`, construct `BlueprintHelper.TaskSpec.v1`, run preview, request write session only when needed, run execute, and return filtered diagnostic results.

### Closed loop

If `task-worker` returns a failure, the Main Agent must inspect:

- error code;
- failed operation;
- affected asset/graph/node/pin when available;
- preview vs execute phase;
- whether the issue is missing context, capability missing, bridge/runtime failure, or malformed TaskSpec.

Then the Main Agent may either:

- dispatch a corrected task package to `task-worker`;
- dispatch one additional bounded context request;
- ask the user for a missing decision;
- stop and report the blocker.

## Supported Agent-Facing CLI Commands

Default Agent-facing commands:

```text
blueprint_get_runtime_profile
blueprinthelper_diagnostics
blueprinthelper_diagnostics_runtime
blueprinthelper_read_agent_guide
blueprinthelper_read_context
blueprinthelper_read_task_context
blueprinthelper_read_reference_context
blueprinthelper_read_function_chain_context
blueprinthelper_preview_task
blueprinthelper_request_write_session
blueprinthelper_execute_task
blueprinthelper_get_task_result
blueprinthelper_get_debug_case
blueprinthelper_list_debug_cases
blueprinthelper_export_debug_bundle
blueprinthelper_query_review_records
```

`blueprinthelper_apply_review_action` is plugin-development/internal and is not part of ordinary Codex Agent workflows.

MCP allowlist companion commands:

```text
blueprint_open_editor
blueprint_close_editor
blueprint_developer_exec_console_command
```

`blueprint_developer_exec_console_command` is developer-only for local BlueprintHelper test orchestration. Frozen legacy, expert, and low-level direct commands are not the normal Agent workflow. If a capability is missing from the supported CLI surface, stop and report the gap unless the request falls inside the explicit MCP allowlist boundary above.

## Read Strategy

- Use `summary` before whole-graph `logic_md` when graph size is unknown.
- If a graph has more than 80 nodes, use scoped reads, block reads, or structured anchors instead of full graph text.
- Use `logic_json` when stable owned-block anchors or importability checks are needed.
- Keep large payloads in artifacts; use `--select` or `--fields` for stdout.

## Reporting

Report results in the user's language. Include tool names, key arguments, status, blockers, validation, and the next step when useful. Do not claim completion unless preview/execute/result evidence supports it.

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
