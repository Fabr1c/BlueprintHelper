# 04 - Tool Surface Field Templates 20260512

This page documents only the normal Agent-facing tool surface and TaskSpec fields. Compatibility-only transports or wrappers may still exist, but their direct argument shapes are intentionally not documented here.

Concrete copy-and-edit JSON files live in `AgentFaceService/agent-guide/Templates/`.
Use those templates for routine CLI work; this reference explains the field
contract behind them.

## 1. Call Shape Rules

- Tool arguments use the schema root object.
- Do not wrap tool calls in an extra `args` object.
- `blueprinthelper_preview_task` and `blueprinthelper_execute_task` tool-name inputs prefer wrapping the TaskSpec under root field `task_spec`; grouped `task preview` / `task execute` commands use a bare TaskSpec file.
- Graph body function calls may use `args`; that is not a BlueprintHelper tool wrapper.

## 1.1 CLI Output Trimming

The CLI summary output is intentionally small, and full payloads are written to artifacts. Use `--select` or `--fields` whenever stdout size matters.

Use `--omit` when the default summary is useful but specific fields should be removed:

```powershell
bh blueprinthelper_preview_task --file preview-wrapper.json --omit operation,status
```

Use `--select` / `--fields` when only a small whitelist is needed:

```powershell
bh blueprinthelper_execute_task --file execute-wrapper.json --select task_run_id,artifacts.full_result
bh task result --id task_xxx --select summary.target_assets,artifacts.full_result
```

Use `--max-bytes <n>` as a hard stdout budget. When the budget is exceeded, read `artifacts.full_result` instead of rerunning the command with a broader selection.

`artifacts.full_result` is compact by default. It keeps the CLI-level `BlueprintHelper.CliFullResult.v1` schema but removes nested ToolResult schemas, trace ids, raw `bridge_result`, duplicate TaskSpec `target_assets`, and duplicate nested `task_run_id`. Add `--expert` only when a raw diagnostic `artifacts.debug_result` is needed.

## 2. Allowed Tool Names

Use `tools/list` as final authority. Normal Agent-facing tools:

| Purpose | Tool |
|---|---|
| Runtime profile | `blueprint_get_runtime_profile` |
| Static diagnostics | `blueprinthelper_diagnostics` |
| Runtime diagnostics | `blueprinthelper_diagnostics_runtime` |
| Request write session | `blueprinthelper_request_write_session` |
| AgentGuide index | `blueprinthelper_read_agent_guide` |
| ReadSpec context | `blueprinthelper_read_context` |
| ReadContext capabilities | `blueprinthelper_read_context_capabilities` |
| Task context | `blueprinthelper_read_task_context` |
| Reference context | `blueprinthelper_read_reference_context` |
| Function chain context | `blueprinthelper_read_function_chain_context` |
| Preview TaskSpec | `blueprinthelper_preview_task` |
| Execute TaskSpec | `blueprinthelper_execute_task` |
| Query task result | `blueprinthelper_get_task_result` |
| Debug case summary | `blueprinthelper_get_debug_case` |
| Debug case list | `blueprinthelper_list_debug_cases` |
| Debug bundle manifest | `blueprinthelper_export_debug_bundle` |
| Review record summary query | `blueprinthelper_query_review_records` |

Lifecycle companion tools are available only through the global MCP allowlist for Agent-owned Editor open/close. Compatibility for `blueprint_open_editor` / `blueprint_close_editor` also maps to the global MCP lifecycle tools; ordinary Agents must not use CLI lifecycle helpers. Deprecated MCP ordinary tools are not ordinary Agent entry points or fallback paths.

`blueprinthelper_apply_review_action` is plugin-development/internal and is intentionally omitted from ordinary Agent-facing templates.

## 3. Root Argument Shapes

| Tool | Root argument shape |
|---|---|
| `blueprint_get_runtime_profile` | `{}` |
| `blueprinthelper_diagnostics` | `{}` |
| `blueprinthelper_diagnostics_runtime` | `{}` |
| `blueprinthelper_request_write_session` | `{ "reason": "...", "scope": "project", "ttl_seconds": 900 }` or `{ "reason": "...", "scope": "asset_list", "ttl_seconds": 900, "asset_paths": ["/Game/..."] }` |
| `blueprinthelper_read_agent_guide` | `{}` |
| `blueprinthelper_read_context` | `BlueprintHelper.ReadSpec.v1` fields at root |
| `blueprinthelper_read_context_capabilities` | `{}` |
| `blueprinthelper_read_task_context` | `{ "target": { "asset_path": "..." }, "feature_name": "..." }` |
| `blueprinthelper_read_reference_context` | Reference fields at root |
| `blueprinthelper_read_function_chain_context` | Function chain fields at root |
| `blueprinthelper_preview_task` | `{ "task_spec": { ...BlueprintHelper.TaskSpec.v1... } }` |
| `blueprinthelper_execute_task` | `{ "task_spec": { ...BlueprintHelper.TaskSpec.v1... } }` |
| `blueprinthelper_get_task_result` | `{ "task_run_id": "..." }` |
| `blueprinthelper_get_debug_case` / `blueprinthelper_export_debug_bundle` | `{ "debug_case_id": "..." }` |
| `blueprinthelper_list_debug_cases` | `{ "limit": 20 }` |
| `blueprinthelper_query_review_records` | `{ "asset_path": "...", "task_run_id": "...", "pending_only": true }` |

`blueprinthelper_request_write_session` is only called after a successful preview when `write_permission` is disabled. The running Editor shows a minimal accept/reject prompt. The approval is owned by the running Editor/Bridge for the approved scope and lifetime, and can be used by delegated SideAgents. The tool response omits the raw session id; Agents must not pass `auth_session`, `auth_token`, or `BLUEPRINTHELPER_BRIDGE_TOKEN` in later tool calls.

## 4. Read Context Template

```json
{
  "schema": "required. Literal BlueprintHelper.ReadSpec.v1.",
  "read_type": "required. Currently blueprint_logic is the normal read path.",
  "target": {
    "asset_path": "required. UE asset path such as /Game/Blueprints/BP_Door.",
    "asset_type": "optional. Asset class hint. Usually omit.",
    "target_type": "optional, default blueprint. asset, blueprint, graph, function, event, custom_event, component, member_variable, event_dispatcher, widget, data_table, data_table_row, data_asset, object_property, property, or block.",
    "target_name": "optional. Name for graph, function, event, widget, row, or other target.",
    "block_id": "optional. BlueprintHelper-owned block id when target_type is block."
  },
  "view": {
    "format": "optional for logic reads; defaults to logic_flow for blueprint_logic. logic_flow, logic_md, or logic_json. Omit for non-logic read types.",
    "max_items": "optional. Positive integer truncation guard.",
    "detail": "optional. brief, normal, full, or debug."
  },
  "context": {
    "context_id": "optional. Prior context id.",
    "task_run_id": "optional. Task run id when reading after a write."
  }
}
```

`view.format=summary` is removed from ReadSpec. Non-logic reads use their `read_type` as the compact view contract and should omit `view.format`.

`view.format=logic_flow` is the default for `read_type=blueprint_logic` and is recommended for simple `target_type=function`, `target_type=event`, or `target_type=custom_event` reads when the Agent needs fast execution/data flow understanding. It returns `LogicFlow.v1` and must not be used as a patch/merge anchor source.

`view.format=logic_md` may be used directly for `target_type=function`, `target_type=event`, or `target_type=custom_event` when `target_name` is known and the entry is larger or more branched than a compact `logic_flow` read should carry. These target-entry reads are generated from structured target slices and report sliced stats. Do not use whole-graph `logic_md` until `logic_json` shows the graph is small enough.

`view.max_items` is a truncation guard for `logic_json`; when truncation happens, the tool result sets `data.truncated=true` and includes `payload.truncation.nodes_total` plus `payload.truncation.nodes_returned`.

If `blueprinthelper_read_context` is not visible or callable, stop with `tool_unavailable`. Do not read `.vs\BlueprintCache`, Saved exports, or local JSON files as a substitute for missing BlueprintHelper tool availability.

## 4.5 Read Context Capabilities Template

Use this local read-only tool when an Agent needs to know which ReadContext read types, asset target types, and formats are currently supported. It does not read UE assets or call Bridge.

```json
{}
```

Returned data schema is `ReadContextCapabilities.v1`. `asset_types`, `formats`, and `read_type_ids` are the full sets. `read_types[]` is a negative capability diff: each row lists only `unsupported_asset_types` and `unsupported_formats` for that read type.

## 5. Reference Context Template

```json
{
  "asset_path": "required. UE asset path to analyze.",
  "target_type": "optional, default asset. asset, blueprint, graph, function, event, custom_event, member_variable, local_variable, event_dispatcher, block, widget, data_table_row, or interface.",
  "target_name": "optional. Named target.",
  "graph_name": "optional. Graph name for graph-scoped checks.",
  "declaring_class_path": "optional. Native or generated class path used to disambiguate inherited members.",
  "block_id": "optional. BlueprintHelper-owned block id.",
  "widget_name": "optional. Widget name.",
  "row_name": "optional. DataTable row name.",
  "interface_path": "optional. Interface asset path.",
  "search_scope": "optional, default project. asset or project.",
  "resolution_policy": "optional, default ue_then_name. ue_then_name, ue_only, or name_only.",
  "detail": "optional, default samples. summary, samples, or full.",
  "max_results": "optional, default 50, max 500."
}
```

For member-level targets (`function`, `event`, `custom_event`,
`member_variable`, `local_variable`, or `event_dispatcher`), `target_name` is
required. For `local_variable`, `graph_name` is also required. Do not send
`target_guid`, `scope`, or `include_samples`; those fields are not part of the
current Agent-facing contract.

## 5.5 Function Chain Context Template

Use this read-only tool when an Agent needs the next project-authored Blueprint logic entries reachable from one function/event/custom event. It returns an index for follow-up `blueprinthelper_read_context` calls, not full graph bodies.

```json
{
  "asset_path": "required. UE Blueprint asset path such as /Game/BP_PlayerController.",
  "target_type": "required. function, event, or custom_event.",
  "target_name": "required. Entry function or event name.",
  "graph_name": "optional. Graph name for event/custom_event disambiguation.",
  "max_depth": "optional, default 3. Cross custom-logic expansion depth.",
  "include_data_dependencies": "optional, default true. Include custom pure functions feeding branch conditions, arguments, returns, or set values.",
  "expand_cross_asset": "optional, default true. Expand uniquely resolved project Blueprint calls in other assets."
}
```

Returned data schema is `FunctionChainContext.v1`. `custom_logic_refs[]` does not include `node_ref`, `node_path`, owner fields, or raw GUID fields. Do not send `target_guid`, `entry`, `target`, `query`, or owner fields.

## 6. Task Tool Wrapper

Use this wrapper with `blueprinthelper_preview_task` and
`blueprinthelper_execute_task`. Use a bare `BlueprintHelper.TaskSpec.v1` root
with grouped `task preview --file` and `task execute --file`.

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

Compile validation is only for assets that have a Blueprint compile step.

| Asset or operation | `validation.should_compile` | Validation rule |
|---|---:|---|
| `blueprint_class`, Blueprint graph, components, variables, class settings, signatures | `true` | Compile plus read-back |
| `blueprint_interface` | `true` | Compile plus read-back |
| `widget_blueprint`, UMG widget edits | `true` | Compile plus widget-tree read-back |
| DataAsset Blueprint class fixture (`asset_type=blueprint_class`, `parent_class=PrimaryDataAsset`) | `true` | Compile plus class parent read-back |
| `structure` / UserDefinedStruct | `false` | No Blueprint compile; verify fields by read-back |
| `data_table` creation or row edits | `false` | No Blueprint compile; verify row struct and rows by read-back |
| `data_asset` instance creation or property edits | `false` | No Blueprint compile; verify asset class and properties by read-back |
| `input_action`, `input_mapping_context`, plain UObject properties | `false` | No Blueprint compile; verify properties by read-back |

Do not report a skipped compile or `no_op` reuse as a compile failure for non-Blueprint data assets. For idempotent fixtures, `no_op` is acceptable only when read-back proves the existing asset matches the requested type and content.

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

Agent-facing `create_asset` fields under `behavior.asset`:

| Field | Required | Purpose |
|---|---|---|
| `asset_type` | Required | Semantic asset kind. Prefer canonical values such as `blueprint_class`, `blueprint_interface`, `structure`, `input_action`, `input_mapping_context`, `data_asset`, `data_table`, and `widget_blueprint`. Accepted aliases include `blueprint`, `actor`, `Actor`, and `blueprintclass` for `blueprint_class`, plus `datatable` for `data_table`. |
| `parent_class` | Conditional | Parent UObject class for Blueprint-class, WidgetBlueprint, or DataAsset-style assets when the factory needs a base class. For ordinary Actor Blueprint fixtures, set `parent_class=Actor` or `/Script/Engine.Actor` even if an alias would normalize to Actor. |
| `fields` | Conditional | Structure field definitions for `asset_type=structure`. Each entry names a field and type; optional `default_value` may seed the struct field. This is not DataTable row data. |
| `row_struct` | Conditional | Required for `asset_type=data_table` or alias `datatable`. It points to the row UStruct asset used by the DataTable. Row values are edited later through the DataTable TaskSpec flow. |
| `data_asset_class` | Conditional | Required for `asset_type=data_asset`. Must be a concrete `UDataAsset` subclass. In a new project, first create a Blueprint class with `asset_type=blueprint_class` and `parent_class=PrimaryDataAsset`, then pass that Blueprint asset path or generated class path here. Do not pass `/Script/Engine.DataAsset` or `/Script/Engine.PrimaryDataAsset`. |
| `collision_policy` | Optional | Asset collision handling. Prefer `reuse_if_exists` for idempotent smoke fixtures or `fail_if_exists` when reuse would hide a setup problem. |

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
      "parent_widget_name": "optional. Parent widget name. Omit or pass an empty string when creating the root widget; widget_class controls the root type and the UE layer still validates root/container compatibility.",
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

## 13. Function/Event Signature Behavior

Function/Event signature edits are expressed through `task_type=edit_blueprint_signature`. These are TaskSpec fields only; Agents must not call any low-level signature or graph tools directly.

```json
{
  "signature_strategy": "required. signature_edit.",
  "changes": [
    {
      "kind": "required. ensure_function, ensure_interface_function, ensure_custom_event, ensure_interface_event, ensure_event_dispatcher, ensure_override_event, or remove_signature.",
      "function_name": "required for ensure_function or ensure_interface_function.",
      "event_name": "required for ensure_custom_event, ensure_interface_event, ensure_override_event, or native event entries.",
      "dispatcher_name": "required for ensure_event_dispatcher and event_dispatcher remove_signature.",
      "graph_name": "required for custom/interface event entries; optional for override/native events, default EventGraph.",
      "inputs": "optional. Array of pin specs for function, custom event, interface event, dispatcher, or override/native event signatures.",
      "outputs": "optional. Array of pin specs for function signatures.",
      "interface_path": "required for interface function or interface event entries.",
      "event_kind": "optional for ensure_override_event. native_event or override_event.",
      "execute_policy": "optional. Defaults to blocked_preflight; ensure_override_event may use create_if_missing when the caller explicitly wants to create a missing native/override event entry.",
      "signature_mismatch_policy": "optional. For ensure_event_dispatcher, use block.",
      "signature_kind": "required for remove_signature unless it can be inferred from function_name, event_name, or dispatcher_name.",
      "require_reference_context": "required true for remove_signature. Real removal remains blocked until reference-analysis cleanup policy lands."
    }
  ]
}
```

For `replace_owned_graph` with `replace.scope=custom_event_definition`, the compiler splits the TaskSpec into two TaskPlan steps: first `blueprint_signature.ensure_custom_event`, then `graph_write.replace_body` with `custom_event_body`. The body remains GraphWrite-owned; the event declaration and pin signature remain Signature-owned.

## 14. Graph Write Behavior

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

For `merge_owned_graph` with `branch_fork`, keep the TaskSpec semantic and provide the strategy-specific fields:

```json
{
  "kind": "insert_flow",
  "scope": "owned_block_call",
  "insert_strategy": "branch_fork",
  "anchor": {
    "block_id": "required. Existing BlueprintHelper-owned anchor block id.",
    "group_entry_node_path": "required. Group entry node path from logic_json.",
    "node_ref": "required. Anchor node ref inside the owned block.",
    "pin_ref": "required. Anchor exec pin ref.",
    "link_ref": "optional. Existing successor link ref when available."
  },
  "inserted": {
    "call_kind": "owned_block_call",
    "block_id": "required. Existing BlueprintHelper-owned CustomEvent block to call."
  },
  "sequence_order": [
    "original_successor",
    "inserted_logic"
  ]
}
```

`sequence_order` is required only for `branch_fork`, and values must be `original_successor` and `inserted_logic`. Preview must resolve `inserted.block_id` to a BlueprintHelper-owned CustomEvent block; missing, non-owned, or non-CustomEvent targets are preview blockers.

After a successful `branch_fork` execute, read back LogicJson or LogicMd and verify the inserted Sequence or equivalent distribution node, the inserted call, and the original successor are reachable from the anchor, with no orphaned nodes in the affected flow.

## 15. Function Call Body Statement

Prefer the short GraphStatement form `kind="call"` + `target`. The legacy-compatible `kind="call_function"` + `name` remains accepted by the compiler. The target/name may be a native function name, a Blueprint display name, an owner-qualified native name, or an explicit component/member call for append-owned graph writes. Preview resolves the function against the target Blueprint graph. If the target is ambiguous, use an owner-qualified native name and preview again.

```json
{
  "kind": "call",
  "target": "PrintString",
  "args": {
    "InString": {
      "kind": "literal",
      "value_type": "string",
      "value": "message"
    }
  }
}
```

Append-owned graph writes may use explicit component/member calls. The object prefix must name a Blueprint member or component that can be read through a generated getter, and the function part is resolved by the UE graph-aware resolver:

```json
{
  "kind": "call",
  "target": "DoorMesh.AddAngularImpulseInDegrees",
  "args": {}
}
```

## 16. Task Result Query

```json
{
  "task_run_id": "required. Exact task_run_id returned by blueprinthelper_execute_task."
}
```

