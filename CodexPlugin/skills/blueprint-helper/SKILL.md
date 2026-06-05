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

Do not inspect the BlueprintHelper plugin package or implementation source (`CodexPlugin/`, `ClaudePlugin/`, `AgentFaceService/`, or the UE `BlueprintHelper/` source) merely to learn how to use the plugin. That is redundant and forbidden for ordinary plugin usage. Use this skill, the AgentGuide, CLI reference, and templates instead. Read plugin source only when the user explicitly asks for BlueprintHelper plugin development, installation repair, or debugging.

## Entry Rule

The supported Agent-facing entry for ordinary TaskSpec reads and writes is the BlueprintHelper CLI. The global MCP endpoint is retained only for editor open/close lifecycle in ordinary Agent workflows.

Important: call editor lifecycle commands only through the global MCP tools `mcp__blueprint_helper__blueprint_open_editor` and `mcp__blueprint_helper__blueprint_close_editor`. Do not run `bh open_editor`, `bh close_editor`, `blueprint_open_editor`, or `blueprint_close_editor` through the CLI to start or close Unreal Editor. If the global MCP lifecycle tools are unavailable, stop and report `lifecycle_mcp_unavailable` instead of using a CLI fallback. Do not validate lifecycle behavior through plugin-local MCP or one-shot shell MCP clients because the sandbox may reap child editor processes.

Deprecated MCP ordinary read/write/debug/task tools are forbidden for Agent workflows. Do not use them as fallback.

## Configure Routing

When the user asks to configure BlueprintHelper safety/profile preferences for Codex, use the sibling `blueprint-helper-configure` skill. If that skill is not indexed in the current Codex session, follow `skills/blueprint-helper-configure/SKILL.md` from this plugin package as the fallback configure workflow.

## CLI Entry

Preferred CLI shape:

```powershell
bh <tool_name> [--file params.json | --json "{...}" | --stdin] [--select field[,field...]] [--format summary|json|full]
```

On Windows PowerShell, `bh` should resolve to the `.cmd` launcher installed by the root installer. If an older install resolves to blocked `bh.ps1`, rerun `install.cmd` or call `bh.cmd`.

PowerShell-safe JSON rule: use `--file` for reusable JSON and `--stdin` for generated JSON. Do not pass non-trivial generated payloads as inline `--json $json`, because PowerShell can strip quotes before Node receives the argument.

If `bh` is not on PATH, use the built CLI entry:

```powershell
node <BLUEPRINTHELPER_ROOT>\AgentFaceService\cli\build\cli\index.js <tool_name> [args]
```

Use compact output for routine loops:

```powershell
bh blueprint_get_runtime_profile --json "{}" --select status,summary
bh task preview --file .\task_spec.json --select status,preview_id,summary,artifacts.full_result
bh task execute --file .\task_spec.json --select status,task_run_id,summary,artifacts.full_result
```

For complex JSON, use the CLI catalog first:

```powershell
bh tools domains --format json
bh tools list <domain> <kind> --format json
bh tools templates <tool_id> --format json
```

Then read only the concrete template paths returned by `bh tools templates <tool_id>`, copy a returned template, and call the CLI with `--file`. If you call the direct tool-name entries `blueprinthelper_preview_task` or `blueprinthelper_execute_task`, use the wrapper templates with root field `task_spec`; if you call grouped `task preview` or `task execute`, use a bare `BlueprintHelper.TaskSpec.v1` file.

## Tool Catalog Flow

Do not scan plugin source or template indexes to choose BlueprintHelper tools.

Use the CLI catalog:

```powershell
bh tools domains --format json
bh tools list <domain> <kind> --format json
bh tools templates <tool_id> --format json
```

Then read only the concrete template paths returned by `bh tools templates <tool_id>`.

The standard read path is:

1. Main Agent chooses `domain` and `kind`.
2. Main Agent runs `bh tools list <domain> <kind> --format json`.
3. Main Agent selects a `tool_id`.
4. Main Agent runs `bh tools templates <tool_id> --format json`.
5. Main Agent dispatches the required sideAgent with the selected `tool_id`, returned template paths, `allowed_tools`, and `stop_conditions`.
6. The sideAgent fills the returned template and calls only the CLI tools listed in `allowed_tools`.

There is no independent tool detail step.

Generated JSON example:

```powershell
$json | bh blueprinthelper_read_context --stdin --format full
```

## Mandatory Codex Subagent Workflow

When the request involves BlueprintHelper, Unreal Engine Blueprint assets, UMG, DataAsset, DataTable, graph edits, editor asset diagnostics, Bridge/runtime checks, preview, execute, compile, save, or UE editor asset writes, the Main Agent must use the Codex subagent workflow. This includes read-only inspection and summarization of UE editor assets.

Do not expose this mandatory subagent workflow as a configure-time preference. Do not fall back to local Main Agent execution for BlueprintHelper editor-asset work. If Codex cannot dispatch subagents, stop and report `sideagent_unavailable`.

The Main Agent may run only bounded preflight CLI commands before dispatch, such as `blueprint_get_runtime_profile` and diagnostics. It must not satisfy UE asset discovery or context reads locally with `blueprinthelper_find_assets`, `blueprinthelper_read_context`, `blueprinthelper_read_reference_context`, `blueprinthelper_read_function_chain_context`, or ad hoc shell/source reads. Delegate UE asset context reads to `blueprint-explorer`, source/schema/template context to `sourcecode-explorer`, and TaskSpec preview/execute work to `task-worker`.

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

### Main Agent flow

1. Read `references/08_User_Preferences.md`, `references/00_Agent_Onboarding_Index_20260504.md`, and `references/CODEX_ADAPTER.md` when BlueprintHelper guidance is needed.
2. Convert the user's request into intent, target, scope, operation mode, and safety constraints.
3. If the target asset, target graph/scope, or create-vs-modify strategy is unclear, ask the user before any write delegation.
4. Run only bounded preflight checks locally: CLI availability, runtime profile, diagnostics/Bridge status, and global MCP editor lifecycle when explicitly needed.
5. Dispatch the smallest required Codex sideAgent task package:
   - `blueprint-explorer` for Blueprint/UMG/DataAsset/DataTable/editor-asset reads.
   - `sourcecode-explorer` for source/schema/template context when the user task truly requires repository context.
   - `task-worker` after context is sufficient for preview/execute or validation.
6. Review each compact sideAgent result, update the Main Agent context ledger, and decide whether to answer, ask the user, dispatch one bounded follow-up, or stop with a blocker.

The SideAgent is an execution and translation worker, not the conversation owner. Do not pass the full conversation or full `SKILL.md`; pass only the semantic task package and the reference paths it must read.

### Main Agent context ledger

Before dispatching any follow-up sideAgent:

- check accumulated sideAgent results for the same asset, graph/scope, view format, target name, and validation evidence;
- answer directly if existing translated evidence is enough;
- if more data is needed, identify the exact missing field, target slice, validation result, or template/schema constraint;
- delegate one atomic BlueprintHelper tool step for that missing data, not a broad repeat read or a second full-graph analysis.

The Main Agent owns context reuse. SideAgents do not decide whether previous sideAgent evidence is sufficient.

### SideAgent delegation package

When delegating, use compact semantic fields instead of dumping rules:

```yaml
user_goal: "<what the user wants in gameplay/editor terms>"
main_agent_decision: "<why this requires BlueprintHelper tool access>"
operation_mode: "create_new | modify_existing | inspect_only | validate_only"
target_asset_path: "<UE asset path, or unknown>"
target_graph_or_scope: "<graph/function/event/widget/table/object scope, or unknown>"
safety_constraints:
  allow_modify_user_nodes: false
  require_preview: true
  require_write_session_if_disabled: true
  write_session_scope: "running Editor/Bridge, usable by delegated sideAgents within approved scope and lifetime"
read_strategy:
  avoid_full_logic_md_when_graph_size_unknown: true
  large_graph_node_threshold: 80
  large_graph_policy: "estimate size first, then read summary, logic_flow, bounded logic_json, or block-scoped slices"
tool_call_intent:
  tool_name: "<single BlueprintHelper CLI/tool step this sideAgent should execute>"
  missing_field_reason: "<why Main Agent cannot answer from accumulated sideAgent results>"
references_to_read:
  - "references/09_SideAgent_Tool_Execution.md"
  - "<workflow reference if needed>"
tool_id: "<selected tool_id from bh tools list>"
returned_template_paths: []
allowed_tools: []
stop_conditions:
  - "missing target asset or create/modify strategy"
  - "Bridge unavailable"
  - "runtime_profile blocks write"
  - "preview blocked"
  - "write session rejected"
  - "tool unavailable"
return_format: "Chinese compact YAML summary with tool names, key arguments, status, blockers, validation, and next step"
```

For read-only user requests, still dispatch `blueprint-explorer` with `operation_mode: inspect_only` or `validate_only`. The Main Agent should summarize the sideAgent result for the user instead of calling the asset read command directly.

### SideAgent responsibility

The sideAgent task package must make these responsibilities explicit:

- construct valid BlueprintHelper tool parameters from the user's goal, target, and Main Agent context;
- read `references/09_SideAgent_Tool_Execution.md` and only the workflow/template references needed for the assigned step;
- call only the assigned BlueprintHelper tool or one atomic CLI step;
- avoid broad investigation, adjacent repeated reads, full conversation analysis, or deciding whether prior context is sufficient;
- treat missing commands as `tool_unavailable`, a CLI installation or registration problem;
- never replace unavailable BlueprintHelper CLI commands with shell reads, `.vs\BlueprintCache`, Saved exports, local JSON parsing, plugin source inspection, or deprecated MCP ordinary tools;
- estimate graph size before requesting full graph `logic_md`; use summary, `logic_flow`, bounded `logic_json`, function/event/custom-event slices, structured anchors, or block-scoped reads for larger graphs;
- run preview, write-session request, execute, and result lookup only when the Main Agent assigned that step;
- treat an approved write session as running Editor/Bridge permission, not a single-agent secret; never request, pass, print, or reveal `auth_session`;
- return concise translated evidence to the Main Agent and stop instead of asking the user directly.

### Preflight before dispatch

Before dispatching BlueprintHelper subagents:

1. Identify the target UE project and `.uproject` path when editor launch/build may be required.
2. Confirm BlueprintHelper CLI is available: `bh` or the built CLI entry.
3. Check runtime profile with CLI:
   `bh blueprint_get_runtime_profile --json "{}" --select status,summary`
4. Confirm Bridge connectivity with CLI diagnostics/runtime profile.
5. If the editor must be launched or closed, use only the global MCP lifecycle tools. Never use CLI lifecycle aliases; if MCP lifecycle tools are unavailable, report `lifecycle_mcp_unavailable`.
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
source_control_policy: "<checkout/status policy for target assets before execute>"
allowed_tools: []
tool_id: "<selected tool_id from bh tools list>"
returned_template_paths: []
stop_conditions: []
```

`task-worker` must read only returned template paths from `bh tools templates <tool_id>`, construct `BlueprintHelper.TaskSpec.v1`, run preview, request write session only when needed, run execute, and return filtered diagnostic results.

For write tasks against existing UE assets, the Main Agent must include a source-control step before execute. Use `blueprinthelper_source_control_status` or `blueprinthelper_source_control_checkout` for the target assets after preview succeeds and before execute when source control is enabled or when any lifecycle/save result reports `checkout_required`. If the status or checkout result returns `checked_out_by_other`, `source_control_conflicted`, `source_control_unavailable`, `checkout_failed`, or `not_editable`, stop and report the returned `agent_message` / `recommended_action`; do not edit or close the editor as if the save succeeded.

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
blueprinthelper_find_assets
blueprinthelper_read_context
blueprinthelper_read_context_capabilities
blueprinthelper_read_reference_context
blueprinthelper_read_function_chain_context
blueprinthelper_source_control_status
blueprinthelper_source_control_checkout
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

Global MCP lifecycle tool ids:

```text
mcp__blueprint_helper__blueprint_open_editor
mcp__blueprint_helper__blueprint_close_editor
```

CLI lifecycle aliases are not Agent execution paths. Do not call `bh open_editor`, `bh close_editor`, `blueprint_open_editor`, or `blueprint_close_editor`; the CLI rejects Agent-owned lifecycle and points back to global MCP.

Frozen legacy, expert, and low-level direct commands are not the normal Agent workflow. If a capability is missing from the supported CLI surface, stop and report the gap unless the request falls inside the explicit MCP lifecycle boundary above.

When the Unreal `asset_path` is unknown, dispatch `blueprint-explorer` to call `blueprinthelper_find_assets` first. When the Unreal `asset_path` is already known, dispatch `blueprint-explorer` to call `blueprinthelper_read_context`. Do not infer Unreal `asset_path` values from filesystem `.uasset` paths. If multiple candidates are returned, the Main Agent must narrow the request or ask for confirmation before any write flow. A write request must resolve one explicit Unreal `asset_path` before `blueprinthelper_preview_task`.

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
