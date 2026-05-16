---
name: blueprint-helper-task-worker
description: Fork the task-worker sideAgent to construct a template-first BlueprintHelper.TaskSpec.v1, run preview/execute through CLI, and return compact diagnostics.
context: fork
agent: task-worker
---

# BlueprintHelper Task Worker Fork

Invoke the `task-worker` sideAgent with one bounded task package.

The Main Agent, not this sideAgent, owns user clarification, lifecycle MCP, safety/write boundary decisions, closed-loop decisions, and final response.

Input must be compact YAML:

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
reasoning: "maximum_available"
```

Return only the sideAgent's compact YAML result to the Main Agent.

