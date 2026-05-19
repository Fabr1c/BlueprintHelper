# Read Template Semantic Index

Use read templates when the Agent needs facts before planning, previewing,
executing, repairing, or reporting a BlueprintHelper task. These templates do
not request write permission.

## Task And Asset Orientation

| Intent | Template | Command |
|---|---|---|
| Build a compact task-context pack for a feature request | `blueprinthelper_read_task_context_template.json` | `blueprinthelper_read_task_context` |
| Get a compact asset summary before deeper reads | `read_context_asset_summary_template.json` | `blueprinthelper_read_context` |
| Discover supported ReadContext capability matrix | `blueprinthelper_read_context_capabilities_template.json` | `blueprinthelper_read_context_capabilities` |

## Blueprint Logic Reads

| Intent | Template | Command |
|---|---|---|
| Read a graph as structured anchors for patches or merges | `read_context_graph_logic_json_template.json` | `blueprinthelper_read_context` |
| Read graph context without a full logic dump | `read_context_graph_context_template.json` | `blueprinthelper_read_context` |
| Read a BlueprintHelper-owned block with stable anchors | `read_context_block_logic_json_template.json` | `blueprinthelper_read_context` |
| Read one simple function body as LogicFlow | `read_context_function_logic_flow_template.json` | `blueprinthelper_read_context` |
| Read one simple event body as LogicFlow | `read_context_event_logic_flow_template.json` | `blueprinthelper_read_context` |
| Read one simple custom event body as LogicFlow | `read_context_custom_event_logic_flow_template.json` | `blueprinthelper_read_context` |
| Read one function body as LogicMD | `read_context_function_logic_md_template.json` | `blueprinthelper_read_context` |
| Read one event body as LogicMD | `read_context_event_logic_md_template.json` | `blueprinthelper_read_context` |
| Read one custom event body as LogicMD | `read_context_custom_event_logic_md_template.json` | `blueprinthelper_read_context` |
| Trace project-authored function/event/custom-event calls reachable from one entry | `blueprinthelper_read_function_chain_context_template.json` | `blueprinthelper_read_function_chain_context` |

## Reference And Dependency Context

| Intent | Template | Command |
|---|---|---|
| Read asset-level dependency and referencer safety context | `blueprinthelper_read_reference_context_safety_template.json` | `blueprinthelper_read_reference_context` |
| Read compact asset dependency context | `blueprinthelper_read_reference_context_dependencies_template.json` | `blueprinthelper_read_reference_context` |
| Read project-scope external dependent samples | `blueprinthelper_read_reference_context_external_dependents_template.json` | `blueprinthelper_read_reference_context` |
| Find where a function is referenced | `blueprinthelper_read_reference_context_function_template.json` | `blueprinthelper_read_reference_context` |
| Find where a member variable is read or written | `blueprinthelper_read_reference_context_member_variable_template.json` | `blueprinthelper_read_reference_context` |
| Find where a local variable is used inside a graph | `blueprinthelper_read_reference_context_local_variable_template.json` | `blueprinthelper_read_reference_context` |
| Find where an event dispatcher is bound, called, or cleared | `blueprinthelper_read_reference_context_event_dispatcher_template.json` | `blueprinthelper_read_reference_context` |

## Structure, Data, And UI Reads

| Intent | Template | Command |
|---|---|---|
| Read Blueprint components | `read_context_components_template.json` | `blueprinthelper_read_context` |
| Read member variables | `read_context_variables_template.json` | `blueprinthelper_read_context` |
| Read event dispatchers | `read_context_event_dispatchers_template.json` | `blueprinthelper_read_context` |
| Read object property path/value context | `read_context_object_property_template.json` | `blueprinthelper_read_context` |
| Read UMG WidgetTree | `read_context_widget_tree_template.json` | `blueprinthelper_read_context` |
| Read one widget property | `read_context_widget_property_template.json` | `blueprinthelper_read_context` |
| Read DataAsset context | `read_context_data_asset_template.json` | `blueprinthelper_read_context` |
| Read DataTable metadata and rows | `read_context_data_table_template.json` | `blueprinthelper_read_context` |
| Read one DataTable row | `read_context_data_table_row_template.json` | `blueprinthelper_read_context` |
