# Write Template Semantic Index

Use write templates only for TaskSpec-first preview and execute workflows.
Preview before execute, request a write session only when preview reports that
write permission is disabled, and read back the target context after execute.

## Preview And Execute Entrypoints

| Intent | Template | Command |
|---|---|---|
| Preview a direct tool-name TaskSpec wrapper | `blueprinthelper_preview_task_wrapper_template.json` | `blueprinthelper_preview_task` |
| Execute a direct tool-name TaskSpec wrapper | `blueprinthelper_execute_task_wrapper_template.json` | `blueprinthelper_execute_task` |
| Preview a bare TaskSpec through grouped CLI | `task_preview_bare_taskspec_template.json` | `task preview` |
| Execute a bare TaskSpec through grouped CLI | `task_execute_bare_taskspec_template.json` | `task execute` |

## Asset Fixtures And Asset Creation

| Intent | Template |
|---|---|
| Create or ensure a Blueprint class asset | `taskspec_create_asset_blueprint_class_template.json` |
| Create or ensure a Widget Blueprint asset | `taskspec_create_asset_widget_blueprint_template.json` |
| Create or ensure a UserDefinedStruct asset | `taskspec_create_asset_structure_template.json` |
| Create or ensure a DataTable asset | `taskspec_create_asset_data_table_template.json` |
| Create or ensure a DataAsset instance | `taskspec_create_asset_data_asset_template.json` |

## Composite Blueprint Feature Authoring

| Intent | Template |
|---|---|
| Create a compact Blueprint feature with variables, graph, and validation | `taskspec_create_blueprint_feature_template.json` |
| Edit Blueprint components | `taskspec_edit_blueprint_components_template.json` |
| Edit Blueprint member variables | `taskspec_edit_blueprint_variables_template.json` |
| Edit Blueprint class settings, interfaces, class defaults, or reparenting | `taskspec_edit_blueprint_class_settings_template.json` |

## Signature Authoring

| Intent | Template |
|---|---|
| Ensure a function signature | `taskspec_edit_blueprint_signature_function_template.json` |
| Ensure a custom event signature | `taskspec_edit_blueprint_signature_custom_event_template.json` |
| Ensure an event dispatcher signature | `taskspec_edit_blueprint_signature_dispatcher_template.json` |
| Request signature removal with reference-context guard | `taskspec_edit_blueprint_signature_remove_template.json` |

## Graph Body Edits

| Intent | Template |
|---|---|
| Append a new owned graph block | `taskspec_graph_append_owned_template.json` |
| Append first-class container actions such as array/map/set operations | `taskspec_graph_append_container_action_template.json` |
| Append component-bound events or delegate bind/assign/unbind/call statements | `taskspec_graph_append_event_delegate_template.json` |
| Append generic schedule nodes without FunctionAction ownership mixing | `taskspec_graph_append_generic_schedule_template.json` |
| Append representative generic op, transform, struct/select, create, and branch statements | `taskspec_graph_append_generic_ops_template.json` |
| Replace an owned graph block | `taskspec_graph_replace_owned_template.json` |
| Patch node comment | `taskspec_graph_patch_node_comment_template.json` |
| Patch node position | `taskspec_graph_patch_node_position_template.json` |
| Patch pin default value | `taskspec_graph_patch_pin_default_template.json` |
| Merge by appending after an owned anchor | `taskspec_graph_merge_append_after_template.json` |
| Merge by inserting between owned flow links | `taskspec_graph_merge_insert_between_template.json` |
| Merge by branching from an owned flow anchor | `taskspec_graph_merge_branch_fork_template.json` |
| Merge a new BlueprintHelper-owned body into an external user-authored exec flow | `taskspec_graph_merge_external_flow_template.json` |

## UMG, Data, And UObject Edits

| Intent | Template |
|---|---|
| Edit UMG WidgetTree or widget properties | `taskspec_edit_umg_widget_template.json` |
| Edit DataTable rows | `taskspec_edit_data_table_rows_template.json` |
| Edit UObject or asset properties | `taskspec_edit_object_properties_template.json` |
