---
name: blueprint-helper-task-worker
description: Fork the task-worker sideAgent to execute a BlueprintHelper.TaskWorkerPackage.v1 package through CLI preview/execute/readback.
context: fork
agent: task-worker
---

# BlueprintHelper Task Worker Fork

Invoke the `task-worker` sideAgent only after MainAgent has confirmed target asset, scope, operation intent, modification boundary, evidence sufficiency, source-control gate, and write-session gate.

The MainAgent owns user clarification, lifecycle MCP, safety/write boundary decisions, source-control gate, write-session gate, closed-loop user decisions, and final response. TaskWorker owns CLI catalog/composer discovery, TaskSpec/ReadSpec construction, preview, execute, readback, retry accounting, and capability-boundary output.

Input must be compact YAML:

```yaml
schema: BlueprintHelper.TaskWorkerPackage.v1
task_package_id: "<stable id>"
user_goal: "<editor/gameplay intent>"
operation_mode: "create_new | modify_existing | inspect_only | validate_only"
target:
  asset_path: "<explicit Unreal asset path>"
  graph_or_scope: "<graph/function/event/widget/table/object scope>"
capability_scope:
  family_scope:
    - "normal_blueprint | material_blueprint | animation_blueprint | umg_widget | data_table | data_asset | object"
  allowed_operation_intent: "<natural language operation boundary>"
modification_scope:
  assets:
    - "<asset path>"
  user_owned_nodes_policy: "preserve | explicit_external_mutation_allowed"
  expected_changed_surface: "<short description>"
evidence:
  blueprint_explorer_summary: "<compact UE asset evidence>"
prewrite_gates:
  source_control:
    status: "passed | not_required"
    checked_assets:
      - "<asset path>"
  write_session:
    status: "approved | not_required"
    approved_scope:
      - "<asset path>"
retry_budget:
  max_attempts: "<optional override; default comes from agent.task_worker.max_attempts>"
readback_required: true
stop_conditions:
  - "preview_blocked"
  - "execute_failed"
  - "readback_mismatch"
  - "evidence_conflict"
  - "retry_budget_exceeded"
  - "prewrite_gate_stale_or_insufficient"
return_format: "compact Chinese YAML with status, attempts, evidence, blockers, and next step"
```

TaskWorker must discover concrete family, write mode, cluster, operation, quick-access template, TaskSpec, and readback template through BlueprintHelper CLI catalog/composer commands inside `family_scope` and `allowed_operation_intent`. Use the current CLI discovery surface, including `bh tools templates families` and `bh tools templates compose`, when building TaskSpec files. Run grouped task commands with `bh task preview --file <task-spec.json>` and `bh task execute --file <task-spec.json>`. MainAgent does not supply exact catalog selections.

Use the write index in order before reporting `capability_missing`:

```powershell
bh tools templates families --workflow preview_execute --format json
bh tools templates write-modes --family <family> --format json
bh tools templates clusters --family <family> --format json
bh tools templates operations --family <family> --cluster <cluster> --write-mode <mode> --format json
bh tools templates quick-access --family <family> --cluster <cluster> --operation <operation> --write-mode <mode> --format json
```

Use the read index for required readback before reporting readback capability missing:

```powershell
bh tools read-templates families --format json
bh tools read-templates clusters --family <family> --format json
bh tools read-templates list --family <family> --cluster <cluster> --format json
```

Treat `output.format` in the read-template index as the returned evidence shape, not as `view.format`. `view.format` must come from the leaf ReadSpec/template fields and current CLI schema. Classify blockers precisely: `bridge_unavailable` means the Bridge request path is unavailable, `route_missing` means no active indexed route/template exists, wrong input or invalid enum means the payload is malformed, and `graph_body_target_unresolved` means the graph/function/event target could not be resolved from readback evidence.

For graph body replacement, `function_body` is a body kind, not ownership proof. Use BlueprintHelper-owned `function_body`/`replace_owned_graph` only when template/readback evidence proves an owned route; user-authored event/function bodies must use `external_body` with `adapter_boundary.body_entry` and `body_fingerprint`.

TaskWorker retry budget comes from `agent.task_worker.max_attempts` in BlueprintHelper setting unless the package explicitly includes `retry_budget.max_attempts` for a narrower task-specific override.

TaskWorker must not request write session or run source-control checkout/status gates. If execute fails with `write_session_missing`, `checkout_required`, `not_editable`, or equivalent stale gate errors, stop with `prewrite_gate_stale_or_insufficient`.

Success requires preview passed, execute completed, readback completed, readback matches the user intent, no evidence conflict, retry budget not exceeded, and approved scope respected. Return only the sideAgent's compact YAML result to MainAgent.
