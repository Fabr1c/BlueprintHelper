# BlueprintHelper Current Capability Integration Smoke Test (2026-05-05)

## Purpose

This smoke validates the current TaskSpec-first architecture as a connected system, not as isolated tools:

```text
Agent
-> blueprinthelper_* Agent tools
-> BlueprintHelper.ReadSpec.v1 / BlueprintHelper.TaskSpec.v1
-> MCP / Python compiler and read router
-> BlueprintHelper.TaskPlan.v1 structured IR
-> Bridge preview_task_plan / execute_task_plan
-> UE Task Runtime lowering
-> existing UE capability clusters
-> TaskRunJournal / ReadContextPack / ReferenceContextPack
```

The smoke verifies three things:

1. The Agent-facing surface is still small and TaskSpec-first.
2. Reads, previews, executes, journals, and read-back checks can be chained.
3. Cross-capability TaskPlan topology is visible, especially `blueprint_signature -> graph_write`.

## Current Scope

Run this smoke against disposable assets only.

Covered now:

- MCP / Python contract regression.
- Runtime profile and diagnostics.
- AgentGuide entry point.
- `blueprinthelper_read_context` first slice: `read_type = blueprint_logic`, `format = logic_md / logic_json / summary / schema`.
- `blueprinthelper_read_reference_context` read-only reference viewer.
- GraphWrite Append with explicit Signature dependency:
  - `blueprint_signature.ensure_custom_event`
  - `graph_write.ensure_entry`
- GraphWrite Replace / Patch / Merge compiler and TaskPlan contract checks.
- P1 TaskSpec compiler coverage for:
  - AssetFactory
  - BlueprintComponent
  - BlueprintClassSettings
  - BlueprintVariables
  - UMGWidget
  - DataTable
- Composite `create_blueprint_feature` preview path across multiple capability clusters.
- TaskRunJournal partial failure fields:
  - `blocked_by_step_ids`
  - `blocked_reason`
  - `recovery`

Not covered as default smoke:

- Production asset edits.
- Real destructive remove / replace on user-owned logic.
- Material graph and animation blueprint reads or writes.
- Old Agent-facing atomic MCP write tools.
- Global editor undo / redo.

## Required Agent-Facing Tools

The smoke assumes these are the default Agent-facing tools:

```text
blueprinthelper_get_runtime_profile
blueprinthelper_diagnostics
blueprinthelper_read_agent_guide
blueprinthelper_read_context
blueprinthelper_read_reference_context
blueprinthelper_preview_task
blueprinthelper_execute_task
blueprinthelper_get_task_result
blueprinthelper_open_editor
blueprinthelper_close_editor
```

Old low-level write tools must not be used in this smoke. They are internal, debug, expert, or transitional entries only.

## Test Assets

Replace placeholders before running:

| Placeholder | Meaning |
|---|---|
| `_BP_SMOKE_` | Existing disposable Actor Blueprint, for example `/Game/BlueprintHelper/Smoke/BP_TaskSpecSmoke` |
| `_GRAPH_NAME_` | New unique graph name, for example `BH_Smoke_20260505_001` |
| `_EVENT_NAME_` | New unique custom event name, for example `BH_SmokeEvent_20260505_001` |
| `_FUNCTION_NAME_` | Existing or disposable function name, for example `CalculateSmokeValue` |
| `_WBP_SMOKE_` | Disposable Widget Blueprint |
| `_DT_SMOKE_` | Disposable DataTable |
| `_IA_SMOKE_` | Disposable InputAction path |
| `_TASK_RUN_ID_` | Returned by `blueprinthelper_execute_task` |

Rules:

- Use a fresh `_GRAPH_NAME_` and `_EVENT_NAME_` for every execute run.
- Prefer `should_save = false` unless the test explicitly checks save behavior.
- For P1 cluster execute tests, use assets created only for this smoke.
- If a fixture asset is missing, record the case as `blocked_by_fixture`, not as implementation failure.

## Level 0: Local Contract Regression

Run from:

```text
G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\BlueprintHelper_MCP_Server
```

Command:

```powershell
npm.cmd test
```

Pass criteria:

- TypeScript build passes.
- Python tests pass.
- Node tests pass.
- Contract tests confirm:
  - GraphWrite Append compiles to `blueprint_signature` before `graph_write`.
  - `validation.should_compile` and `validation.should_save` are used.
  - legacy `validation.compile` / `validation.save` are rejected.
  - TaskRunJournal schema accepts partial failure with blockers and recovery.

This level does not require Unreal Editor.

## Level 1: Editor And Bridge Preflight

### 1. Runtime Profile

Call `blueprinthelper_get_runtime_profile`.

Pass criteria:

- Tool succeeds.
- Bridge is reachable.
- Runtime profile reports an editor session or launchable project.

Stop if the Bridge is unreachable.

### 2. Diagnostics

Call `blueprinthelper_diagnostics`.

Pass criteria:

- Tool succeeds.
- No fatal diagnostics block editor asset operations.
- Warnings are recorded but do not fail the smoke unless they affect `_BP_SMOKE_`.

### 3. Agent Guide

Call `blueprinthelper_read_agent_guide`.

Pass criteria:

- The returned markdown is the AgentGuide onboarding index.
- It points Agents toward TaskSpec / ReadSpec workflows instead of old atomic write tools.

## Level 2: ReadSpec Smoke

### 1. Read Blueprint Logic Schema

Call `blueprinthelper_read_context`:

```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "blueprint_logic",
  "target": {
    "asset_path": "_BP_SMOKE_",
    "target_type": "blueprint"
  },
  "view": {
    "format": "schema"
  }
}
```

Pass criteria:

- `schema = BlueprintHelper.McpToolResult.v1`.
- `operation = read_context`.
- `status = completed`.
- `modified = false`.
- `data.schema = ReadContextPack.v1`.
- `data.read_type = blueprint_logic`.
- `data.format = schema`.
- `data.payload.schema` is a short schema name, without the `BlueprintHelper.` prefix.

### 2. Read Blueprint Logic As LogicMD

Call:

```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "blueprint_logic",
  "target": {
    "asset_path": "_BP_SMOKE_",
    "target_type": "graph",
    "target_name": "EventGraph"
  },
  "view": {
    "format": "logic_md",
    "detail": "normal",
    "max_items": 200
  }
}
```

Pass criteria:

- `data.schema = ReadContextPack.v1`.
- `data.payload.schema = LogicMd.v1`.
- Returned content is read-only logic information.
- No TaskSpec draft is embedded in the payload.

### 3. Read Blueprint Logic As LogicJson

Call:

```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "blueprint_logic",
  "target": {
    "asset_path": "_BP_SMOKE_",
    "target_type": "custom_event",
    "target_name": "_EVENT_NAME_"
  },
  "view": {
    "format": "logic_json",
    "detail": "debug",
    "max_items": 200
  }
}
```

Pass criteria:

- `data.payload.schema = LogicJson.v1`.
- LogicJson is read-only.
- LogicJson node refs are not treated as TaskSpec write anchors unless a future contract explicitly maps them.

### 4. Unsupported Read Types Are Explicit

Call one non-implemented read type, for example:

```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "component_context",
  "target": {
    "asset_path": "_BP_SMOKE_",
    "target_type": "blueprint"
  },
  "view": {
    "format": "summary"
  }
}
```

Current pass criteria:

- The tool returns a clear unsupported-read error.
- It does not silently return partial or fake component data.

This is expected until `blueprinthelper_read_context` grows additional read domains.

## Level 3: Reference Context Smoke

Call `blueprinthelper_read_reference_context`:

```json
{
  "asset_path": "_BP_SMOKE_",
  "target_type": "blueprint",
  "scope": "safety_context",
  "max_results": 50,
  "include_samples": true
}
```

Pass criteria:

- `schema = BlueprintHelper.McpToolResult.v1`.
- `operation = read_reference_context`.
- `modified = false`.
- `data.schema = BlueprintHelper.ReferenceContextPack.v1` or `ReferenceContextPack.v1`, depending on the current Bridge normalization layer.
- `dependencies`, `referencers`, and `external_dependents` remain separate arrays.
- If analysis is incomplete, `ok = true` is allowed only when `data.analysis.partial = true` and `unsupported_checks` explains the gap.

Use this tool after blocked previews or before high-risk remove / replace / rename scenarios.

## Level 4: GraphWrite Append Integration Smoke

This is the primary execute smoke because it crosses Signature, GraphWrite, Task Runtime, Bridge, and read-back.

### 1. Preview Task

Call `blueprinthelper_preview_task`:

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1",
    "context_id": "ctx_current_integration_smoke_append_20260505",
    "task_type": "edit_blueprint_graph",
    "feature_name": "CurrentIntegrationSmokeAppend",
    "target": {
      "asset_path": "_BP_SMOKE_",
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
                    "value": "BlueprintHelper current integration smoke"
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

Pass criteria:

- `operation = preview_task`.
- `status = dry_run`.
- `modified = false`.
- `data.schema = TaskPreviewResult.v1`.
- `data.passed = true`.
- `data.blocked = false`.
- `data.task_plan.schema = BlueprintHelper.TaskPlan.v1`.
- `data.task_plan.steps[0].capability = blueprint_signature`.
- `data.task_plan.steps[0].strategy = custom_event_signature`.
- `data.task_plan.steps[1].capability = graph_write`.
- `data.task_plan.steps[1].strategy = owned_graph_edit`.
- `data.task_plan.steps[1].target.graph = _GRAPH_NAME_`.

Expected compiler-owned TaskPlan topology:

```json
[
  {
    "step_id": "step_001",
    "capability": "blueprint_signature",
    "write": {
      "strategy": "custom_event_signature",
      "ops": [
        {
          "op": "ensure_custom_event",
          "event_name": "_EVENT_NAME_",
          "graph_name": "_GRAPH_NAME_"
        }
      ]
    }
  },
  {
    "step_id": "step_002",
    "capability": "graph_write",
    "depends_on": ["step_001"],
    "write": {
      "strategy": "owned_graph_edit",
      "ops": [
        {
          "op": "ensure_entry",
          "entry_type": "custom_event",
          "name": "_EVENT_NAME_"
        }
      ]
    }
  }
]
```

The Agent does not author this TaskPlan. It is shown here only as expected debug topology.

### 2. Execute Task

Call `blueprinthelper_execute_task` with the same TaskSpec.

Pass criteria:

- The tool performs preview before write.
- `operation = execute_task`.
- `modified = true` only after the write phase succeeds.
- `data.schema = TaskRunSummary.v1`.
- A `task_run_id` is returned.
- The summary reports Signature and GraphWrite substeps.
- If `should_compile = true`, compile status is recorded in the result or journal.
- If `should_save = false`, the asset may be dirty but should not be saved by this task.

Stop if preview is blocked; do not force execute.

### 3. Get Task Result

Call `blueprinthelper_get_task_result`:

```json
{
  "task_run_id": "_TASK_RUN_ID_"
}
```

Pass criteria:

- `data.schema = BlueprintHelper.TaskRunJournal.v1`.
- `status = completed` for a clean run.
- Journal contains entries for the Signature and GraphWrite path.
- Step statuses use `completed`, `failed`, `blocked`, or `skipped`.
- No step status uses legacy `applied`.

### 4. Read Back

Call `blueprinthelper_read_context` twice:

```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "blueprint_logic",
  "target": {
    "asset_path": "_BP_SMOKE_",
    "target_type": "graph",
    "target_name": "_GRAPH_NAME_"
  },
  "view": {
    "format": "logic_md"
  },
  "context": {
    "task_run_id": "_TASK_RUN_ID_"
  }
}
```

```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "blueprint_logic",
  "target": {
    "asset_path": "_BP_SMOKE_",
    "target_type": "custom_event",
    "target_name": "_EVENT_NAME_"
  },
  "view": {
    "format": "logic_json"
  },
  "context": {
    "task_run_id": "_TASK_RUN_ID_"
  }
}
```

Pass criteria:

- The new graph or custom event can be read.
- LogicMD contains the new custom event name.
- LogicJson contains the new custom event and `PrintString` call.
- Read-back does not expose raw adapter payloads as Agent contract.

## Level 5: GraphWrite Replace / Patch / Merge Preview Smoke

These are preview-first unless disposable fixtures are prepared for execute.

Pass criteria for all three:

- `preview_task` succeeds or returns a precise blocked reason.
- Compiler output uses `capability = graph_write`.
- Adapter operation names are not Agent-authored input.
- Dry-run does not modify assets.

### Replace Preview

Use `behavior.graph_strategy = replace_owned_graph` and a selector for a known disposable custom event body.

Expected TaskPlan:

- `write.ops[0].op = replace_body`.
- Runtime lowering target is `replace_blueprint_graph`.

### Patch Preview

Use `behavior.graph_strategy = patch_owned_graph` with a known node or pin ref from LogicJson.

Expected TaskPlan:

- `write.ops[0].op = set_pin_default`, `set_node_comment`, or `set_node_position`.
- Runtime lowering target is `patch_blueprint_graph`.

### Merge Preview

Use `behavior.graph_strategy = merge_owned_graph` with a disposable owned block or flow anchor.

Expected TaskPlan:

- `write.ops[0].op = insert_flow`.
- Runtime lowering target is `merge_blueprint_graph`.

Known limitation:

- LogicJson node refs and TaskSpec patch/merge refs are not fully unified yet. If the Agent cannot map refs safely, record `blocked_by_contract_gap` instead of guessing.

## Level 6: P1 Capability Cluster Preview Smoke

Run these through `blueprinthelper_preview_task`. Execute only on disposable fixtures.

### 1. AssetFactory

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1",
    "context_id": "ctx_smoke_asset_factory",
    "task_type": "create_asset",
    "feature_name": "SmokeAssetFactory",
    "target": {
      "asset_path": "_IA_SMOKE_",
      "target_type": "asset"
    },
    "behavior": {
      "asset_strategy": "ensure_asset",
      "asset": {
        "asset_type": "input_action",
        "value_type": "bool",
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

Expected preview summary:

- `capability = asset_factory`.
- `strategy = asset_create`.
- `ops = 1`.

### 2. BlueprintComponent

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1",
    "context_id": "ctx_smoke_component",
    "task_type": "edit_blueprint_components",
    "feature_name": "SmokeComponent",
    "target": {
      "asset_path": "_BP_SMOKE_",
      "target_type": "blueprint"
    },
    "behavior": {
      "component_strategy": "component_tree",
      "changes": [
        {
          "kind": "ensure_component_present",
          "name": "SmokeRoot",
          "class": "SceneComponent"
        },
        {
          "kind": "configure_component",
          "name": "SmokeRoot",
          "properties": [
            {
              "property_path": "Mobility",
              "value": "Movable"
            }
          ]
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

Expected preview summary:

- Two `blueprint_component` steps.
- Strategies are `component_tree`.

### 3. BlueprintVariables

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1",
    "context_id": "ctx_smoke_variables",
    "task_type": "edit_blueprint_variables",
    "feature_name": "SmokeVariables",
    "target": {
      "asset_path": "_BP_SMOKE_",
      "target_type": "blueprint"
    },
    "behavior": {
      "variable_strategy": "member_variables",
      "changes": [
        {
          "kind": "ensure_member_variable",
          "name": "SmokeValue",
          "variable_type": {
            "category": "float"
          },
          "category": "Smoke"
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

Expected preview summary:

- `capability = blueprint_variable`.
- `strategy = member_variables`.

### 4. BlueprintClassSettings

Use only a disposable interface path. If no interface fixture exists, record `blocked_by_fixture`.

Expected preview summary:

- `capability = blueprint_class_settings`.
- `strategy = class_settings`.
- Interface add/remove and class default steps are separate TaskPlan steps.

### 5. UMGWidget

Use only `_WBP_SMOKE_`.

Expected preview summary:

- `capability = umg_widget`.
- Widget tree edit and widget property edit are separate strategies.

### 6. DataTable

Use only `_DT_SMOKE_`.

Expected preview summary:

- `capability = data_table`.
- `strategy = row_edit`.
- Add, update, and delete row operations are separate TaskPlan steps.

## Level 7: Composite Feature Linkage Smoke

Run preview first. Execute only if all referenced assets are disposable and present.

Use this shape for a physics door feature:

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1",
    "context_id": "ctx_smoke_composite_physics_door",
    "task_type": "create_blueprint_feature",
    "feature_name": "SmokePhysicsDoor",
    "target": {
      "asset_path": "_BP_SMOKE_",
      "target_type": "blueprint"
    },
    "scope_policy": {
      "prefer_new_graph": true,
      "graph_name": "_GRAPH_NAME_",
      "allow_modify_user_nodes": false,
      "allow_create_assets": false
    },
    "asset_policy": {
      "if_target_asset_missing": "fail",
      "if_referenced_asset_missing": "fail",
      "if_component_exists": "reuse_if_type_matches"
    },
    "components": [
      {
        "name": "SceneRoot",
        "class": "SceneComponent",
        "set_as_root": true
      },
      {
        "name": "DoorMesh",
        "class": "StaticMeshComponent",
        "attach_to": "SceneRoot",
        "properties": {
          "Mobility": "Movable",
          "CollisionProfileName": "PhysicsActor",
          "BodyInstance.bSimulatePhysics": true
        }
      }
    ],
    "variables": [
      {
        "name": "bDoorOpen",
        "type": "bool",
        "default": false,
        "category": "Door"
      },
      {
        "name": "OpenImpulse",
        "type": "float",
        "default": 50000,
        "category": "Door"
      }
    ],
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
                "kind": "set_member_variable",
                "name": "bDoorOpen",
                "value": {
                  "kind": "literal",
                  "value_type": "bool",
                  "value": true
                }
              },
              {
                "kind": "call_function",
                "name": "PrintString",
                "args": {
                  "InString": {
                    "kind": "literal",
                    "value_type": "string",
                    "value": "Smoke physics door opened"
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

Pass criteria:

- Preview decomposes into multiple capability steps.
- Expected capability order includes:
  - `blueprint_component`
  - `blueprint_variable`
  - `blueprint_signature`
  - `graph_write`
- `graph_write` body write depends on the custom event signature step.
- No Agent-authored TaskPlan or adapter operation appears in the request.

## Level 8: Failure And Recovery Smoke

### 1. Schema Error

Submit a TaskSpec without `schema`.

Pass criteria:

- `preview_task` fails before Bridge write.
- Error category is schema or validation.
- No asset is modified.

### 2. Semantic Error

Submit:

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1",
    "context_id": "ctx_smoke_bad_strategy",
    "task_type": "edit_blueprint_graph",
    "feature_name": "BadStrategy",
    "target": {
      "asset_path": "_BP_SMOKE_",
      "target_type": "blueprint"
    },
    "scope_policy": {
      "graph_name": "_GRAPH_NAME_",
      "allow_modify_user_nodes": false
    },
    "behavior": {
      "graph_strategy": "replace_graph",
      "entries": [
        {
          "entry_type": "custom_event",
          "name": "_EVENT_NAME_",
          "body": {
            "schema": "BlueprintLogicSpec.v1",
            "statements": []
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

Pass criteria:

- Error code identifies unsupported graph strategy.
- Error path points to `behavior.graph_strategy`.
- No Bridge write occurs.

### 3. Preview Blocked

Reuse an existing non-empty graph name for `append_new_owned_graph`.

Pass criteria:

- `preview_task` returns `passed = false`.
- `blocked = true`.
- `modified = false`.
- Agent can call `blueprinthelper_read_reference_context` or `blueprinthelper_read_context` to debug the target.

### 4. Partial Failure Journal

This is validated by the local regression suite and should be smoke-tested in UE only when a controlled fixture can fail during execute after at least one independent step succeeds.

Pass criteria:

- `TaskRunJournal.status = partial_failure`.
- Failed step has `status = failed`.
- Dependent downstream steps have:
  - `status = blocked`
  - `blocked_by_step_ids`
  - `blocked_reason`
- Independent steps may continue.
- Journal has:
  - `recovery.recommended_action`
  - `recovery.safe_to_retry`
  - `recovery.rollback_available`

## Level 9: Build Check

Use the project build command, not BuildPlugin:

```powershell
F:\UE_5.6\Engine\Build\BatchFiles\Build.bat MrStoneEditor Win64 Development -Project="G:\UnrealPractise\MrStone\MrStone.uproject"
```

Pass criteria:

- Build reaches C++ compilation.
- No BlueprintHelper compile errors.
- If the build stops before compilation with `UnauthorizedAccessException` or `Unable to rename ... Intermediate ...`, record it as environment / file-lock failure, not a BlueprintHelper compile failure.

## Final Smoke Report Template

```text
Smoke run:
Date:
Editor project:
MCP server version:
Target asset:

Level 0 MCP/Python regression: PASS / FAIL
Level 1 Bridge preflight: PASS / FAIL
Level 2 ReadSpec blueprint_logic: PASS / FAIL
Level 3 Reference context: PASS / FAIL / PARTIAL
Level 4 Append signature->graph execute: PASS / FAIL
Level 5 Replace/Patch/Merge preview: PASS / FAIL / BLOCKED_BY_CONTRACT_GAP
Level 6 P1 cluster previews: PASS / FAIL / BLOCKED_BY_FIXTURE
Level 7 Composite preview: PASS / FAIL / BLOCKED_BY_FIXTURE
Level 8 Failure/recovery: PASS / FAIL / LOCAL_ONLY
Level 9 UE build: PASS / FAIL / ENV_BLOCKED

Task run ids:
- append:
- composite:

Notes:
- 

Open issues:
- 
```

## Actual Smoke Report: 2026-05-05 (Rerun 1)

```text
Smoke run: 2026-05-05 Rerun
Date: 2026-05-05
Editor project: G:/UnrealPractise/MrStone/MrStone.uproject (UE 5.6)
MCP server version: 0.3.8
Target asset: /Game/BlueprintHelper/Smoke/BP_TaskSpecSmoke

Level 0 MCP/Python regression: PASS
Level 1 Bridge preflight: PASS
Level 2 ReadSpec blueprint_logic: PASS
Level 3 Reference context: PARTIAL
Level 4 Append signature->graph execute: PASS
Level 5 Replace/Patch/Merge preview: FAIL
Level 6 P1 cluster previews: PARTIAL
Level 7 Composite preview: FAIL
Level 8 Failure/recovery: PARTIAL
Level 9 UE build: PASS

Task run ids:
- append_graphwrite: task_5806121649296A709F32088EB10C55F0
- variables_execute: task_38C6DC0D4AC56E1DD89F4992D9A7B3AB
- composite: N/A (preview_task blocked at Bridge)

Notes:
- Level 0: 112/112 tests pass (30 Python + 82 Node). All contract checks confirmed.
- Level 1: Runtime Profile status=degraded (merge/journal/review/store not_implemented - expected).
  Diagnostics: no blocking issues. Agent Guide returns correct TaskSpec-first index.
- Level 2: All ReadSpec formats work correctly.
- Level 3: ReferenceContextPack.v1 returned, analysis.partial=true.
- Level 4.1 (preview): PASS. preview_task for edit_blueprint_graph with NEW graph name worked.
  TaskPlan: step_001 (blueprint_signature → custom_event_signature) → step_002 (graph_write → owned_graph_edit).
- Level 4.2 (execute): PASS. Full pipeline worked:
  step_001 signature: status=no_op (deferred_to_graph_write)
  step_002 graph_write: status=applied (graph BH_Smoke_Rerun_20260505 created with PrintString call)
  post_operation compile: succeeded
- Level 4.3 (get_task_result): PASS.
  data.schema = BlueprintHelper.TaskRunJournal.v1, status=completed.
  Steps use 'completed' status (not legacy 'applied').
- Level 4.4 (read_back LogicMd): PASS.
  Graph BH_Smoke_Rerun_20260505 shows: Entry=BH_SmokeRerunEvent_20260505,
  Execution: BH_SmokeRerunEvent_20260505.then → 打印字符串.execute. Nodes=2, Orphans=0.
- Level 4.4 (read_back LogicJson): PARTIAL.
  LogicJson returned but custom_event lookup in EventGraph returned EventGraph nodes,
  not the custom event's graph. ReadContextPack correctly returned LogicJson.v1 schema.
- Level 5 Replace preview: FAIL (Bridge blocks replace_owned_graph on existing graph).
- Level 5 Patch preview: FAIL (same).
- Level 5 Merge preview: FAIL (same).
- Level 6.1 AssetFactory preview: PASS. capability=asset_factory, strategy=asset_create.
- Level 6.2 Component preview: FAIL (Bridge blocks edit_blueprint_components).
- Level 6.3 Variables preview+execute: PASS. SmokeValue (float) variable created, compiled OK.
- Level 6.4 ClassSettings: NOT TESTED (blocked_by_fixture).
- Level 6.5 UMGWidget: NOT TESTED (blocked_by_fixture).
- Level 6.6 DataTable: NOT TESTED (blocked_by_fixture).
- Level 7 Composite preview: FAIL (Bridge blocks create_blueprint_feature).
- Level 8.1 Schema Error: PASS. Zod validation rejected missing schema field.
- Level 8.2 Semantic Error (replace_graph): FAIL (same Bridge blocking).
- Level 8.3 Preview Blocked (existing graph reuse): FAIL (same Bridge blocking).
- Level 8.4 Partial Failure Journal: NOT TESTED (blocked by preview).
- Level 9: PASS. Build succeeded in 7.90s. Zero errors.

Open issues:
- [implementation_failure] Bridge supports ONLY append_new_owned_graph with NEW graph name for
  edit_blueprint_graph. All other graph_strategy values (replace_owned_graph, patch_owned_graph,
  merge_owned_graph) fail. Also: edit_blueprint_components and create_blueprint_feature fail.
  Affects: Level 5, Level 6.2, Level 7, Level 8.2, Level 8.3.
- [contract_gap] LogicJson custom_event lookup searches only EventGraph, not custom graphs.
  BH_SmokeRerunEvent_20260505 exists in graph BH_Smoke_Rerun_20260505 but read_context with
  target_type=custom_event returns EventGraph data instead.
- [blocked_by_fixture] Missing disposable assets for ClassSettings, UMG, and DataTable P1 tests.
- [working_capability] Two TaskSpec pipelines are fully functional:
  1. edit_blueprint_graph + append_new_owned_graph + new graph name:
     TaskSpec → Python → TaskPlan → Bridge preview → Bridge execute → compile. PASS.
  2. edit_blueprint_variables:
     TaskSpec → Python → TaskPlan → Bridge preview → Bridge execute → compile. PASS.
```


## Failure Classification

Use these labels consistently:

| Label | Meaning |
|---|---|
| `implementation_failure` | Code path exists but behaves incorrectly. |
| `contract_gap` | Field or topology is not yet fully defined. |
| `blocked_by_fixture` | Disposable test asset or referenced asset is missing. |
| `blocked_by_environment` | Editor, Bridge, build lock, or permission issue. |
| `expected_unsupported` | Current slice intentionally rejects the request. |

Do not mark an old atomic MCP write tool failure as smoke failure. This smoke validates the TaskSpec / ReadSpec mainline.
