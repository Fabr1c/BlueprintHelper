# BlueprintHelper TaskSpec UE Smoke Test (2026-05-05)

## Purpose

This smoke verifies the current TaskSpec-first runtime path against a real Unreal Editor session:

```text
Agent
-> MCP task tools
-> Python/MCP Task Compiler
-> BlueprintHelper.TaskPlan.v1 GraphWrite IR
-> Bridge preview_task_plan / execute_task_plan
-> UE Task Runtime lowering
-> existing GraphWrite capability cluster
```

The smoke must not require the Agent to choose low-level atomic MCP tools or author TaskPlan directly. The only Agent-authored write input is `BlueprintHelper.TaskSpec.v1`.

## Scope

This smoke covers the first implemented slice only:

- `task_type = edit_blueprint_graph`
- `behavior.graph_strategy = append_new_owned_graph`
- `behavior.entries[].entry_type = custom_event`
- `behavior.entries[].body.statements[].kind = call_function`
- `TaskPlan.steps[0].capability = graph_write`
- `TaskPlan.steps[0].write.strategy = owned_graph_edit`
- `TaskPlan.steps[0].write.ops[0].op = ensure_entry`
- UE Task Runtime lowers the GraphWrite IR to the existing `append_blueprint_graph` adapter command.

This smoke does not cover asset creation, replace/patch/merge graph strategies, function signatures, event signatures, UMG, DataAsset, or DataTable clusters.

## Preconditions

1. Unreal Editor is running for the target project.
2. BlueprintHelper Bridge is reachable.
3. MCP server is running and exposes these task-level tools:
   - `blueprinthelper_get_runtime_profile`
   - `blueprinthelper_diagnostics`
   - `blueprinthelper_read_task_context`
   - `blueprinthelper_preview_task`
   - `blueprinthelper_execute_task`
   - `blueprinthelper_get_task_result`
4. A disposable Actor Blueprint already exists. The first TaskSpec slice edits an existing Blueprint asset; it does not create one.

Recommended smoke asset:

```text
/Game/BlueprintHelper/Smoke/BP_TaskSpecSmoke
```

Use a unique graph and event name for every run:

```text
graph_name: BH_TaskSpecSmoke_20260505_001
event_name: BH_TaskSpecSmokeEvent_20260505_001
```

Do not run this smoke against production gameplay assets.

## Placeholder Values

Replace these before running:

| Placeholder | Value |
|---|---|
| `_ASSET_PATH_` | Existing disposable Blueprint asset path |
| `_GRAPH_NAME_` | New unique graph name |
| `_EVENT_NAME_` | New unique custom event name |
| `_TASK_RUN_ID_` | Returned by `blueprinthelper_execute_task` |

## Smoke TaskSpec

Use this exact TaskSpec shape for `blueprinthelper_preview_task` and `blueprinthelper_execute_task`:

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1",
    "context_id": "ctx_taskspec_ue_smoke_20260505_001",
    "task_type": "edit_blueprint_graph",
    "feature_name": "TaskSpecUESmoke",
    "target": {
      "asset_path": "_ASSET_PATH_",
      "target_type": "blueprint"
    },
    "scope_policy": {
      "graph_name": "_GRAPH_NAME_",
      "allow_modify_user_nodes": false
    },
    "behavior": {
      "graph_strategy": "append_new_owned_graph",
      "entries": [
        {
          "entry_type": "custom_event",
          "name": "_EVENT_NAME_",
          "body": {
            "schema": "BlueprintLogicSpec.v1",
            "statements": [
              {
                "kind": "call_function",
                "name": "PrintString",
                "args": {
                  "InString": {
                    "kind": "literal",
                    "value_type": "string",
                    "value": "TaskSpec UE smoke executed"
                  },
                  "Duration": {
                    "kind": "literal",
                    "value_type": "float",
                    "value": 2
                  }
                }
              }
            ]
          }
        }
      ]
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

Do not use `validation.compile` or `validation.save`. Those legacy keys are rejected.

## Step 1: Runtime Profile

Call `blueprinthelper_get_runtime_profile`.

Pass criteria:

- Tool result is successful.
- Result confirms the Bridge/runtime profile is available.
- If profile reports that writes require dry-run first, continue because this smoke uses preview before execute.

Stop if the Bridge is not reachable.

## Step 2: Read Task Context

Call `blueprinthelper_read_task_context`:

```json
{
  "target": {
    "asset_path": "_ASSET_PATH_"
  },
  "intent": "smoke_test_graph_append",
  "feature_name": "TaskSpecUESmoke"
}
```

Pass criteria:

- Tool result is successful.
- `data.schema = BlueprintHelper.TaskContextPack.v1`.
- `data.runtime.bridge_reachable = true`.
- `data.target.asset_path = _ASSET_PATH_`.
- `data.target.exists = true`.
- `data.recommended_constraints.graph_strategy = append_new_owned_graph`.
- `data.recommended_constraints.allow_modify_user_nodes = false`.

Stop if the target asset does not exist.

## Step 3: Preview Task

Call `blueprinthelper_preview_task` with the smoke TaskSpec.

Pass criteria:

- Tool result is successful.
- `operation = preview_task`.
- `status = dry_run`.
- `modified = false`.
- `data.schema = BlueprintHelper.TaskPreview.v1`.
- `data.passed = true`.
- `data.blocked = false`.
- `data.task_plan.schema = BlueprintHelper.TaskPlan.v1`.
- `data.task_plan.task_name = TaskSpecUESmoke`.
- `data.task_plan.target_assets[0] = _ASSET_PATH_`.
- `data.task_plan.steps[0].capability = graph_write`.
- `data.task_plan.steps[0].target.asset_path = _ASSET_PATH_`.
- `data.task_plan.steps[0].target.graph = _GRAPH_NAME_`.
- `data.task_plan.steps[0].strategy = owned_graph_edit`.
- `data.task_plan.steps[0].ops = 1`.

The summarized preview result intentionally does not expose the full TaskPlan body. The compiler-owned TaskPlan for this smoke must contain one GraphWrite op equivalent to:

```json
{
  "op": "ensure_entry",
  "entry_type": "custom_event",
  "name": "_EVENT_NAME_",
  "body": {
    "schema": "BlueprintLogicSpec.v1",
    "statements": [
      {
        "kind": "call_function",
        "name": "PrintString"
      }
    ]
  }
}
```

Failure criteria:

- `data.passed = false`.
- `data.blocked = true`.
- Error code indicates missing asset, unsupported graph strategy, unsupported entry type, unsupported statement kind, graph collision, or Bridge dry-run failure.

If preview fails, do not call `blueprinthelper_execute_task`.

## Step 4: Execute Task

Call `blueprinthelper_execute_task` with the same smoke TaskSpec only after preview passes.

Pass criteria:

- Tool result is successful.
- `operation = execute_task`.
- `data.schema = BlueprintHelper.TaskExecution.v1`.
- `data.task_run_id` is a non-empty string.
- `data.preview_id` is a non-empty string.
- `data.task.feature_name = TaskSpecUESmoke`.
- `data.task.target_assets[0] = _ASSET_PATH_`.
- `data.task.applied_steps = 1`.
- `data.task.modified_assets = 1`.
- `data.bridge_result.ok = true`.

Runtime-level pass criteria inside `data.bridge_result.data.steps[0]`, when present:

- `capability = graph_write`.
- `operation = graph_write`.
- `adapter_operation = append_blueprint_graph`.
- `result.operation = append_blueprint_graph`.

Expected editor state:

- `_GRAPH_NAME_` exists on the disposable Blueprint, or the runtime result reports the created graph.
- `_EVENT_NAME_` exists as the generated custom event entry.
- The Blueprint is dirty in memory.
- The asset is not saved by this smoke because `validation.should_save = false`.

If execute returns `task_preview_blocked`, the runtime correctly refused to write after dry-run failure and the smoke fails unless the blocked preview was intentionally induced.

## Step 5: Read Task Result

Call `blueprinthelper_get_task_result`:

```json
{
  "task_run_id": "_TASK_RUN_ID_"
}
```

Pass criteria:

- Tool result is successful.
- `operation = get_task_result`.
- `data.schema = BlueprintHelper.TaskRunJournal.v1`.
- `data.task_run_id = _TASK_RUN_ID_`.
- `data.status = completed`.
- `data.target_assets[0] = _ASSET_PATH_`.
- `data.steps[0].step_id = step_001`.

The MCP result store and UE runtime journal should preserve the runtime distinction:

```text
capability = graph_write
operation = graph_write
adapter_operation = append_blueprint_graph
```

This distinction is important: `append_blueprint_graph` is the runtime lowering adapter target, not the Agent-authored TaskSpec operation.

## Optional Post-Read Inspection

After execute, call `blueprinthelper_read_task_context` again for `_ASSET_PATH_`.

Pass if the compact graph summary includes `_GRAPH_NAME_`. If the compact context does not include enough graph detail, use an internal/debug graph read tool only for inspection. Do not turn that debug read into the default Agent write workflow.

## Cleanup

Because `should_save = false`, the smoke should leave an unsaved editor-side modification.

Recommended cleanup:

1. Inspect the generated graph and event.
2. Close or revert the dirty disposable Blueprint without saving.
3. If you intentionally want to keep the smoke asset, save it manually.
4. Do not save generated smoke graphs into shared gameplay assets.

## Pass Checklist

- [ ] Runtime profile or diagnostics confirms Bridge availability.
- [ ] `read_task_context` returns `BlueprintHelper.TaskContextPack.v1`.
- [ ] Preview returns `status = dry_run`, `passed = true`, `blocked = false`.
- [ ] Preview TaskPlan summary reports `capability = graph_write`, `strategy = owned_graph_edit`, `ops = 1`.
- [ ] Execute returns `BlueprintHelper.TaskExecution.v1` with a non-empty `task_run_id`.
- [ ] Runtime child result shows lowering to `append_blueprint_graph` when that detail is available.
- [ ] `get_task_result` returns `BlueprintHelper.TaskRunJournal.v1`.
- [ ] `_GRAPH_NAME_` and `_EVENT_NAME_` are visible on the disposable Blueprint.
- [ ] The asset remains unsaved unless explicitly kept.

## Known Limits

- The first slice supports one TaskPlan step.
- The first slice supports `ensure_entry(custom_event)` only for runtime lowering.
- `call_function` and `set_member_variable` are the supported statement kinds, but this smoke uses only `call_function` to avoid depending on a pre-existing Blueprint variable.
- The compiler-owned GraphWrite TaskPlan step must not carry `operation = append_blueprint_graph`; that value is reserved for runtime adapter details such as `adapter_operation` and child results.
- Function/event signature management is a future UE capability cluster.
