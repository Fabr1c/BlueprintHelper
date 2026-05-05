# BlueprintHelper MCP Tools API Reference

文档版本：2026-05-04

This reference is aligned with the current documentation mainline, where ordinary Agents use TaskSpec-first orchestration.

Architecture baseline:

```text
Agent -> TaskSpec semantic task -> MCP Task Tools -> Python/MCP Task Compiler -> TaskPlan structured edit language / IR -> Bridge task-level preview/execute -> UE Task Runtime lowering -> Existing UE capability clusters / Bridge commands
```

Ordinary Agents author `BlueprintHelper.TaskSpec.v1` only. The existing low-level MCP tools remain documented for compatibility, debug / expert workflows, internal Task Runtime capability mapping, and automation tests.

## Common Return Shape

Agent-facing task tools use `BlueprintHelper.McpToolResult.v1` as the outer envelope. The same envelope remains the public shape for normalized tool results:

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "preview_task",
  "trace_id": "trace_20260504_0001",
  "status": "completed",
  "modified": false,
  "data": {
    "schema": "TaskPreviewResult.v1"
  }
}
```

Default Agent-facing payload schemas:

| Tool | Result shape |
|---|---|
| `blueprinthelper_read_agent_guide` | Markdown text only |
| `blueprinthelper_read_context` | `McpToolResult.v1` with `data.schema = ReadContextPack.v1` |
| `blueprinthelper_read_reference_context` | `McpToolResult.v1` with `data.schema = ReferenceContextPack.v1` |
| `blueprinthelper_preview_task` | `McpToolResult.v1` with `data.schema = TaskPreviewResult.v1` |
| `blueprinthelper_execute_task` | `McpToolResult.v1` with `data.schema = TaskRunSummary.v1` or `TaskRunJournal.v1` |
| `blueprinthelper_get_task_result` | `McpToolResult.v1` with `data.schema = TaskRunJournal.v1` |

UE façade results use `FBlueprintHelperToolResultBase`; MCP normalizes them into the public `McpToolResult.v1` envelope. Compact debug facts belong under `data.debug`; large details should use `large_payload_ref`.

TaskSpec validation, semantic, policy, capability, preview, and execution failures are returned as task-level errors. A failed TaskSpec does not require the Agent to inspect raw Bridge / UE operation errors by default:

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "preview_task",
  "trace_id": "trace_20260504_0002",
  "status": "failed",
  "modified": false,
  "error": {
    "code": "taskspec_semantic_invalid",
    "category": "semantic_error",
    "message": "TaskSpec contains contradictory instructions.",
    "retryable": true,
    "agent_action": "fix_taskspec_and_retry",
    "issues": []
  }
}
```

Transport or Bridge connection failure:

```text
Bridge error: Bridge connection error: connect ECONNREFUSED 127.0.0.1:54321
```

## Tool Audience Summary

Default Agent-facing tools:

- `blueprinthelper_read_agent_guide`
- `blueprinthelper_get_runtime_profile`
- `blueprinthelper_diagnostics`
- `blueprinthelper_read_context`
- `blueprinthelper_read_reference_context`
- `blueprinthelper_preview_task`
- `blueprinthelper_execute_task`
- `blueprinthelper_get_task_result`
- `blueprinthelper_open_editor`
- `blueprinthelper_close_editor`

Legacy/internal/debug/expert tools remain registered only until their TaskSpec / ReadSpec replacements are complete. Capabilities that already have a TaskPlan adapter and TaskSpec compiler coverage should have their old Agent-facing atomic MCP write tools removed first. Remaining old tools stay legacy/internal/debug/expert/test until their adapter and TaskSpec support lands, then they are removed in the same slice. Ordinary Agents should prefer the default tools above unless the user explicitly asks for a low-level debug path or a failure investigation needs raw capability detail.

Legacy/internal/debug/expert inventory:

- Context, guide, and diagnostics: `blueprinthelper_read_agent_guide`, `blueprint_get_editor_context`, `blueprinthelper_diagnostics_runtime`
- Direct logic/raw reads and validation: `blueprint_get_logic_md`, `blueprint_validate_json`, `blueprint_export_to_json`, `blueprint_get_logic`, `blueprint_get_logic_json`
- Asset and component capabilities: `blueprint_create_asset`, `blueprint_read_components`, `blueprint_add_component`, `blueprint_set_component_property`, `blueprint_set_component_properties`, `blueprint_remove_component`, `blueprint_open_asset`, `blueprint_list_assets`, `blueprint_search_assets`, `blueprint_save_asset`, `blueprint_get_asset_info`
- Blueprint graph/member capabilities: `blueprint_import_json_to_graph`, `blueprint_import_agent_graph`, `blueprint_compile_blueprint`, `blueprint_list_graphs`, `blueprint_list_variables`, `blueprint_list_event_dispatchers`, `blueprint_add_variable`, `blueprint_remove_variable`, `blueprint_add_graph`, `blueprint_remove_graph`, `blueprint_add_event_dispatcher`, `blueprint_delete_nodes`
- UMG, UObject, and DataTable capabilities: `blueprint_get_widget_tree`, `blueprint_add_widget`, `blueprint_remove_widget`, `blueprint_move_widget`, `blueprint_get_widget_properties`, `blueprint_set_widget_property`, `blueprint_get_object_properties`, `blueprint_set_object_property`, `blueprint_get_datatable_rows`, `blueprint_add_datatable_row`, `blueprint_update_datatable_row`, `blueprint_delete_datatable_row`
- Editor lifecycle and local process tools: `blueprint_undo`, `blueprint_redo`, `blueprint_play_in_editor`, `blueprint_stop_pie`, `blueprint_create_blueprint`, `blueprint_exec_console_command`, `blueprint_close_editor`, `blueprint_build_project`, `blueprint_open_editor`

## Task-Level Tool Reference

This is the documented target surface for ordinary Agents. Some entries may be ahead of the currently checked-in TypeScript implementation while the Task Compiler and UE Task Runtime are being introduced.

| Tool | Type | Purpose | Writes Assets | Notes |
|---|---|---|---:|---|
| `blueprinthelper_get_runtime_profile` | Read | Returns version, Bridge state, write permission, safety profile, and unavailable capabilities | No | Call at session start or before write planning |
| `blueprinthelper_diagnostics` | Read | Returns static/runtime diagnostics | No | Blocking diagnostics are business state, not transport failure |
| `blueprinthelper_read_agent_guide` | Read | Returns the AgentGuide onboarding index Markdown | No | Documentation entry for capability surface and schema guide paths |
| `blueprinthelper_read_context` | Read | Executes `BlueprintHelper.ReadSpec.v1` through the generic read router | No | Default asset-domain read entry |
| `blueprinthelper_read_reference_context` | Read | Returns compact reference impact context | No | Independent reference viewer; not part of every write flow |
| `blueprinthelper_preview_task` | Preview | Validates `BlueprintHelper.TaskSpec.v1`, compiles `BlueprintHelper.TaskPlan.v1`, and dry-runs/preflights | No | Returns GraphWrite IR summary when compilation succeeds and suggested patches for schema/semantic errors when possible |
| `blueprinthelper_execute_task` | Mutate | Executes an approved TaskPlan through Bridge task-level execute and UE Task Runtime lowering | Yes | Returns task-level summary and validation result |
| `blueprinthelper_get_task_result` | Query | Reads `BlueprintHelper.TaskRunJournal.v1` task result summary | No | Used for async/follow-up result lookup, audit summaries, and optional adapter child result inspection |

Primary data structures:

| Schema | Consumer | Purpose |
|---|---|---|
| `BlueprintHelper.ReadSpec.v1` | MCP / Python Read Router | Agent-authored generic read request |
| `BlueprintHelper.TaskSpec.v1` | Python / MCP Task Compiler | Agent-authored semantic task specification |
| `BlueprintHelper.TaskPlan.v1` | Bridge task-level preview/execute, UE Task Runtime | Compiler-owned structured edit language / IR generated from TaskSpec |
| `BlueprintHelper.TaskRunJournal.v1` | UE / MCP query | Task-level journal grouping child transactions |
| `BlueprintHelper.TaskProtocolContract.v1` | Docs / tests | Versioned contract metadata for fixed TaskSpec and TaskPlan fields |

## Internal GraphWrite TaskPlan IR And Lowering

Ordinary Agents submit `BlueprintHelper.TaskSpec.v1`. They must not author `BlueprintHelper.TaskPlan.v1` directly. TaskPlan is a compiler-owned structured edit language / IR executed by UE Task Runtime through existing capability clusters.

Primary GraphWrite TaskPlan IR fields:

| Field | Meaning |
|---|---|
| `capability` | `graph_write` |
| `target.asset_path` | Target Blueprint asset |
| `target.graph` | Target graph or graph family |
| `write.strategy` | Structural write strategy such as `owned_graph_edit` |
| `write.ops[]` | Ordered structural edit ops such as `ensure_entry`, `replace_body`, `set_pin_default`, `set_node_comment`, `set_node_position`, `insert_flow` |
| `constraints.allow_modify_user_nodes` | Whether user-owned graph nodes may be modified |
| `constraints.ownership_scope` | Ownership boundary, for example `blueprinthelper_owned` |

Compiled first-class GraphWrite IR example:

```json
{
  "step_id": "step_001",
  "capability": "graph_write",
  "target": {
    "asset_path": "/Game/Blueprints/BP_StoneGate",
    "graph": "BH_StoneGateActivation"
  },
  "write": {
    "strategy": "owned_graph_edit",
    "ops": [
      {
        "op": "ensure_entry",
        "entry_type": "custom_event",
        "name": "InitializeStoneGate"
      },
      {
        "op": "replace_body",
        "entry_name": "InitializeStoneGate",
        "body": {
          "schema": "BlueprintLogicSpec.v1",
          "statements": [
            {
              "kind": "call_function",
              "name": "PrintString"
            }
          ]
        }
      }
    ]
  },
  "constraints": {
    "allow_modify_user_nodes": false,
    "ownership_scope": "blueprinthelper_owned"
  }
}
```

The following commands are Runtime lowering adapter targets, not the primary TaskPlan abstraction:

| Operation | Target fields | Args fields | Bridge dry-run placement |
|---|---|---|---|
| `append_blueprint_graph` | `asset_path`, `graph` | `feature_name`, `nodes`, `links` | root `dry_run` |
| `replace_blueprint_graph` | `asset_path`, `graph`, `replace_scope` | `selector`, `replacement.nodes`, `replacement.links`, `options.strict`, `options.preserve_layout` | `options.dry_run` |
| `patch_blueprint_graph` | `asset_path`, `graph`, `patch_scope` | `patch_type`, `patched_ref`, `patch`, `expected_old_state` | root `dry_run` |
| `merge_blueprint_graph` | `asset_path`, `graph`, `merge_scope`, `insert_strategy` | `anchor`, `inserted`, `sequence_order` | root `dry_run` |

Custom Event creation remains part of GraphWrite IR via an `ensure_entry` op and is lowered to `append_blueprint_graph` when appropriate. It is not a separate default Agent-facing tool, and the default TaskSpec-first surface does not add a separate custom-event mutation tool or event-listing tool.

Raw Bridge / UE operation errors are internal facts. Python / MCP normalizes them into task-level errors for ordinary Agent consumption. Debug / expert mode may expose raw trace references or summarized operation errors.

Current TaskSpec-first write slices:

| TaskSpec task_type | TaskPlan capability | Runtime adapter | Scope |
|---|---|---|---|
| `edit_blueprint_graph` | `graph_write` | `append_blueprint_graph` | `ensure_entry(custom_event)` plus supported simple statements |
| `edit_blueprint_variables` | `blueprint_variable` | `add_blueprint_member_variables` | `ensure_member_variable` only |

For `blueprint_variable`, preview is handled at UE Task Runtime level and does not call the mutating variable adapter during dry-run.

Canonical field contract: `Resources/Docs/TaskSpec_TaskPlan_Contract_20260504.md`.

## Token And Setup State

Current MCP Server behavior:

- `BLUEPRINTHELPER_BRIDGE_TOKEN` is forwarded to the Bridge when set.
- A formal setup/session token is not enforced by the TypeScript MCP Server yet.
- Future guard resources may require reading `.blueprinthelper/agent-profile.json` before writes.

Unless noted otherwise, Bridge-backed tools require Unreal Editor to be running with the BlueprintHelper Bridge reachable at `BRIDGE_HOST:BRIDGE_PORT`.

## Risk Levels

| Level | Meaning |
|---|---|
| Low | Read-only or validation-only |
| Medium | Opens assets, saves, compiles, or changes non-destructive structure |
| High | Deletes, imports, creates assets, or changes data |
| Critical | Can run arbitrary editor commands, close editor, or build project |

## Existing Low-Level Tool Reference

The following entries describe the current low-level inventory. Ordinary Agents should not use this table as the default planning surface for complex edits. These tools are legacy/internal/debug/expert capability entries unless a user explicitly requests direct use or a failure investigation requires it.

| Tool | Type | Bridge | Inputs | Success example | Failure example | Risk | Preconditions | Token/session |
|---|---|---:|---|---|---|---|---|---|
| `blueprinthelper_read_agent_guide` | Read | No | none | AgentGuide onboarding index Markdown | AgentGuide index file missing | Low | MCP Server can read plugin `Resources` | None |
| `blueprint_get_editor_context` | Read | Yes | none | `{ "success": true, "result": { "active_blueprint": "...", "active_graph": "..." } }` | No active editor context or Bridge unavailable | Low | Editor Bridge reachable | Optional Bridge token |
| `blueprint_validate_json` | Read | Yes | `json` string | `{ "success": true, "result": { "valid": true } }` | Invalid JSON or rule violation | Low | Editor Bridge reachable | Optional Bridge token |
| `blueprint_export_to_json` | Read | Yes | optional `target_blueprint`, optional `target_graph`, optional `scope` | `{ "success": true, "result": { "json": "..." } }` | Target asset or graph not found | Low | Editor Bridge reachable, target asset for explicit reads | Optional Bridge token |
| `blueprint_get_logic` | Read | Yes | optional `target_blueprint`, optional `target_graph`, optional `scope`, optional `detail`, optional `include_data_dependencies`, optional `include_orphans` | Markdown LogicMD plus optional safety summary | Target asset or graph not found | Low | Editor Bridge reachable | Optional Bridge token |
| `blueprint_get_logic_json` | Read | Yes | optional `target_blueprint`, optional `target_graph`, optional `scope`, optional `detail`, optional data/orphan/node/position/raw type flags | `{ "success": true, "result": { "format": "logic_json", "graphs": [] } }` | Target asset or graph not found | Low | Editor Bridge reachable | Optional Bridge token |
| `blueprint_import_json_to_graph` | Mutate | Yes | `json`, optional `target_blueprint`, optional `target_graph`, optional `compile_after_import`, optional `strict`, optional `allow_partial` | `{ "success": true, "result": { "nodes_created": 3, "links_connected": 2 } }` | Validation failed, link failed, rolled back | High | Explicit target recommended, raw BlueprintHelper JSON required | Optional Bridge token, setup not enforced |
| `blueprint_import_agent_graph` | Mutate | Yes | `schema`, `version`, `target_blueprint`, `target_graph`, `mode`, `layout`, optional `declarations`, `nodes`, optional `links`, optional `options` | `{ "success": true, "result": { "operations_applied": 4 } }` | Contract validation failed, unsupported node kind | High | Explicit Blueprint and graph required | Optional Bridge token, setup not enforced |
| `blueprint_compile_blueprint` | EditorCommand | Yes | optional `target_blueprint` | `{ "success": true, "result": { "compiled": true } }` | Compile error diagnostics | Medium | Editor Bridge reachable, target Blueprint or active Blueprint | Optional Bridge token |
| `blueprint_open_asset` | EditorCommand | Yes | `asset_path` | `{ "success": true, "message": "opened" }` | Asset not found | Medium | Editor Bridge reachable, asset path known | Optional Bridge token |
| `blueprint_list_assets` | Read | Yes | optional `path`, optional `class_filter`, optional `name_filter`, optional `recursive`, optional `max_results` | `{ "success": true, "result": { "assets": [] } }` | Invalid content path | Low | Editor Bridge reachable | Optional Bridge token |
| `blueprint_search_assets` | Read | Yes | `query`, optional `path`, optional `class_filter`, optional `max_results` | `{ "success": true, "result": { "assets": [] } }` | Invalid content path | Low | Editor Bridge reachable | Optional Bridge token |
| `blueprint_save_asset` | Mutate | Yes | `asset_path` | `{ "success": true, "message": "saved" }` | Save failed or asset not found | Medium | Editor Bridge reachable, asset path known | Optional Bridge token, profile may require confirmation |
| `blueprint_get_asset_info` | Read | Yes | `asset_path` | `{ "success": true, "result": { "class": "Blueprint" } }` | Asset not found | Low | Editor Bridge reachable | Optional Bridge token |
| `blueprint_list_graphs` | Read | Yes | optional `target_blueprint` | `{ "success": true, "result": { "graphs": [] } }` | Blueprint not found | Low | Editor Bridge reachable | Optional Bridge token |
| `blueprint_list_variables` | Read | Yes | optional `target_blueprint` | `{ "success": true, "result": { "variables": [] } }` | Blueprint not found | Low | Editor Bridge reachable | Optional Bridge token |
| `blueprint_list_event_dispatchers` | Read | Yes | optional `target_blueprint` | `{ "success": true, "result": { "dispatchers": [] } }` | Blueprint not found | Low | Editor Bridge reachable | Optional Bridge token |
| `blueprint_add_variable` | Mutate | Yes | optional `target_blueprint`, `name`, optional `pin_type`, optional `default_value`, optional `category`, optional `flags` | `{ "success": true, "result": { "variable": "Health" } }` | Duplicate name or invalid pin type | Medium | Explicit target recommended | Optional Bridge token, profile may require plan |
| `blueprint_remove_variable` | Mutate | Yes | optional `target_blueprint`, `name` | `{ "success": true, "message": "removed" }` | Variable used by graph or Blueprint not found | High | Explicit target recommended, read references first | Optional Bridge token, profile may require confirmation |
| `blueprint_add_graph` | Mutate | Yes | optional `target_blueprint`, `name`, optional `graph_type`, optional `inputs`, optional `outputs`, optional `is_pure` | `{ "success": true, "result": { "graph": "ComputeScore" } }` | Duplicate graph or invalid signature | Medium | Explicit target recommended | Optional Bridge token, profile may require plan |
| `blueprint_remove_graph` | Mutate | Yes | optional `target_blueprint`, `name` | `{ "success": true, "message": "removed" }` | Cannot remove EventGraph or graph not found | High | Explicit target recommended, read references first | Optional Bridge token, profile may require confirmation |
| `blueprint_add_event_dispatcher` | Mutate | Yes | optional `target_blueprint`, `name`, optional `params` | `{ "success": true, "result": { "dispatcher": "OnHealthChanged" } }` | Duplicate name or invalid parameter | Medium | Explicit target recommended | Optional Bridge token, profile may require plan |
| `blueprint_delete_nodes` | Mutate | Yes | optional `target_blueprint`, optional `target_graph`, `node_ids` | `{ "success": true, "result": { "operations_applied": 2 } }` | Protected node, missing node, rollback | High | Explicit Blueprint and graph strongly recommended | Optional Bridge token, profile should require confirmation |
| `blueprint_get_widget_tree` | Read | Yes | `asset_path` | `{ "success": true, "result": { "tree": {} } }` | WidgetBlueprint not found | Low | Editor Bridge reachable, WidgetBlueprint path known | Optional Bridge token |
| `blueprint_add_widget` | Mutate | Yes | `asset_path`, `widget_class`, optional `parent_name`, optional `widget_name` | `{ "success": true, "result": { "widget_name": "TitleText" } }` | Invalid class or parent not found | Medium | Read widget tree first | Optional Bridge token, profile may require plan |
| `blueprint_remove_widget` | Mutate | Yes | `asset_path`, `widget_name` | `{ "success": true, "message": "removed" }` | Widget not found or protected root | High | Read widget tree first | Optional Bridge token, profile should require confirmation |
| `blueprint_move_widget` | Mutate | Yes | `asset_path`, `widget_name`, `new_parent`, optional `insert_index` | `{ "success": true, "message": "moved" }` | Parent not found or invalid slot | Medium | Read widget tree first | Optional Bridge token, profile may require plan |
| `blueprint_get_widget_properties` | Read | Yes | `asset_path`, `widget_name` | `{ "success": true, "result": { "properties": [] } }` | Widget not found | Low | Widget path and name known | Optional Bridge token |
| `blueprint_set_widget_property` | Mutate | Yes | `asset_path`, `widget_name`, `property_name`, `value` | `{ "success": true, "message": "property set" }` | Invalid property or text import value | Medium | Read widget properties first | Optional Bridge token, profile may require confirmation |
| `blueprint_get_object_properties` | Read | Yes | `asset_path` | `{ "success": true, "result": { "properties": [] } }` | Asset not found or no editable fields | Low | UObject asset path known | Optional Bridge token |
| `blueprint_set_object_property` | Mutate | Yes | `asset_path`, `property_name`, `value` | `{ "success": true, "message": "property set" }` | Invalid property, unsafe flag, invalid value | Medium | Read object properties first | Optional Bridge token, profile may require confirmation |
| `blueprint_get_datatable_rows` | Read | Yes | `asset_path`, optional `row_names` | `{ "success": true, "result": { "rows": [] } }` | DataTable not found or row missing | Low | DataTable asset path known | Optional Bridge token |
| `blueprint_add_datatable_row` | Mutate | Yes | `asset_path`, `row_name`, optional `fields` | `{ "success": true, "message": "row added" }` | Duplicate row or invalid field value | High | Read table schema and rows first | Optional Bridge token, profile may require confirmation |
| `blueprint_update_datatable_row` | Mutate | Yes | `asset_path`, `row_name`, `fields` | `{ "success": true, "message": "row updated" }` | Row not found or invalid field value | Medium | Read target row first | Optional Bridge token, profile may require confirmation |
| `blueprint_delete_datatable_row` | Mutate | Yes | `asset_path`, `row_name` | `{ "success": true, "message": "row deleted" }` | Row not found | High | Read target row first | Optional Bridge token, profile should require confirmation |
| `blueprint_undo` | EditorCommand | Yes | none | `{ "success": true, "message": "undone" }` | Nothing to undo | Medium | Editor Bridge reachable | Optional Bridge token |
| `blueprint_redo` | EditorCommand | Yes | none | `{ "success": true, "message": "redone" }` | Nothing to redo | Medium | Editor Bridge reachable | Optional Bridge token |
| `blueprint_play_in_editor` | EditorCommand | Yes | none | `{ "success": true, "message": "PIE started" }` | PIE already running or editor not ready | Medium | Editor Bridge reachable | Optional Bridge token, profile may require permission |
| `blueprint_stop_pie` | EditorCommand | Yes | none | `{ "success": true, "message": "PIE stopped" }` | No PIE session running | Medium | Editor Bridge reachable | Optional Bridge token |
| `blueprint_create_blueprint` | Mutate | Yes | `asset_path`, optional `parent_class` | `{ "success": true, "result": { "asset_path": "/Game/..." } }` | Invalid path, duplicate asset, invalid parent class | High | Confirm destination path and parent class | Optional Bridge token, profile should require plan |
| `blueprint_exec_console_command` | EditorCommand | Yes | `command` | `{ "success": true, "result": { "output": "..." } }` | Console command failed | Critical | User should approve command | Optional Bridge token, profile should require confirmation |
| `blueprint_close_editor` | EditorCommand | Yes | optional `save_all` | `{ "success": true, "message": "closing" }` | Save failed or Bridge unavailable | Critical | User should approve closing editor | Optional Bridge token, profile should require confirmation |
| `blueprint_build_project` | LocalProcess | No | optional `target`, optional `configuration`, optional `platform` | `{ "success": true, "message": "Build succeeded." }` | Missing env vars or build failed with exit code | Critical | `UE_ENGINE_DIR` and `UE_PROJECT_FILE` set, editor should be closed | No Bridge token, setup not enforced |
| `blueprint_open_editor` | LocalProcess | Launch no, readiness ping yes | optional `wait_timeout_ms` | `{ "success": true, "code": "EDITOR_BRIDGE_AVAILABLE" }` | Missing env vars, bad `.uproject`, launch failure, Bridge timeout | Critical | `UE_ENGINE_DIR` and `UE_PROJECT_FILE` set | Bridge token used only for readiness ping when set |

## Write Tools Summary

The following low-level tools modify assets or editor state. They remain documented for legacy/internal/debug/expert use and for Task Runtime capability mapping:

```text
blueprint_import_json_to_graph
blueprint_import_agent_graph
blueprint_compile_blueprint
blueprint_open_asset
blueprint_save_asset
blueprint_add_variable
blueprint_remove_variable
blueprint_add_graph
blueprint_remove_graph
blueprint_add_event_dispatcher
blueprint_delete_nodes
blueprint_add_widget
blueprint_remove_widget
blueprint_move_widget
blueprint_set_widget_property
blueprint_set_object_property
blueprint_add_datatable_row
blueprint_update_datatable_row
blueprint_delete_datatable_row
blueprint_undo
blueprint_redo
blueprint_play_in_editor
blueprint_stop_pie
blueprint_create_blueprint
blueprint_exec_console_command
blueprint_close_editor
blueprint_build_project
blueprint_open_editor
```

For ordinary Agent editor-asset mutations, use TaskSpec-first orchestration:

```text
blueprinthelper_read_agent_guide
 -> blueprinthelper_get_runtime_profile
 -> blueprinthelper_read_context
 -> blueprinthelper_preview_task
 -> blueprinthelper_execute_task
 -> blueprinthelper_get_task_result when needed
```

The TaskSpec should state the exact target asset, target graph when relevant, allowed modification scope, resource references, failure policy, and validation policy.

## Input Notes

### Blueprint target fields

Several Blueprint tools accept:

```json
{
  "target_blueprint": "/Game/Blueprints/BP_Player.BP_Player",
  "target_graph": "EventGraph"
}
```

The source permits omitting these and falling back to active editor context. The safe workflow requires explicit values for destructive writes.

### Pin type shape

Variable, graph signature, and dispatcher parameter tools use:

```json
{
  "category": "bool",
  "sub_category": "",
  "object_path": "",
  "container_type": "None"
}
```

Typical categories include `bool`, `int`, `float`, `real`, `byte`, `name`, `string`, `text`, `object`, `class`, `struct`, `interface`, `softobject`, `softclass`, and `enum`.

### AgentImportGraph shape

`blueprint_import_agent_graph` expects semantic graph input:

This is a legacy/expert tool schema. Its `options.compile` and `options.save` fields are not TaskSpec fields and are not aliases for `validation.should_compile` / `validation.should_save`.

```json
{
  "schema": "BlueprintHelper.AgentImportGraph",
  "version": "1.0",
  "target_blueprint": "/Game/BP/BP_Player.BP_Player",
  "target_graph": "EventGraph",
  "mode": "append",
  "layout": "auto",
  "nodes": [
    { "id": "begin_play", "kind": "event", "event": "BeginPlay" },
    { "id": "print", "kind": "call", "function": "PrintString" }
  ],
  "links": [
    { "kind": "exec", "from": "begin_play.then", "to": "print.execute" }
  ],
  "options": {
    "compile": true,
    "save": false,
    "strict": true,
    "dry_run": false
  }
}
```

Supported semantic node kinds in the MCP schema are `event`, `custom_event`, `call`, `get`, `set`, `branch`, `sequence`, and `comment`.

## Resource-Related Tools

The MCP Server also exposes two resources:

```text
blueprint://rules/json-to-blueprint
blueprint://context/active-graph
```

Use `blueprinthelper_read_agent_guide` when a tool call is more convenient than opening the AgentGuide index file. Use `blueprint_get_editor_context` when a tool call is more convenient than reading the active graph resource. The JSON-to-Blueprint rules resource remains a legacy/debug reference for raw JSON workflows.
