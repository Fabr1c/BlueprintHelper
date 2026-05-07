# 04 - MCP Field Templates 20260507

This page documents only the normal Agent-facing surface and TaskSpec fields. Frozen compatibility tools stay registered in MCP, but their direct argument shapes are intentionally not documented here.

## 1. Transport Rules

- MCP tool arguments are the schema root object.
- Do not wrap tool calls in an extra `args` object.
- `blueprinthelper_preview_task` and `blueprinthelper_execute_task` wrap the TaskSpec under root field `task_spec`.
- Graph body function calls may use `args`; that is not an MCP transport wrapper.

## 2. Allowed Tool Names

Use `tools/list` as final authority. Normal Agent-facing tools:

| Purpose | Tool |
|---|---|
| Runtime profile | `blueprint_get_runtime_profile` |
| Static diagnostics | `blueprinthelper_diagnostics` |
| Runtime diagnostics | `blueprinthelper_diagnostics_runtime` |
| AgentGuide index | `blueprinthelper_read_agent_guide` |
| ReadSpec context | `blueprinthelper_read_context` |
| Task context | `blueprinthelper_read_task_context` |
| Reference context | `blueprinthelper_read_reference_context` |
| Preview TaskSpec | `blueprinthelper_preview_task` |
| Execute TaskSpec | `blueprinthelper_execute_task` |
| Query task result | `blueprinthelper_get_task_result` |
| Start target Editor preflight | `blueprint_open_editor` |

## 3. Root Argument Shapes

| Tool | Root argument shape |
|---|---|
| `blueprint_get_runtime_profile` | `{}` |
| `blueprinthelper_diagnostics` | `{}` |
| `blueprinthelper_diagnostics_runtime` | `{}` |
| `blueprinthelper_read_agent_guide` | `{}` |
| `blueprinthelper_read_context` | `BlueprintHelper.ReadSpec.v1` fields at root |
| `blueprinthelper_read_task_context` | `{ "target": { "asset_path": "..." }, "feature_name": "..." }` |
| `blueprinthelper_read_reference_context` | Reference fields at root |
| `blueprinthelper_preview_task` | `{ "task_spec": { ...BlueprintHelper.TaskSpec.v1... } }` |
| `blueprinthelper_execute_task` | `{ "task_spec": { ...BlueprintHelper.TaskSpec.v1... } }` |
| `blueprinthelper_get_task_result` | `{ "task_run_id": "..." }` |
| `blueprint_open_editor` | `{ "wait_timeout_ms": 120000 }` or `{}` |

## 4. Read Context Template

```json
{
  "schema": "required. Literal BlueprintHelper.ReadSpec.v1.",
  "read_type": "required. Currently blueprint_logic is the normal read path.",
  "target": {
    "asset_path": "required. UE asset path such as /Game/Blueprints/BP_Door.",
    "asset_type": "optional. Asset class hint. Usually omit.",
    "target_type": "optional, default blueprint. asset, blueprint, graph, function, event, custom_event, component, member_variable, event_dispatcher, widget, data_table_row, or block.",
    "target_name": "optional. Name for graph, function, event, widget, row, or other target.",
    "block_id": "optional. BlueprintHelper-owned block id when target_type is block."
  },
  "view": {
    "format": "optional, default logic_md. logic_md, logic_json, summary, or schema.",
    "max_items": "optional. Positive integer truncation guard.",
    "detail": "optional. brief, normal, full, or debug."
  },
  "context": {
    "context_id": "optional. Prior context id.",
    "task_run_id": "optional. Task run id when reading after a write."
  }
}
```

## 5. Reference Context Template

```json
{
  "asset_path": "required. UE asset path to analyze.",
  "target_type": "optional, default asset. asset, blueprint, graph, function, event, custom_event, member_variable, block, widget, data_table_row, or interface.",
  "target_name": "optional. Named target.",
  "graph_name": "optional. Graph name for graph-scoped checks.",
  "block_id": "optional. BlueprintHelper-owned block id.",
  "widget_name": "optional. Widget name.",
  "row_name": "optional. DataTable row name.",
  "interface_path": "optional. Interface asset path.",
  "scope": "optional, default safety_context. safety_context, dependencies, referencers, external_dependents, or all.",
  "max_results": "optional, default 50, max 500.",
  "include_samples": "optional, default true."
}
```

## 6. Task Tool Wrapper

```json
{
  "task_spec": {
    "schema": "required. Literal BlueprintHelper.TaskSpec.v1.",
    "context_id": "optional. Context id from read tools.",
    "task_type": "required. Supported task type.",
    "feature_name": "optional. Display or journal label only.",
    "target": {
      "asset_path": "required. UE asset path to create or edit.",
      "target_type": "optional, default blueprint. Use asset for asset fixture creation."
    },
    "behavior": "required for most task types. See templates below.",
    "scope_policy": "optional or required by graph and composite writes.",
    "asset_policy": "optional. Asset existence and reference policy.",
    "resources": "optional. Named asset references.",
    "components": "optional. Composite component section.",
    "variables": "optional. Composite variable section.",
    "class_settings": "optional. Composite class settings section.",
    "integration": "optional. Current executable slice supports interface integration.",
    "execution_policy": {
      "dry_run_mode": "optional, default full. none, quick, or full.",
      "on_missing_capability": "optional. Recommended stop_and_report."
    },
    "validation": {
      "should_compile": "optional, default false.",
      "should_save": "optional, default false."
    }
  }
}
```

Do not use `validation.compile` or `validation.save`.

## 7. Asset Fixture TaskSpec

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1",
    "task_type": "create_asset",
    "feature_name": "CreateSmokeFixture",
    "target": {
      "asset_path": "/Game/BlueprintHelperSmoke/BP_ClassSettingsSmoke",
      "target_type": "asset"
    },
    "behavior": {
      "asset_strategy": "ensure_asset",
      "asset": {
        "asset_type": "blueprint_class",
        "parent_class": "Actor",
        "collision_policy": "reuse_if_exists"
      }
    },
    "execution_policy": {
      "dry_run_mode": "full",
      "on_missing_capability": "stop_and_report"
    },
    "validation": {
      "should_compile": true,
      "should_save": false
    }
  }
}
```

Use canonical `asset_type=blueprint_class` plus explicit `parent_class=Actor` for ordinary Actor Blueprint fixtures.

## 8. Component Behavior

```json
{
  "component_strategy": "required. component_tree.",
  "changes": [
    {
      "kind": "required. ensure_component_present, configure_component, or remove_component.",
      "name": "required by practical operations. Component name.",
      "class": "optional. Component class for ensure_component_present.",
      "attach": "optional. Attach target, socket, and rule object.",
      "on_name_conflict": "optional. Conflict policy.",
      "properties": "optional. Array of property settings."
    }
  ]
}
```

## 9. Class Settings Behavior

```json
{
  "class_settings_strategy": "required. class_settings.",
  "interfaces": {
    "ensure_present": "optional. Array of interface asset paths to add.",
    "ensure_absent": "optional. Array of interface asset paths to remove."
  },
  "class_defaults": "optional. Array of property setting objects.",
  "parent_class": "not supported here. Set parent when creating the Blueprint asset."
}
```

## 10. UMG Widget Behavior

```json
{
  "widget_strategy": "required. widget_blueprint_edit.",
  "changes": [
    {
      "kind": "required. create_widget, update_widget_property, or delete_widget.",
      "widget_name": "required by named widget operations.",
      "widget_class": "optional. Widget class for create_widget.",
      "parent_widget_name": "optional. Parent widget name.",
      "property_name": "optional. Property name.",
      "property_path": "optional. Property path.",
      "value": "optional. New property value."
    }
  ]
}
```

## 11. DataTable Behavior

```json
{
  "row_strategy": "required. row_edit.",
  "rows": [
    {
      "action": "required. add, update, or delete.",
      "row_name": "required. DataTable row name.",
      "fields": "optional. Object of field names to values for add or update."
    }
  ]
}
```

## 12. Object Property Behavior

```json
{
  "property_strategy": "required. property_edit.",
  "changes": [
    {
      "kind": "optional. set_property or set_object_property.",
      "property_path": "required. UObject property path.",
      "value": "required. New value."
    }
  ]
}
```

## 13. Graph Write Behavior

```json
{
  "scope_policy": {
    "graph_name": "required. Target graph name.",
    "allow_modify_user_nodes": "optional, default false."
  },
  "behavior": {
    "graph_strategy": "required. append_new_owned_graph, replace_owned_graph, patch_owned_graph, or merge_owned_graph.",
    "entries": "required for append_new_owned_graph.",
    "replace": "required for replace_owned_graph.",
    "patches": "required for patch_owned_graph.",
    "merges": "required for merge_owned_graph."
  }
}
```

Block-scoped anchors must come from `blueprinthelper_read_context` with `view.format=logic_json`:

```json
{
  "block_id": "required. BlueprintHelper-owned block id.",
  "group_entry_node_path": "required. Group entry node path.",
  "node_ref": "required. Node reference.",
  "pin_ref": "required for pin anchors.",
  "link_ref": "required for insert_between, optional for append_after."
}
```

## 14. Function Call Body Statement

```json
{
  "kind": "call_function",
  "name": "PrintString",
  "args": {
    "InString": {
      "kind": "literal",
      "value_type": "string",
      "value": "message"
    }
  }
}
```

## 15. Task Result Query

```json
{
  "task_run_id": "required. Exact task_run_id returned by blueprinthelper_execute_task."
}
```
