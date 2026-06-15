---
name: blueprint-helper-blueprint-explorer
description: Fork the blueprint-explorer sideAgent to collect compact BlueprintHelper UE editor-asset context. Use after Main Agent preflight when Blueprint, UMG, DataAsset, DataTable, graph, variable, component, or Bridge/runtime context is needed.
context: fork
agent: blueprint-explorer
---

# BlueprintHelper Blueprint Explorer Fork

Invoke the `blueprint-explorer` sideAgent with the smallest possible task package.

The Main Agent, not this sideAgent, owns user clarification, lifecycle MCP, safety/write boundary decisions, and final response.

Input must be compact YAML:

```yaml
user_goal: "<what the user wants>"
target_asset_path: "<UE asset path>"
target_graph_or_scope: "<graph/function/event/widget/table/object scope>"
operation_mode: "create_new | modify_existing | inspect_only | validate_only"
requested_context: []
read_strategy: "<find_assets | summary | logic_flow | logic_json_delta_after_logic_flow | bounded_logic_json | reference_context | function_chain_context>"
allowed_tools: []
stop_conditions: []
reasoning: "maximum_available"
```

Use `logic_json_delta_after_logic_flow` only after the same target's `logic_flow` has been read and the Main Agent needs GraphWrite anchors, pins, links, or boundary evidence. If delta lacks required write-location evidence or conflicts with the earlier `logic_flow`, return the blocker instead of switching to binary asset reads or plugin source inspection.

Return only the sideAgent's compact YAML result to the Main Agent.

