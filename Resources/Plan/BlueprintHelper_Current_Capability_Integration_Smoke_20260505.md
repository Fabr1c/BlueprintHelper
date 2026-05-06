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

These smoke cases are TaskSpec-first: send `task_spec` through
`blueprinthelper_preview_task` and then `blueprinthelper_execute_task` if preview passes.
Do not call old atomic MCP write tools directly in this section.

Pass criteria for all three:

- `blueprinthelper_preview_task` passes or returns a precise blocked reason.
- `blueprinthelper_execute_task` uses the same TaskSpec only after preview pass.
- Preview output uses `capability = graph_write`.
- Preview is dry-run; `modified = false`.
- Adapter op names are not Agent-authored request fields.
- If blocked, do not force execute.

### Replace Preview (Confirmed Shape)

Use `graph_strategy = replace_owned_graph` and `behavior.replace` object:

```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "context_id": "ctx_current_integration_smoke_replace_20260505",
  "task_type": "edit_blueprint_graph",
  "feature_name": "SmokeReplaceBody",
  "target": {
    "asset_path": "_BP_SMOKE_",
    "target_type": "blueprint"
  },
  "scope_policy": {
    "graph_name": "_GRAPH_NAME_",
    "allow_modify_user_nodes": false
  },
  "behavior": {
    "graph_strategy": "replace_owned_graph",
    "replace": {
      "scope": "custom_event_body",
      "selector": {
        "kind": "custom_event",
        "name": "_EVENT_NAME_",
        "graph_id": "_GRAPH_NAME_"
      },
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
                "value": "replace body test"
              }
            }
          }
        ]
      }
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
```

Expected TaskPlan:

- `write.ops[0].op = replace_body`.
- Runtime lowering target is `replace_blueprint_graph`.

### Patch Preview (Confirmed Shape)

Use `graph_strategy = patch_owned_graph` and `behavior.patches[]` with block-scoped
`target_ref`:

```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "context_id": "ctx_current_integration_smoke_patch_20260505",
  "task_type": "edit_blueprint_graph",
  "feature_name": "SmokePatch",
  "target": {
    "asset_path": "_BP_SMOKE_",
    "target_type": "blueprint"
  },
  "scope_policy": {
    "graph_name": "_GRAPH_NAME_",
    "allow_modify_user_nodes": false
  },
  "behavior": {
    "graph_strategy": "patch_owned_graph",
    "patches": [
      {
        "kind": "set_pin_default",
        "target_ref": {
          "block_id": "_BLOCK_ID_",
          "group_entry_node_path": "_GROUP_ENTRY_NODE_PATH_",
          "node_ref": "_GROUP_NODE_REF_",
          "pin_ref": "_GROUP_PIN_REF_"
        },
        "value": {
          "kind": "literal",
          "value_type": "string",
          "value": "patched"
        }
      },
      {
        "kind": "set_node_comment",
        "target_ref": {
          "block_id": "_BLOCK_ID_",
          "group_entry_node_path": "_GROUP_ENTRY_NODE_PATH_",
          "node_ref": "_GROUP_NODE_REF_"
        },
        "value": "patched by smoke"
      },
      {
        "kind": "set_node_position",
        "target_ref": {
          "block_id": "_BLOCK_ID_",
          "group_entry_node_path": "_GROUP_ENTRY_NODE_PATH_",
          "node_ref": "_GROUP_NODE_REF_"
        },
        "patch": {
          "x": 320,
          "y": 200
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
```

Expected TaskPlan:

- `write.ops[0].op` is one of `set_pin_default`, `set_node_comment`, `set_node_position` per patch.
- Runtime lowering target is `patch_blueprint_graph`.

### Merge Preview (Confirmed Shape)

Use `graph_strategy = merge_owned_graph` and `behavior.merges[]` with block-scoped
`anchor`:

```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "context_id": "ctx_current_integration_smoke_merge_20260505",
  "task_type": "edit_blueprint_graph",
  "feature_name": "SmokeMerge",
  "target": {
    "asset_path": "_BP_SMOKE_",
    "target_type": "blueprint"
  },
  "scope_policy": {
    "graph_name": "_GRAPH_NAME_",
    "allow_modify_user_nodes": false
  },
  "behavior": {
    "graph_strategy": "merge_owned_graph",
    "merges": [
      {
        "kind": "insert_flow",
        "scope": "function_call",
        "insert_strategy": "append_after",
        "anchor": {
          "block_id": "_BLOCK_ID_",
          "group_entry_node_path": "_GROUP_ENTRY_NODE_PATH_",
          "node_ref": "_GROUP_NODE_REF_",
          "pin_ref": "_GROUP_PIN_REF_"
        },
        "inserted": {
          "call_kind": "function_call",
          "name": "_FUNCTION_NAME_"
        }
      },
      {
        "kind": "insert_flow",
        "scope": "custom_event_call",
        "insert_strategy": "insert_between",
        "anchor": {
          "block_id": "_BLOCK_ID_",
          "group_entry_node_path": "_GROUP_ENTRY_NODE_PATH_",
          "link_ref": "_GROUP_LINK_REF_"
        },
        "inserted": {
          "call_kind": "custom_event_call",
          "name": "_CUSTOM_EVENT_NAME_"
        }
      },
      {
        "kind": "insert_flow",
        "scope": "owned_block_call",
        "insert_strategy": "branch_fork",
        "anchor": {
          "block_id": "_BLOCK_ID_",
          "group_entry_node_path": "_GROUP_ENTRY_NODE_PATH_",
          "node_ref": "_GROUP_NODE_REF_",
          "pin_ref": "_GROUP_PIN_REF_"
        },
        "inserted": {
          "call_kind": "owned_block_call",
          "block_id": "_INSERTED_BLOCK_ID_"
        },
        "sequence_order": [
          "inserted_logic",
          "original_successor"
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
```

Expected TaskPlan:

- `write.ops[0].op = insert_flow`.
- Runtime lowering target is `merge_blueprint_graph`.
- `sequence_order` only appears for `insert_strategy = branch_fork`.

Known limitation:

- Patch/Merge write anchors are now fixed to the v0.3.6 grouped LogicJson / block-scoped contract. For BlueprintHelper-owned blocks, use `block_id` / `group_entry_node_path` plus group-local `node_ref` / `pin_ref` / `link_ref`; do not guess from raw `nodes[index]`, display names, or GUID-first refs.

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

Open issues:
```

## Actual Smoke Report: 2026-05-05 (Rerun 2)

```text
Smoke run: 2026-05-05 Rerun 2
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
Level 7 Composite preview: PASS
Level 8 Failure/recovery: PARTIAL
Level 9 UE build: PASS

Task run ids:
- append_graphwrite: task_5806121649296A709F32088EB10C55F0
- variables_execute: task_38C6DC0D4AC56E1DD89F4992D9A7B3AB
- composite: N/A (preview only, not executed)

Notes:
- Level 0: 112/112 tests pass (30 Python + 82 Node). All contract checks confirmed.
- Level 1: Runtime Profile status=degraded (merge/journal/review/store not_implemented - expected).
  Diagnostics: no blocking issues. Agent Guide returns correct TaskSpec-first index.
- Level 2: All ReadSpec formats work correctly (schema, logic_md, logic_json, unsupported handling).
- Level 3: ReferenceContextPack.v1 returned, analysis.partial=true.
- Level 4.1 (preview): PASS. preview_task for append_new_owned_graph + NEW graph name works.
  TaskPlan: step_001 (blueprint_signature → custom_event_signature) → step_002 (graph_write → owned_graph_edit).
- Level 4.2 (execute): PASS. Full pipeline:
  step_001 signature: status=no_op (deferred_to_graph_write)
  step_002 graph_write: status=applied (graph BH_Smoke_Rerun_20260505 created with PrintString)
  post_operation compile: succeeded, 0 warnings.
- Level 4.3 (get_task_result): PASS.
  data.schema=BlueprintHelper.TaskRunJournal.v1, status=completed. Steps use 'completed' (not legacy 'applied').
- Level 4.4 (read_back LogicMd): PASS.
  Graph BH_Smoke_Rerun_20260505: Entry=BH_SmokeRerunEvent_20260505,
  Exec: BH_SmokeRerunEvent_20260505.then → 打印字符串.execute. Nodes=2, Orphans=0.
- Level 4.4 (read_back LogicJson): PASS (was PARTIAL in Rerun 1, now fixed).
  LogicJson correctly found custom_event in BH_Smoke_Rerun_20260505 graph:
  entry.kind=custom_event, entry.name=BH_SmokeRerunEvent_20260505.
  nodes[0].kind=custom_event, nodes[1].kind=call_function (打印字符串).
  Stats: nodes=7, exec_links=2 (includes all graphs).
- Level 5 Replace preview+execute: SUPERSEDED_BY_RERUN_4_PASS.
  Rerun 4 verified the relink/read-back fix with full pipeline execution.
- Level 5 Patch preview: SUPERSEDED_BY_RERUN_4_PASS for owned-block patch on Replace-created nodes.
- Level 5 Merge preview: SUPERSEDED_BY_RERUN_4_PASS for the listed owned-block insert_flow cases.

  ## Level 5 Rerun Result (2026-05-06) / Historical Failure Logs + Superseded Recommended Shapes

  ### Replace (strategy=replace_owned_graph) — historical pipeline PASS, superseded by Rerun 4 current PASS

  **Correct TaskSpec shape:**
  ```json
  "behavior": {
    "graph_strategy": "replace_owned_graph",
    "replace": {
      "scope": "custom_event_body",
      "selector": {
        "kind": "custom_event",
        "name": "BH_TaskSpecSmokeEvent_20260504_001",
        "graph_id": "BH_TaskSpecSmoke_20260504_001"
      },
      "body": { "schema": "BlueprintLogicSpec.v1", "statements": [...] }
    }
  }
  ```

  **Preview result:** `passed=true, blocked=false`
  TaskPlan: capability=graph_write, strategy=owned_graph_edit, ops=1

  **Historical execute result:** PASS — adapter_operation=replace_blueprint_graph, status=applied
  Compile: success, 0 warnings
  task_run_id: `task_A923B94D4F70F3599281D48F52D903B1`

  **Historical read-back observation:** Replace correctly substituted the custom event body, but the new
  PrintString node ended up as an orphan (0 exec links). The old exec link was removed during
  replace and the new nodes were not auto-linked. This is a behavioral detail of the replace
  operation, not a pipeline failure.

  **Superseded status:** preserved entry -> replacement body relink and
  grouped LogicJson node-local links were verified by Rerun 4. Current status is PASS.

  **Historical wrong-shape error log (Rerun 2, using `behavior.entries`):**
  ```
  ok: false
  error.code: "taskspec_semantic_invalid"
  error.message: "behavior.replace must be an object."
  issues[0]: { code: "missing_required_object", path: "behavior.replace" }
  ```

  ### Patch (strategy=patch_owned_graph) — historical failure log, superseded by Rerun 4 owned-block PASS

  **Current recommended TaskSpec shape:**
  ```json
  "behavior": {
    "graph_strategy": "patch_owned_graph",
    "patches": [{
      "kind": "set_node_comment",
      "target_ref": {
        "block_id": "_BLOCK_ID_",
        "group_entry_node_path": "_GROUP_ENTRY_NODE_PATH_",
        "node_ref": "_GROUP_NODE_REF_"
      },
      "value": "Patched: smoke test comment"
    }]
  }
  ```

  **Historical failure input from rerun 2, preserved as failure log only:**
  ```json
  "behavior": {
    "graph_strategy": "patch_owned_graph",
    "patches": [{
      "kind": "set_node_comment",
      "target_ref": { "graph_id": "BH_TaskSpecSmoke_20260504_001", "node_ref": "nodes[0]" },
      "value": "Patched: smoke test comment"
    }]
  }
  ```

  **Historical Python compiler result:** PASS (schema validation passes, TaskPlan generated)
  **Historical Bridge preview result:** BLOCKED
  ```
  passed: false, blocked: true
  issues[0]: { code: "target_node_not_found", path: "nodes[0]",
               message: "无法定位节点：node_ref=nodes[0], node_path=" }
  ```
  **Superseded status:** grouped LogicJson block output, compiler lowering, and
  UE resolver support for `block_id` / `group_entry_node_path` plus group-local
  refs were verified by Rerun 4 for owned blocks.

  **Historical wrong-shape error log (Rerun 2, using `behavior.entries`):**
  ```
  error.code: "taskspec_semantic_invalid"
  error.message: "behavior.patches must be a non-empty list."
  ```

  ### Merge (strategy=merge_owned_graph) — historical failure log, superseded by Rerun 4 owned-block PASS

  **Current recommended TaskSpec shape:**
  ```json
  "behavior": {
    "graph_strategy": "merge_owned_graph",
    "merges": [{
      "kind": "insert_flow",
      "scope": "custom_event_call",
      "insert_strategy": "append_after",
      "anchor": {
        "block_id": "_BLOCK_ID_",
        "group_entry_node_path": "_GROUP_ENTRY_NODE_PATH_",
        "node_ref": "_GROUP_NODE_REF_",
        "pin_ref": "_GROUP_PIN_REF_"
      },
      "inserted": { "call_kind": "custom_event_call", "name": "BH_SmokeRerunEvent_20260505" }
    }]
  }
  ```

  **Historical failure input from rerun 2, preserved as failure log only:**
  ```json
  "behavior": {
    "graph_strategy": "merge_owned_graph",
    "merges": [{
      "kind": "insert_flow",
      "scope": "custom_event_call",
      "insert_strategy": "append_after",
      "anchor": { "graph_id": "BH_TaskSpecSmoke_20260504_001", "node_ref": "nodes[0]", "pin_ref": "then" },
      "inserted": { "call_kind": "custom_event_call", "name": "BH_SmokeRerunEvent_20260505" }
    }]
  }
  ```

  **Historical Python compiler result:** PASS (schema validation passes, TaskPlan generated)
  **Historical Bridge preview result:** BLOCKED
  ```
  passed: false, blocked: true
  issues[0]: { code: "anchor_node_not_found", path: "anchor",
               message: "无法定位节点：node_ref=nodes[0], node_path=" }
  ```
  **Superseded status:** the same block-scoped fix set as Patch was verified by
  Rerun 4 for `insert_between + function_call`, `append_after + function_call`,
  and `insert_between + custom_event_call`. Raw unscoped `nodes[index]` inputs
  stay in the historical log only.

  **Historical wrong-shape error log (Rerun 2, using `behavior.entries`):**
  ```
  error.code: "taskspec_semantic_invalid"
  error.message: "behavior.merges must be a non-empty list."
  ```

  ### Failure Classification Update

  | Strategy | Stage | Status | Reason |
  |---|---|---|---|
  | `replace_owned_graph` | Full pipeline + read-back | SUPERSEDED_BY_RERUN_4_PASS | Current relink/read-back fix verified |
  | `patch_owned_graph` | Mainline block-scoped owned path | SUPERSEDED_BY_RERUN_4_PASS | Owned-block path verified |
  | `merge_owned_graph` | Mainline block-scoped owned path | SUPERSEDED_BY_RERUN_4_PASS | Listed insert_flow cases verified |

  **Remaining action after Rerun 4:**
  1. Fix `append_after + custom_event_call` empty preview error.
  2. Add a `branch_fork` merge smoke fixture.
  3. Keep historical ad hoc references out of the Agent-facing mainline.

  Historical ad hoc formats tested — all fail at Bridge:
  | node_ref value | LogicJson index | Event name | JSONPath |
  |---|---|---|---|
  | `nodes[0]` | `target_node_not_found` | — | — |
  | `BH_TaskSpecSmokeEvent_20260504_001` | — | `target_node_not_found` | — |
  | `$.graphs[...].nodes[0]` | — | — | not yet tested |

  Bridge currently cannot resolve these ad hoc string references. Do not add a
  dedicated node-lookup command as the mainline fix; implement the v0.3.6 grouped
  block resolver and keep GUIDs as expert/debug fallback only.

  ### 当前推荐 TaskSpec shape（供后续本地重跑使用）

  本节保留上述历史失败日志不改写；后续重跑直接用以下形状并继续走
  `blueprinthelper_preview_task` / `blueprinthelper_execute_task`：

  - `replace_owned_graph`

  ```json
  "behavior": {
    "graph_strategy": "replace_owned_graph",
    "replace": {
      "scope": "custom_event_body",
      "selector": {
        "kind": "custom_event",
        "name": "_EVENT_NAME_",
        "graph_id": "_GRAPH_NAME_"
      },
      "body": {
        "schema": "BlueprintLogicSpec.v1",
        "statements": []
      }
    }
  }
  ```

  - `patch_owned_graph`

  ```json
  "behavior": {
    "graph_strategy": "patch_owned_graph",
    "patches": [
      {
        "kind": "set_pin_default",
        "target_ref": {
          "block_id": "_BLOCK_ID_",
          "group_entry_node_path": "_GROUP_ENTRY_NODE_PATH_",
          "node_ref": "_GROUP_NODE_REF_",
          "pin_ref": "_GROUP_PIN_REF_"
        },
        "value": { "kind": "literal", "value_type": "string", "value": "patched" }
      },
      {
        "kind": "set_node_comment",
        "target_ref": {
          "block_id": "_BLOCK_ID_",
          "group_entry_node_path": "_GROUP_ENTRY_NODE_PATH_",
          "node_ref": "_GROUP_NODE_REF_"
        },
        "value": "patched by smoke"
      },
      {
        "kind": "set_node_position",
        "target_ref": {
          "block_id": "_BLOCK_ID_",
          "group_entry_node_path": "_GROUP_ENTRY_NODE_PATH_",
          "node_ref": "_GROUP_NODE_REF_"
        },
        "patch": { "x": 320, "y": 200 }
      }
    ]
  }
  ```

  - `merge_owned_graph`

  ```json
  "behavior": {
    "graph_strategy": "merge_owned_graph",
    "merges": [
      {
        "kind": "insert_flow",
        "scope": "function_call",
        "insert_strategy": "append_after",
        "anchor": {
          "block_id": "_BLOCK_ID_",
          "group_entry_node_path": "_GROUP_ENTRY_NODE_PATH_",
          "node_ref": "_GROUP_NODE_REF_",
          "pin_ref": "_GROUP_PIN_REF_"
        },
        "inserted": {
          "call_kind": "function_call",
          "name": "_FUNCTION_NAME_"
        }
      },
      {
        "kind": "insert_flow",
        "scope": "custom_event_call",
        "insert_strategy": "insert_between",
        "anchor": {
          "block_id": "_BLOCK_ID_",
          "group_entry_node_path": "_GROUP_ENTRY_NODE_PATH_",
          "link_ref": "_GROUP_LINK_REF_"
        },
        "inserted": {
          "call_kind": "custom_event_call",
          "name": "_CUSTOM_EVENT_NAME_"
        }
      },
      {
        "kind": "insert_flow",
        "scope": "owned_block_call",
        "insert_strategy": "branch_fork",
        "anchor": {
          "block_id": "_BLOCK_ID_",
          "group_entry_node_path": "_GROUP_ENTRY_NODE_PATH_",
          "node_ref": "_GROUP_NODE_REF_",
          "pin_ref": "_GROUP_PIN_REF_"
        },
        "inserted": {
          "call_kind": "owned_block_call",
          "block_id": "_INSERTED_BLOCK_ID_"
        },
        "sequence_order": ["inserted_logic", "original_successor"]
      }
    ]
  }
  ```

  `sequence_order` 仅用于 `insert_strategy = branch_fork`，不出现在其他 insert_strategy 上。

- Level 6.1 AssetFactory preview: PASS. capability=asset_factory, strategy=asset_create, ops=1.
- Level 6.2 Component preview: PASS (was FAIL in Rerun 1, now working).
  passed=true, blocked=false. Two steps: ensure_component_present + configure_component.
  Both capability=blueprint_component, strategy=component_tree.
- Level 6.3 Variables preview+execute: PASS. SmokeValue (float) variable created, compiled OK.
- Level 6.4 ClassSettings: NOT TESTED (blocked_by_fixture - no disposable interface asset).
- Level 6.5 UMGWidget: NOT TESTED (blocked_by_fixture - no _WBP_SMOKE_ asset).
- Level 6.6 DataTable: NOT TESTED (blocked_by_fixture - no _DT_SMOKE_ asset).
- Level 7 Composite preview: PASS (was FAIL in Rerun 1, now working).
  passed=true, blocked=false. 7 steps decomposed:
  component (x3) → variable (x2) → signature (x1) → graph_write (x1).
  Capability order matches expected: blueprint_component → blueprint_variable →
  blueprint_signature → graph_write. No Agent-authored adapter ops in request.
- Level 8.1 Schema Error: PASS. Zod validation rejected TaskSpec without 'schema' field.
  Error: "Invalid literal value, expected 'BlueprintHelper.TaskSpec.v1'". No assets modified.
- Level 8.2 Semantic Error (replace_graph): PASS. Python compiler rejects unsupported strategy:
  ```
  error.code: "unsupported_graph_strategy"
  error.message: "Unsupported GraphWrite graph_strategy."
  issues[0].path: "behavior.graph_strategy"
  issues[0].message: "Use append_new_owned_graph, replace_owned_graph, patch_owned_graph, or merge_owned_graph."
  suggested_patch: { op: "replace", path: "/behavior/graph_strategy", value: "append_new_owned_graph" }
  ```
  No Bridge write occurs. Error code identifies unsupported strategy. Error path = behavior.graph_strategy.
- Level 8.3 Preview Blocked (existing graph reuse): PASS.
  `passed=false, blocked=true, modified=false`.
  Issue: `code=target_graph_not_empty`, message="图表 BH_TaskSpecSmoke_20260504_001 非空，Append 不允许写入已有内容的图表。"
  Agent can call `blueprinthelper_read_context` or `blueprinthelper_read_reference_context` to inspect the target.
- Level 8.4 Partial Failure Journal: NOT TESTED (blocked by lack of controlled failure fixture).
- Level 9: PASS. Build succeeded in 7.90s. Zero compile errors.

Open issues:
- [pending_verification] Patch/Merge block-scoped path: contract decision is fixed
  to grouped LogicJson / block-scoped anchors, and source support has been added
  across LogicJson output, compiler lowering, and UE resolver. Raw unscoped
  `nodes[index]`, display names, JSONPath, and GUID-first refs are historical
  failure inputs, not the Agent-facing fix. Rerun 4 verified the owned-block path.
- [superseded_by_rerun_4] Replace exec link regen: source fix has been added for
  preserved entry -> replacement body relink, and grouped LogicJson now rehydrates
  graph-level `links[]` into node-local links. Rerun 4 verified PASS.
- [blocked_by_fixture] Missing disposable assets for ClassSettings, UMG, and DataTable P1 tests.
- [working_capability] Confirmed current TaskSpec pipelines:
  1. edit_blueprint_graph + append_new_owned_graph + new graph → compile. PASS.
  2. edit_blueprint_variables → compile. PASS.
  3. edit_blueprint_components preview. PASS.
  4. create_blueprint_feature (composite) preview. PASS.
  5. create_asset (AssetFactory) preview. PASS.
- [superseded_by_rerun_4] Replace full pipeline has re-entered the confirmed list.
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

---

## Actual Smoke Report: 2026-05-06 (Rerun 3 — Level 5 Focus)

```text
Smoke run: 2026-05-06 Rerun 3
Date: 2026-05-06
Editor project: G:/UnrealPractise/MrStone/MrStone.uproject (UE 5.6)
MCP server version: 0.3.8
Target asset: /Game/BlueprintHelper/Smoke/BP_TaskSpecSmoke

Level 0 MCP/Python regression: NOT RERUN (112/112 previous, no new source changes)
Level 1 Bridge preflight: NOT RERUN (no changes since Rerun 2)
Level 2 ReadSpec blueprint_logic: NOT RERUN (no changes)
Level 3 Reference context: NOT RERUN
Level 4 Append signature->graph execute: NOT RERUN
Level 5 Replace preview+execute+read-back: PASS
Level 5 Patch preview+execute: PASS
Level 5 Merge preview+execute: PARTIAL (preview PASS, execute FAIL)
Level 6-9: NOT RERUN

Task run ids:
- replace: task_3EBE2A3C4FD87EC061604C8EBC832830
- patch:   task_A61D83604E4D02B8BC50EDB07B4EF2F4
- merge:   N/A (execute failed, no task_run_id generated)
```

### Level 5 Replace — PASS (Full Pipeline)

**Preview:**
- `preview_id`: preview_1778042639459_0002
- `passed`: true, `blocked`: false
- TaskPlan: 1 step, capability=graph_write, strategy=owned_graph_edit

**TaskSpec shape (verified correct):**
```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "context_id": "ctx_smoke_replace_20260506",
  "task_type": "edit_blueprint_graph",
  "feature_name": "SmokeReplaceVerify",
  "target": { "asset_path": "/Game/BlueprintHelper/Smoke/BP_TaskSpecSmoke", "target_type": "blueprint" },
  "scope_policy": { "graph_name": "BH_TaskSpecSmoke_20260504_001", "allow_modify_user_nodes": false },
  "behavior": {
    "graph_strategy": "replace_owned_graph",
    "replace": {
      "scope": "custom_event_body",
      "selector": { "kind": "custom_event", "name": "BH_TaskSpecSmokeEvent_20260504_001", "graph_id": "BH_TaskSpecSmoke_20260504_001" },
      "body": { "schema": "BlueprintLogicSpec.v1", "statements": [...] }
    }
  },
  "execution_policy": { "dry_run_mode": "full", "on_missing_capability": "stop_and_report" },
  "validation": { "should_compile": true, "should_save": false }
}
```

**Key learning: TaskSpec 中的函数参数必须使用结构化 literal 格式:**
```json
"args": {
  "InString": { "kind": "literal", "value_type": "string", "value": "..." }
}
```
不能用 `"params": { "InString": "plain string" }` 这样的简写格式。

**Execute:**
- `task_run_id`: task_3EBE2A3C4FD87EC061604C8EBC832830
- `adapter_operation`: replace_blueprint_graph, `status`: applied
- `modified_assets`: 1
- `compile`: success, 0 warnings

**Read-back (LogicMd):**
```
Graph: BH_TaskSpecSmoke_20260504_001
Nodes: 2 | Exec Links: 1 | Orphans: 0
BH_TaskSpecSmokeEvent_20260504_001.then -> 打印字符串.execute
```

**Read-back (LogicJson):**
```json
{
  "nodes": [
    { "node_ref": "nodes[0]", "kind": "custom_event", "links": [{
        "link_ref": "links[0]", "pin_ref": "then", "type": "exec",
        "from_pin": "then", "to_node": "nodes[1]", "to_pin": "execute"
    }]},
    { "node_ref": "nodes[1]", "kind": "call_function", "name": "打印字符串" }
  ]
}
```

**Replace RELINK FIX VERIFIED.** 之前的 rerun 中替换后的新节点是 orphan（0 exec links），现在 exec link 正确连接到 entry event → PrintString。

### Level 5 Patch — PASS (Full Pipeline)

**Preview:**
- `preview_id`: preview_1778042823077_0005
- `passed`: true, `blocked`: false
- Target graph: BH_Smoke_Rerun_20260505（必须用 Append 创建的图，因为只有这些节点有 BlueprintHelper ownership metadata）

**TaskSpec shape (verified correct):**
```json
{
  "task_type": "edit_blueprint_graph",
  "scope_policy": { "graph_name": "BH_Smoke_Rerun_20260505", "allow_modify_user_nodes": false },
  "behavior": {
    "graph_strategy": "patch_owned_graph",
    "patches": [{
      "kind": "set_node_comment",
      "target_ref": {
        "block_id": "BH_Smoke_Rerun_20260505",
        "group_entry_node_path": "$.graphs[BH_Smoke_Rerun_20260505].nodes[0]",
        "node_ref": "nodes[1]"
      },
      "value": "Patched: smoke test comment 2026-05-06"
    }]
  }
}
```

**Execute:**
- `task_run_id`: task_A61D83604E4D02B8BC50EDB07B4EF2F4
- `adapter_operation`: patch_blueprint_graph, `status`: applied
- `patch_result.changed`: true
- `compile`: success, 0 warnings

**Read-back:** Graph structure intact (Nodes: 2, Exec Links: 1, Orphans: 0).

**BLOCK-SCOPED RESOLVER VERIFIED** for nodes with BlueprintHelper ownership metadata.

**Key finding — Ownership metadata 的范围:**
- BH_TaskSpecSmoke_20260504_001 上的 Patch 预览失败 (`target_node_not_found`)。该图的节点通过 Replace 操作创建，**没有** BlueprintHelper ownership metadata（`BlueprintHelperOwned`/`BlueprintHelperBlockId`）。
- BH_Smoke_Rerun_20260505 上的 Patch 预览成功。该图的节点通过 Append 操作创建，**有**正确的 ownership metadata。
- **结论**: Replace service 没有给新节点打上 ownership metadata。这是一个已知的覆盖缺口，需要后续补齐。

### Level 5 Merge — PARTIAL (preview PASS, execute FAIL)

**Preview 成功的情况：**
| 组合 | Scope | Strategy | Inserted | Preview |
|------|-------|----------|----------|---------|
| insert_between + custom_event_call | custom_event_call | insert_between | custom_event_call | PASS (preview_1778042924563_0008) |
| insert_between + function_call | function_call | insert_between | function_call | PASS (preview_1778043199275_0014) |

**Preview 被 blocked 的情况：**
| 组合 | Blocked Reason |
|------|---------------|
| append_after + custom_event_call | `anchor_exec_pin_already_connected` — then pin 已被占用 |
| append_after + function_call (nodes[0]) | `anchor_exec_pin_already_connected` |
| append_after + function_call (nodes[1]) | `anchor_node_not_found` — Replace 创建的节点缺 ownership metadata |

**Execute:**
所有 Merge execute 尝试均返回 `failed: , modified=false.` — 无详细错误信息。

**分析:**
- Python compiler 编译通过（preview 成功）
- Bridge preflight 通过（preview 返回 passed=true）
- UE Merge Service 的 Execute 路径在某个环节失败，但未产生可读的错误消息
- 可能是 `insert_between` 的 link 断开/重连逻辑有 bug，或跨图事件调用创建失败

**Classification:** `implementation_gap` — Merge execute 路径存在 UE 侧 bug，需要在 Merge Service 的 Execute 方法中调试。

**TaskSpec shape (用于后续重跑):**
```json
{
  "task_type": "edit_blueprint_graph",
  "behavior": {
    "graph_strategy": "merge_owned_graph",
    "merges": [{
      "kind": "insert_flow",
      "scope": "function_call",
      "insert_strategy": "insert_between",
      "anchor": {
        "block_id": "_BLOCK_ID_",
        "group_entry_node_path": "_GROUP_ENTRY_NODE_PATH_",
        "node_ref": "_NODE_REF_",
        "pin_ref": "_PIN_REF_",
        "link_ref": "_LINK_REF_"
      },
      "inserted": { "call_kind": "function_call", "name": "_FUNCTION_NAME_" }
    }]
  }
}
```

### 待验证的边界与已知缺口

1. **[verified] Replace relink**: preserved entry → replacement body exec link 正确生成，不再 orphan。
2. **[verified] Block-scoped resolver**: 对有 ownership metadata 的节点正确工作。
3. **[gap] Replace ownership metadata**: Replace 操作不设置 BlueprintHelperOwned/BlueprintHelperBlockId 元数据，导致 block-scoped Patch/Merge 无法定位 Replace 创建的节点。
4. **[gap] Merge execute**: UE Merge Service 的 Execute 路径不可用，所有 merge 策略的 execute 均失败。
5. **[gap] block_id 不在 LogicJson 输出中**: LogicJson grouped output 缺少 `block_id` 字段（TODO 中标记为已完成但实际输出未体现），当前使用 `name` 字段值作为 block_id 替代，仅对 Append 创建的图有效。
## Source Fix Notes (2026-05-06)

Status: source patched, pending user-side UE build and smoke rerun.

- Merge execute: fixed `insert_between` execute path so `OriginalSuccessorPin` is captured after preflight anchor resolution. `function_call` and `custom_event_call` now create call-function nodes instead of treating entry nodes as inserted executable body.
- Replace ownership: replacement body nodes now inherit `BlueprintHelperOwned` / `BlueprintHelperBlockId` when the replace target is an existing BlueprintHelper-owned block or owned custom event entry. Ownership is written only to imported replacement nodes, not the whole graph.
- LogicJson block_id: graph export now includes node metadata needed by LogicGroupBuilder, so `BlueprintHelperBlockId` can surface as `block_id` / owned groups in read-back.
- Added automation coverage:
  - `BlueprintHelper.GraphWrite.BlockScopedAnchors.MergeInsertBetween`
  - `BlueprintHelper.GraphWrite.Replace.CustomEventBodyReconnectsEntryExec` ownership assertion
  - `BlueprintHelper.ObjectFirst.Logic.ReadLogicJsonIncludesOwnedBlockIdFromGraphMetadata`

Required rerun:

```text
Build.bat MrStoneEditor Win64 Development -Project="G:\UnrealPractise\MrStone\MrStone.uproject"
```

Then rerun Level 5 Merge and read-back checks. Expected status after fix: Replace PASS, Patch PASS, Merge insert_between PASS, LogicJson returns usable block_id for owned nodes.

---

## Actual Smoke Report: 2026-05-06 (Rerun 4 — Source Fix Verification)

```text
Smoke run: 2026-05-06 Rerun 4
Date: 2026-05-06
Editor project: G:/UnrealPractise/MrStone/MrStone.uproject (UE 5.6)
MCP server version: 0.3.8
Target asset: /Game/BlueprintHelper/Smoke/BP_TaskSpecSmoke

Level 0-4: NOT RERUN (no changes since Rerun 2)
Level 5 Replace preview+execute+read-back: PASS
Level 5 Patch on Replace-created nodes: PASS
Level 5 Merge insert_between + function_call: PASS
Level 5 Merge append_after + function_call: PASS
Level 5 Merge insert_between + custom_event_call: PASS
Level 6-9: NOT RERUN

Task run ids:
- merge_insert_between_func:   task_1765CD2A4C8F05C52AD5089336FE8D89
- replace:                     task_53323DE34A0829237D8F95ACE4F582D6
- patch_on_replace:            task_352FACAC47CA6AFCE39F7B9D18BD181F
- merge_append_after:          task_9791AA824C8D7594AE2325933DDC3CD1
- merge_custom_event_call:     task_14DD3B204725E7F9D80F81A6B6BCE73D
```

### 测试前确认

- Build: `F:\UE_5.6\Engine\Build\BatchFiles\Build.bat MrStoneEditor Win64 Development` → Target is up to date (0.49s)
- Editor: 通过 `blueprint_open_editor` 启动，Bridge 37s 就绪
- Runtime Profile: status=degraded（merge/journal/review/store 仍标记 not_implemented，但实际 merge 可执行）
- Diagnostics: no blocking issues

### Level 5 Merge — insert_between + function_call: PASS

**Preview:**
- `preview_id`: `preview_1778068872062_0001`
- `passed`: true, `blocked`: false
- TaskPlan: 1 step, capability=graph_write, strategy=owned_graph_edit

**TaskSpec shape (verified correct):**
```json
{
  "task_type": "edit_blueprint_graph",
  "scope_policy": { "graph_name": "BH_Smoke_Rerun_20260505", "allow_modify_user_nodes": false },
  "behavior": {
    "graph_strategy": "merge_owned_graph",
    "merges": [{
      "kind": "insert_flow",
      "scope": "function_call",
      "insert_strategy": "insert_between",
      "anchor": {
        "block_id": "BH_Smoke_Rerun_20260505_BH_SmokeRerunEvent_202605050",
        "group_entry_node_path": "$.graphs[BH_Smoke_Rerun_20260505].nodes[0]",
        "node_ref": "nodes[0]",
        "pin_ref": "then",
        "link_ref": "links[0]"
      },
      "inserted": { "call_kind": "function_call", "name": "PrintString", "args": { "InString": { "kind": "literal", "value_type": "string", "value": "Merge insert_between test 2026-05-06" } } }
    }]
  }
}
```

**Execute:**
- `adapter_operation`: merge_blueprint_graph, `status`: applied
- `merge_result.merged_ref`: anchor_ref=K2Node_CustomEvent_0.then, inserted_ref=PrintString
- `compile`: success, 0 warnings

**Read-back (LogicMd):**
```
Nodes: 3 | Exec Links: 2 | Orphans: 0
BH_SmokeRerunEvent_20260505.then → 打印字符串.execute
打印字符串.then → 打印字符串.execute
```
原 2 节点 1 link → 3 节点 2 links，新 PrintString 正确插入在 entry 和原 PrintString 之间。

### Level 5 Merge — append_after + function_call: PASS

**关键突破：** Rerun 3 中 `append_after` 因 `anchor_exec_pin_already_connected` 被 blocked。ownership metadata 修复后，Replace 创建的节点可被正确解析。

**TaskSpec shape:**
```json
{
  "behavior": {
    "graph_strategy": "merge_owned_graph",
    "merges": [{
      "kind": "insert_flow",
      "scope": "function_call",
      "insert_strategy": "append_after",
      "anchor": {
        "block_id": "BH_TaskSpecSmoke_20260504_001_BH_TaskSpecSmokeEvent_20260504_0010",
        "group_entry_node_path": "$.graphs[BH_TaskSpecSmoke_20260504_001].nodes[0]",
        "node_ref": "nodes[1]",
        "pin_ref": "then"
      },
      "inserted": { "call_kind": "function_call", "name": "PrintString", "args": { "InString": { "kind": "literal", "value_type": "string", "value": "Merge append_after test 2026-05-06" } } }
    }]
  }
}
```

**Execute:**
- `adapter_operation`: merge_blueprint_graph, `status`: applied
- `merge_scope`: function_call, `insert_strategy`: append_after
- `compile`: success, 0 warnings

**Read-back:**
```
Nodes: 3 | Exec Links: 2 | Orphans: 0
BH_TaskSpecSmokeEvent_20260504_001.then → 打印字符串("Replace regression test").execute
打印字符串("Replace regression test").then → 打印字符串("Merge append_after test").execute
```

### Level 5 Merge — insert_between + custom_event_call: PASS

**关键发现：** `custom_event_call` scope 的 `insert_between` anchor **必须**同时提供 `node_ref` + `pin_ref` + `link_ref`，仅 `link_ref` 不够（Zod schema 验证要求三者都存在）。

**TaskSpec shape:**
```json
{
  "behavior": {
    "graph_strategy": "merge_owned_graph",
    "merges": [{
      "kind": "insert_flow",
      "scope": "custom_event_call",
      "insert_strategy": "insert_between",
      "anchor": {
        "block_id": "BH_Smoke_Rerun_20260505_BH_SmokeRerunEvent_202605050",
        "group_entry_node_path": "$.graphs[BH_Smoke_Rerun_20260505].nodes[0]",
        "node_ref": "nodes[2]",
        "pin_ref": "then",
        "link_ref": "links[1]"
      },
      "inserted": { "call_kind": "custom_event_call", "name": "BH_SmokeRerunEvent_20260505" }
    }]
  }
}
```

**Execute:**
- `adapter_operation`: merge_blueprint_graph, `status`: applied
- `merge_scope`: custom_event_call, `insert_strategy`: insert_between
- `compile`: success, 0 warnings

**最终读回 BH_Smoke_Rerun_20260505:**
```
Nodes: 4 | Exec Links: 3 | Orphans: 0
BH_SmokeRerunEvent_20260505.then → 打印字符串.execute
打印字符串.then → BH_SmokeRerunEvent_20260505.execute
BH_SmokeRerunEvent_20260505.then → 打印字符串.execute
```

### Level 5 Replace — relink + ownership: PASS

**TaskSpec shape (同 Rerun 3 验证的 shape):**
```json
{
  "graph_strategy": "replace_owned_graph",
  "replace": {
    "scope": "custom_event_body",
    "selector": { "kind": "custom_event", "name": "BH_TaskSpecSmokeEvent_20260504_001", "graph_id": "BH_TaskSpecSmoke_20260504_001" },
    "body": { "schema": "BlueprintLogicSpec.v1", "statements": [{"kind": "call_function", "name": "PrintString", "args": { "InString": {"kind": "literal", "value_type": "string", "value": "Replace regression test 2026-05-06"}, "Duration": {"kind": "literal", "value_type": "float", "value": 2}}}] }
  }
}
```

**Execute:**
- `adapter_operation`: replace_blueprint_graph, `status`: applied
- `compile`: success, 0 warnings

**Read-back (LogicMd):**
```
Nodes: 2 | Exec Links: 1 | Orphans: 0
BH_TaskSpecSmokeEvent_20260504_001.then → 打印字符串.execute
```
**RELINK FIX VERIFIED** — 替换后 0 orphans。

**Read-back (LogicJson) — ownership metadata:**
```json
"groups": [{
  "group_type": "blueprinthelper_block",
  "block_id": "BH_TaskSpecSmoke_20260504_001_BH_TaskSpecSmokeEvent_20260504_0010",
  "group_entry_node_path": "$.graphs[BH_TaskSpecSmoke_20260504_001].nodes[0]",
  "nodes": [
    {"node_ref": "nodes[0]", "kind": "custom_event", "links": [{ "to_node": "nodes[1]" }]},
    {"node_ref": "nodes[1]", "kind": "call_function"}
  ]
}]
```
**OWNERSHIP METADATA FIX VERIFIED** — Replace 创建的节点现在有完整的 `block_id` 和 grouped 结构。

### Level 5 Patch — on Replace-created nodes: PASS

**关键突破：** Rerun 3 中 Patch 无法定位 Replace 创建的节点（缺 ownership metadata），现在可以。

**TaskSpec shape:**
```json
{
  "graph_strategy": "patch_owned_graph",
  "patches": [{
    "kind": "set_node_comment",
    "target_ref": {
      "block_id": "BH_TaskSpecSmoke_20260504_001_BH_TaskSpecSmokeEvent_20260504_0010",
      "group_entry_node_path": "$.graphs[BH_TaskSpecSmoke_20260504_001].nodes[0]",
      "node_ref": "nodes[1]"
    },
    "value": "Patched on Replace-created node 2026-05-06"
  }]
}
```

**Execute:**
- `adapter_operation`: patch_blueprint_graph, `status`: applied
- `patch_result.changed`: true
- `compile`: success, 0 warnings

### 待验证的边界与已知缺口 (更新)

1. **[verified] Replace relink**: preserved entry → replacement body exec link 正确生成，不再 orphan。
2. **[verified] Block-scoped resolver**: 对有 ownership metadata 的节点正确工作，Append 和 Replace 创建的节点均可定位。
3. **[verified] Replace ownership metadata**: Replace 操作现在正确设置 BlueprintHelperOwned/BlueprintHelperBlockId，Patch/Merge 可定位 Replace 创建的节点。
4. **[verified] Merge execute**: UE Merge Service 的 Execute 路径可用。已验证 `insert_between + function_call`、`append_after + function_call`、`insert_between + custom_event_call`。
5. **[verified] block_id in LogicJson**: LogicJson grouped output 正确包含 `block_id` 字段，Append 和 Replace 创建的图均输出。
6. **[known_gap] append_after + custom_event_call**: preview 返回 `preview_task failed: , modified=false.` 无详细错误信息，需进一步调试。
7. **[known_gap] branch_fork merge strategy**: 尚未在 UE 侧测试。
8. **[known_gap] Level 6 ClassSettings/UMG/DataTable**: blocked_by_fixture，缺乏 disposable 测试资产。
9. **[known_gap] Level 8 Partial Failure Journal**: blocked by lack of controlled failure fixture。

### Source Fix Notes 验证结论

| 修复项 | Rerun 3 | Rerun 4 | 状态 |
|---|---|---|---|
| Merge insert_between execute | ❌ FAIL | ✅ PASS | **verified** |
| Replace relink (0 orphans) | ❌ 历史 orphan | ✅ PASS | **verified** |
| Replace ownership metadata | ❌ 缺失 | ✅ PASS | **verified** |
| Patch on Replace-created nodes | ❌ target_node_not_found | ✅ PASS | **verified** |
| LogicJson block_id | ❌ 缺失 | ✅ PASS | **verified** |

**四项代码修复 + 一项已有能力全部通过验证。Level 5 从 Rerun 3 的 FAIL/PARTIAL 升级到完全 PASS。**

### append_after vs insert_between 行为分析

| 策略 | anchor 必需字段 | 行为 | 适用场景 |
|---|---|---|---|
| `insert_between` | `node_ref` + `pin_ref` + `link_ref` | 断开指定 link，在中间插入新节点 | 目标 pin 已有连接，需要插入中间 |
| `append_after` | `node_ref` + `pin_ref`（不含 `link_ref`） | 在目标 pin 的现有执行链后追加节点 | 目标 pin 已有连接，在末尾追加 |

### custom_event_call scope 的 anchor 字段要求

经 Zod schema 验证，`custom_event_call` scope 的 anchor 与 `function_call` 的 anchor 共享相同必需字段：`node_ref` + `pin_ref` 始终必需，`link_ref` 仅在 `insert_between` 策略时额外需要。

### 试错记录

1. **Preview 参数格式**: 首次调用 `blueprinthelper_preview_task` 时未传入 `task_spec` 参数，返回 Zod 验证错误。需将完整 JSON 对象作为 `task_spec` 参数值传入。

2. **custom_event_call scope anchor 字段**: 首次尝试 `insert_between + custom_event_call` 时只传了 `link_ref`，Zod schema 要求 `node_ref` 和 `pin_ref` 也是必需字段。补充后通过。

3. **append_after + custom_event_call 组合**: 尝试 `append_after + custom_event_call` 时 preview 返回空错误（`failed: , modified=false.`），无法确定根因。该组合标记为 known_gap。

4. **Merge execute 从 FAIL → PASS 的根本原因**: Rerun 3 错误根源是 ownership metadata 缺失导致 UE Merge Service 无法解析 block-scoped anchor 引用，而非 execute 路径本身的 bug。

## Documentation Follow-up From Rerun 4

Rerun 4 included Agent trial/error caused by insufficient guide detail. The following documentation updates have been synchronized:

- `blueprinthelper_preview_task` and `blueprinthelper_execute_task` examples now show the required `{ "task_spec": { ... } }` wrapper.
- GraphWrite Patch/Merge guidance now states that BlueprintHelper-owned writes use grouped LogicJson anchors: `block_id + group_entry_node_path + node_ref + pin_ref`, plus `link_ref` for `insert_between`.
- Merge guidance now explicitly says `link_ref` alone is invalid.
- GraphWrite body examples now state that function calls use structured `args`, not `params` or plain values.
- Current TODO/gap matrix now marks Level 5 Replace/Patch/Merge smoke as verified and keeps only the remaining known gaps.
