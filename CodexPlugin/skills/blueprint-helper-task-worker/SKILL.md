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
template_discovery:
  family: "graph_write"
  write_mode: "graph.replace"
  cluster: "external_body"
  operation: "replace_body"
  quick_access: "body"
task_file_shape: "bare_taskspec_for_grouped_task_commands"
allowed_cli:
  - "bh tools templates families --workflow preview_execute --format json"
  - "bh tools templates write-modes --family graph_write --format json"
  - "bh tools templates clusters --family graph_write --format json"
  - "bh tools templates operations --family graph_write --cluster external_body --write-mode graph.replace --format json"
  - "bh tools templates quick-access --family graph_write --cluster external_body --operation replace_body --write-mode graph.replace --format json"
  - "bh tools templates compose --family graph_write --write-mode graph.replace --templates external_body.replace_body.body --out <task-spec.json> --format json"
  - "bh task preview --file <task-spec.json> --format json"
  - "bh task execute --file <task-spec.json> --format json"
stop_conditions: []
reasoning: "maximum_available"
```

The exact template-discovery values vary by task. They must come from the Main Agent package or from `bh tools templates families` / `write-modes` / `clusters` / `operations` / `quick-access`; do not scan template directories as the primary path.

For write tasks, execute any Main-Agent-assigned source-control status or checkout command after preview and before `bh task execute --file`. If source control reports `checked_out_by_other`, `source_control_conflicted`, `source_control_unavailable`, `checkout_failed`, or `not_editable`, stop and return the agent-facing message and recommended action instead of attempting the write.

Return only the sideAgent's compact YAML result to the Main Agent.

