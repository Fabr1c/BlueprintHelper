# BlueprintHelper MCP Tools API Reference

文档版本：2026-04-30

This reference is aligned with `MCPServer/src/tools.ts` in the current source tree. It covers 44 registered MCP tools.

## Common Return Shape

Bridge-backed tools usually return a text MCP result containing a JSON object:

```json
{
  "request_id": "mcp_1_1710000000000",
  "success": true,
  "message": "optional message",
  "result": {}
}
```

Failure shape:

```json
{
  "request_id": "mcp_1_1710000000000",
  "success": false,
  "error_code": "OPTIONAL_CODE",
  "message": "failure details",
  "result": {}
}
```

Transport or Bridge connection failure:

```text
Bridge error: Bridge connection error: connect ECONNREFUSED 127.0.0.1:54321
```

Some newer write commands may include safety fields:

```json
{
  "success": true,
  "result": {
    "effective_scope": "graph",
    "status": "applied",
    "operations_applied": 3,
    "nodes_created": 2,
    "links_connected": 1,
    "warnings": [],
    "errors": [],
    "rolled_back": false
  }
}
```

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

## Tool Reference

| Tool | Type | Bridge | Inputs | Success example | Failure example | Risk | Preconditions | Token/session |
|---|---|---:|---|---|---|---|---|---|
| `blueprint_get_rule_markdown` | Read | Yes | none | `{ "success": true, "result": { "markdown": "..." } }` or Markdown text | Bridge unavailable | Low | Editor Bridge reachable | Optional Bridge token |
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

The following tools modify assets or editor state:

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

Use read -> plan -> write -> compile/validate -> save for editor-asset mutations.

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

Use `blueprint_get_rule_markdown` when a tool call is more convenient than reading the rules resource. Use `blueprint_get_editor_context` when a tool call is more convenient than reading the active graph resource.
