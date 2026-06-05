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
source_control_policy: "<checkout/status policy for target assets before execute>"
allowed_tools: []
tool_id: "<selected tool_id from bh tools list>"
returned_template_paths: []
stop_conditions: []
reasoning: "maximum_available"
```

For write tasks, execute any Main-Agent-assigned source-control status or checkout tool before `blueprinthelper_execute_task`. If source control reports `checked_out_by_other`, `source_control_conflicted`, `source_control_unavailable`, `checkout_failed`, or `not_editable`, stop and return the agent-facing message and recommended action instead of attempting the write.

Return only the sideAgent's compact YAML result to the Main Agent.

