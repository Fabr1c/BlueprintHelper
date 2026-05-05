# BlueprintHelper TaskSpec / TaskPlan Contract (2026-05-04)

## 1. Contract Status

This file is the human-readable contract for the TaskSpec-first implementation slice.

The machine-readable guard is:

```text
BlueprintHelper_MCP_Server/src/task-contract.ts
BlueprintHelper_MCP_Server/src/task-contract.test.ts
BlueprintHelper_MCP_Server/src/task-protocol.fixtures.ts
```

The supported chain is:

```text
Agent -> TaskSpec semantic task -> MCP Task Tools -> Python/MCP Task Compiler -> TaskPlan structured edit language / IR -> Bridge task-level preview/execute -> UE Task Runtime lowering -> Existing UE capability clusters / Bridge commands
```

Ordinary Agents submit `BlueprintHelper.TaskSpec.v1` only. They do not author `BlueprintHelper.TaskPlan.v1`. The MCP/Python Task Compiler owns TaskPlan generation, and UE Task Runtime executes the lowered adapter work derived from that IR.

## 2. Versioned Schemas

| Data | Schema |
|---|---|
| Agent-facing MCP envelope | `BlueprintHelper.McpToolResult.v1` |
| Task context | `BlueprintHelper.TaskContextPack.v1` |
| Read input | `BlueprintHelper.ReadSpec.v1` |
| Read context payload | `ReadContextPack.v1` |
| Reference context payload | `ReferenceContextPack.v1` |
| Agent input | `BlueprintHelper.TaskSpec.v1` |
| Compiler output | `BlueprintHelper.TaskPlan.v1` |
| Preview payload | `TaskPreviewResult.v1` |
| Execute summary payload | `TaskRunSummary.v1` |
| Runtime result | `BlueprintHelper.TaskRunJournal.v1` |
| Task error | `BlueprintHelper.TaskError.v1` |
| Contract metadata | `BlueprintHelper.TaskProtocolContract.v1` |

## 3. Ownership

| Owner | Writes | Reads |
|---|---|---|
| Agent | `BlueprintHelper.TaskSpec.v1`, `BlueprintHelper.ReadSpec.v1` | AgentGuide index, ReadContextPack, TaskContextPack, preview summary, task result |
| MCP/Python Read Router | ReadContextPack / LogicMD / LogicJson views | ReadSpec |
| MCP/Python Task Compiler | `BlueprintHelper.TaskPlan.v1` | TaskSpec |
| UE Task Runtime | `BlueprintHelper.TaskRunJournal.v1` | TaskPlan |
| Existing Capability Clusters | Bridge/UE operation result facts | TaskPlan step args |

Rules:

- Agent must not submit a TaskPlan.
- MCP task tools must not expose low-level atomic tool planning as the default Agent workflow.
- UE Task Runtime must treat TaskPlan as the internal execution contract.
- Existing Bridge/UE operation results remain internal facts and are normalized into task-level results.
- Read tools should not grow one MCP tool per UE asset domain. New read capability domains should enter through `BlueprintHelper.ReadSpec.v1` and a read router, while write capability domains enter through `BlueprintHelper.TaskSpec.v1` and TaskPlan.
- TaskSpec keeps a small semantic top-level surface. A single semantic TaskSpec may combine multiple capability clusters through compiler decomposition, but those clusters must not become new default Agent-facing atomic tools.
- Preview may expose a TaskPlan summary and debug/expert details, but ordinary Agents do not depend on full TaskPlan payloads or adapter payloads.
- TaskRuntime execution policy is: dry-run all executable steps first, execute steps sequentially only after dry-run passes, record partial failure in TaskRunJournal if execution fails mid-run, and block dependent downstream steps by TaskPlan topology. It does not promise global rollback by default.

## 4. Agent-Facing Tools

Default Agent-facing tools are:

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

Low-level tools remain available only as internal, debug, expert, or test entries while their TaskSpec / ReadSpec replacements are not complete.

Legacy MCP tool removal policy:

- If a capability already has a TaskPlan adapter and TaskSpec compiler coverage, remove or hide the corresponding old Agent-facing atomic MCP write tools first.
- If a capability does not yet have adapter + TaskSpec coverage, keep its old MCP tools as legacy/internal/debug/expert entries until that coverage lands, then remove them in the same implementation slice.
- Direct read tools follow the same rule after `blueprinthelper_read_context` covers their read domain.
- Do not add new Agent-facing atomic MCP tools for newly expanded UE capability clusters.

## 4.1 Result Envelope Layering

Default Agent-facing result layering is fixed as:

| Tool | Outer shape | `data.schema` / payload |
|---|---|---|
| `blueprinthelper_read_agent_guide` | Markdown text only | none |
| `blueprinthelper_read_context` | `BlueprintHelper.McpToolResult.v1` | `ReadContextPack.v1`, with `payload.schema = LogicMd.v1 / LogicJson.v1 / ...` |
| `blueprinthelper_read_reference_context` | `BlueprintHelper.McpToolResult.v1` | `ReferenceContextPack.v1` |
| `blueprinthelper_preview_task` | `BlueprintHelper.McpToolResult.v1` | `TaskPreviewResult.v1` |
| `blueprinthelper_execute_task` | `BlueprintHelper.McpToolResult.v1` | `TaskRunSummary.v1` by default, or `TaskRunJournal.v1` when returning the full journal |
| `blueprinthelper_get_task_result` | `BlueprintHelper.McpToolResult.v1` | `TaskRunJournal.v1` |

UE Agent-facing façade commands return `FBlueprintHelperToolResultBase`. MCP task/read tools normalize those results into `BlueprintHelper.McpToolResult.v1` for Agent consumption. Existing Bridge / UE operation results are internal facts for compiler/runtime/journal use; ordinary Agents should not depend on raw adapter payloads.

Debug data must not expand the default top-level shape. Put compact debug facts under `data.debug` only when directly useful, and use `large_payload_ref` for large payloads.

Long-term read entry consolidation is:

```text
Agent -> ReadSpec -> MCP Read Tool -> Python/MCP Read Router -> UE Read Capability Cluster -> ReadContextPack / LogicMD / LogicJson
```

`blueprinthelper_read_agent_guide` returns the AgentGuide onboarding index document. Agents use that index to discover the currently documented capability surface and then open the specific AgentGuide files for concrete ReadSpec / TaskSpec formats. This is a documentation entry point, not a runtime capability-schema tool.

`blueprinthelper_read_context` is the generic read entry for asset-domain reads. `blueprinthelper_read_task_context` is deprecated until a clearer role is redefined; new read capability should enter through ReadSpec.

`blueprinthelper_read_reference_context` remains an independent Agent-facing read tool. It is a generic reference viewer for asset, variable, function, graph, widget, data table row, and other reference scopes. Its implementation may internally compose multiple reference analyzers, but the Agent sees one reference context envelope.

Editor lifecycle tools should use the `blueprinthelper_*` prefix. `blueprint_undo` and `blueprint_redo` are not part of the default future surface; recovery should move to transaction-level undo/redo or task journal replay instead of global editor undo/redo.

ReadSpec uses common view formats across read domains:

| Format | Purpose |
|---|---|
| `logic_md` | Default low-token human-readable view. |
| `logic_json` | Structured view for precise analysis, diff, patch, merge, and debug. |
| `summary` | Very small overview for large assets or discovery. |
| `schema` | Return field/schema guidance for the selected `read_type` and format without reading asset content. This is a ReadSpec view format, not a JSON Schema dialect commitment. |
| `raw_json` | Debug/expert full-fidelity view; not a default Agent workflow. |

`logic_md` and `logic_json` are not Blueprint-only formats. Future material, animation blueprint, widget, data table, and data asset reads should use the same formats where practical, so the MCP surface does not expand one tool per read shape.

Initial ReadSpec shape:

```json
{
  "schema": "BlueprintHelper.ReadSpec.v1",
  "read_type": "blueprint_logic",
  "target": {
    "asset_path": "_",
    "asset_type": "_",
    "target_type": "graph",
    "target_name": "_",
    "block_id": "_"
  },
  "view": {
    "format": "logic_md",
    "max_items": 200
  },
  "context": {
    "context_id": "_",
    "task_run_id": "_"
  }
}
```

`target.target_name` is interpreted by `target.target_type`:

| `target_type` | `target_name` meaning |
|---|---|
| `graph` | Graph name |
| `function` | Function name |
| `event` | Event name |
| `custom_event` | Custom Event name |
| `component` | Component name |
| `member_variable` | Member variable name |
| `event_dispatcher` | Event dispatcher name |
| `widget` | Widget name |
| `data_table_row` | Row name |

`block_id` remains separate because it is an ownership/id target, not a display name.

Initial supported `read_type` values:

| `read_type` | Purpose |
|---|---|
| `asset_context` | Asset identity, class, parent class, path, and capability summary. |
| `blueprint_logic` | Blueprint logic by blueprint, graph, function, event, custom event, or block target. |
| `component_context` | Blueprint SCS component tree and component property summaries. |
| `variable_context` | Member variables, local variables, defaults, categories, and type summaries. |
| `graph_context` | Graphs, function graphs, macro graphs, and event dispatcher lists. |
| `widget_context` | WidgetTree and widget property summaries. |
| `data_table_context` | DataTable schema, rows, and row summaries. |
| `object_property_context` | UObject / DataAsset reflected property summaries. |

Future read domains such as material graphs and animation blueprints must enter through new `read_type` values instead of new MCP tools.

`summary` is for low-token discovery and first-pass asset inspection before choosing a more detailed read. `schema` is for field guidance for the selected `read_type` / format without reading the asset body. Neither format performs writes or returns TaskSpec drafts.

Read result `data.schema` follows the short-name payload rule. `ReadContextPack.v1` does not include a separate `read_id`, and read results do not carry a `diagnostics` array. Payload errors use the outer MCP/ToolResult error envelope; read completeness uses `truncated` and `large_payload_ref`.

```json
{
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_context",
  "status": "completed",
  "modified": false,
  "target": {
    "asset_path": "_",
    "asset_type": "_",
    "target_type": "graph",
    "target_name": "_",
    "block_id": "_"
  },
  "data": {
    "schema": "ReadContextPack.v1",
    "read_type": "blueprint_logic",
    "format": "logic_md",
    "scope": "target",
    "payload": {
      "schema": "LogicMd.v1"
    },
    "stats": {},
    "truncated": false,
    "large_payload_ref": null
  }
}
```

`data.schema` and nested payload schemas do not repeat the `BlueprintHelper.` prefix. This matches the existing short-name payload rule used by LogicMD, LogicJson, and other UE operation payloads.

## 5. Supported GraphWrite Slice

GraphWrite is Agent-facing only through semantic `TaskSpec.behavior.graph_strategy` values. Adapter operation names remain runtime lowering details.

| Field | Value |
|---|---|
| `task_type` | `edit_blueprint_graph` |
| `target.target_type` | `blueprint` |
| `behavior.graph_strategy` | `append_new_owned_graph`, `replace_owned_graph`, `patch_owned_graph`, `merge_owned_graph` |
| `behavior.entries[].entry_type` | `custom_event` |
| `behavior.entries[].body.statements[].kind` | `call_function`, `set_member_variable` |
| `TaskPlan.steps[].capability` | `graph_write` |
| `TaskPlan.steps[].write.strategy` | `owned_graph_edit` |
| `TaskPlan.steps[].write.ops[].op` | `ensure_entry`, `replace_body`, `set_pin_default`, `set_node_comment`, `set_node_position`, `insert_flow` |
| `TaskPlan.steps[].constraints.allow_modify_user_nodes` | `false` |
| Step batching | append entries may share one step; replace/patch/merge compile to one structural op per step |

Runtime lowering targets `append_blueprint_graph`, `replace_blueprint_graph`, `patch_blueprint_graph`, or `merge_blueprint_graph` depending on the structural op. These are Runtime adapter targets only, not the primary Agent-authored contract.

Current implementation note, 2026-05-05 smoke rerun:

```text
Contract/compiler coverage: append_new_owned_graph, replace_owned_graph, patch_owned_graph, merge_owned_graph.
Smoke-verified Bridge execute coverage: append_new_owned_graph only, and only with a fresh graph name.
Replace/Patch/Merge remain TaskPlan IR contracts until the Bridge/UE blockers are fixed.
```

Ordinary Agents should default to `append_new_owned_graph` with a new BlueprintHelper-owned graph for GraphWrite writes. If preview blocks `replace_owned_graph`, `patch_owned_graph`, or `merge_owned_graph`, stop and report instead of attempting a lower-level write tool fallback.

The second executable slice is Blueprint Variables:

| Field | Supported value |
|---|---|
| `TaskSpec.task_type` | `edit_blueprint_variables` |
| `TaskSpec.behavior.variable_strategy` | `member_variables`, `member_defaults`, `local_variables` |
| `TaskSpec.behavior.changes[].kind` | `ensure_member_variable`, `configure_member_variable`, `remove_member_variable`, `ensure_local_variable`, `configure_local_variable`, `remove_local_variable` |
| `TaskSpec.behavior.defaults[]` | member default changes |
| `TaskPlan.steps[].capability` | `blueprint_variable` |
| `TaskPlan.steps[].write.strategy` | `member_variables`, `member_defaults`, `local_variables` |

Current Blueprint Variables lowering targets `add_blueprint_member_variables` for ensure-only member batches, or `blueprint_variable_batch` for mixed member/default/local ops. Preview uses the service dry-run path where implemented.

The first composite slice is Blueprint Feature composition:

| Field | Supported value |
|---|---|
| `TaskSpec.task_type` | `create_blueprint_feature` |
| `TaskSpec.components[]` | Component ensure plus optional property settings |
| `TaskSpec.variables[]` | Member variable ensure plus optional member defaults |
| `TaskSpec.class_settings.implemented_interfaces[]` | Interface ensure via class settings |
| `TaskSpec.behavior` | Same GraphWrite semantic object as `edit_blueprint_graph` |
| `TaskSpec.integration.interface` | Ensure interface implementation entry and replace its function body |
| `TaskPlan.steps[].capability` | `blueprint_component`, `blueprint_variable`, `blueprint_class_settings`, `blueprint_signature`, `graph_write` |

`create_blueprint_feature` is not a new UE mega-tool. It is a compiler-owned decomposition layer: Agent writes one semantic feature TaskSpec, Python/MCP compiler emits multiple existing capability steps, and UE Task Runtime lowers each step through the existing clusters. The current executable slice supports `integration.interface` by lowering it to class settings, function signature, and GraphWrite function-body steps. It rejects `integration.input` as an explicit out-of-scope area, matching the current UE-side capability boundary, and still rejects `scope_policy.allow_create_assets=true` so asset creation is not silently skipped.

Broader function/event signature management, DataAsset/ObjectProperty, Cleanup/Ownership, and large debug payload export remain future contract extensions. Input mapping integration is intentionally cut from the current roadmap until it is rechartered as a separate capability area.

## 6. TaskSpec Required Fields

Agent must provide these fields for GraphWrite:

```text
schema
task_type
target.asset_path
scope_policy.graph_name
scope_policy.allow_modify_user_nodes
behavior.graph_strategy
behavior.entries[] for append_new_owned_graph
behavior.replace for replace_owned_graph
behavior.patches[] for patch_owned_graph
behavior.merges[] for merge_owned_graph
validation.should_compile
validation.should_save
```

Agent may provide:

```text
context_id
feature_name
target.target_type
execution_policy.dry_run_mode
execution_policy.on_missing_capability
```

Agent must not provide:

```text
keys named compile or save inside validation
```

For composite Blueprint feature creation, Agent must provide:

```text
schema = BlueprintHelper.TaskSpec.v1
task_type = create_blueprint_feature
target.asset_path
at least one of components[], variables[], class_settings, behavior
validation.should_compile
validation.should_save
```

When `behavior` is present, Agent must also provide `scope_policy.graph_name`. The current composite slice supports only `integration.interface`; `integration.input` is rejected and should not be modeled in TaskSpec for this phase. `scope_policy.allow_create_assets=true` must be split into later TaskSpecs after the relevant asset creation capability is explicitly enabled.

## 7. TaskSpec Example

```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "context_id": "ctx_example",
  "task_type": "edit_blueprint_graph",
  "feature_name": "StoneGateActivation",
  "target": {
    "asset_path": "/Game/Blueprints/BP_StoneGate",
    "target_type": "blueprint"
  },
  "scope_policy": {
    "graph_name": "BH_StoneGateActivation",
    "allow_modify_user_nodes": false
  },
  "behavior": {
    "graph_strategy": "append_new_owned_graph",
    "entries": [
      {
        "entry_type": "custom_event",
        "name": "InitializeStoneGate",
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
                  "value": "Stone gate initialized"
                }
              }
            }
          ]
        }
      }
    ]
  },
  "integration": {
    "interface": {
      "interface_asset": "/Game/BlueprintHelperTest/Interaction/BPI_BH_Interactable",
      "function": "Interact",
      "implementation": {
        "call": "OpenPhysicsDoor"
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

Composite physical door core example:

```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "context_id": "ctx_physics_door",
  "task_type": "create_blueprint_feature",
  "feature_name": "PhysicsDoor",
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "blueprint"
  },
  "scope_policy": {
    "prefer_new_graph": true,
    "graph_name": "EG_PhysicsDoor",
    "allow_modify_user_nodes": false,
    "allow_create_assets": false
  },
  "asset_policy": {
    "if_target_asset_missing": "fail",
    "if_referenced_asset_missing": "fail",
    "if_component_exists": "reuse_if_type_matches"
  },
  "resources": {
    "static_meshes": {
      "door_mesh": "/Game/BlueprintHelperTest/Meshes/SM_Door"
    }
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
        "StaticMesh": "$resources.static_meshes.door_mesh",
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
      "default": 50000.0,
      "category": "Door"
    }
  ],
  "class_settings": {
    "implemented_interfaces": [
      "/Game/BlueprintHelperTest/Interaction/BPI_BH_Interactable"
    ]
  },
  "behavior": {
    "graph_strategy": "append_new_owned_graph",
    "entries": [
      {
        "entry_type": "custom_event",
        "name": "OpenPhysicsDoor",
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
              "name": "DoorMesh.AddAngularImpulseInDegrees",
              "args": {
                "VelChange": {
                  "kind": "literal",
                  "value_type": "bool",
                  "value": true
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
```

## 8. TaskPlan Required Fields

TaskPlan is compiler-owned. For the first GraphWrite slice, the compiler must produce:

```text
schema
task_type
target_assets[]
execution_policy.dry_run_mode
execution_policy.should_compile
execution_policy.should_save
steps[].step_id
steps[].capability
steps[].target.asset_path
steps[].target.graph
steps[].write.strategy
steps[].write.ops[]
steps[].constraints.allow_modify_user_nodes
steps[].constraints.ownership_scope
steps[].depends_on
```

For a structured GraphWrite IR step, `steps[].operation` is forbidden. Adapter operation names such as `append_blueprint_graph` may appear only in runtime lowering details, internal child execution results, or TaskRunJournal facts.

`steps[].depends_on` is an optional array of prior `step_id` values. Omitted or empty means the step has no explicit upstream dependency. The compiler should emit dependencies when a later step consumes a capability, entry, function, variable, component, asset, or graph produced by an earlier step.

The current TaskPlan is sent to Bridge task-level preview/execute as:

```json
{
  "task_plan": {
    "schema": "BlueprintHelper.TaskPlan.v1"
  }
}
```

Bridge task-level commands:

```text
preview_task_plan
execute_task_plan
get_task_run_journal
```

Runtime adapter payloads such as `append_blueprint_graph` remain lowering targets and may appear in internal child execution details or journal facts, but they are not the primary TaskPlan example contract.

## 9. TaskPlan Example

```json
{
  "schema": "BlueprintHelper.TaskPlan.v1",
  "task_name": "StoneGateActivation",
  "task_type": "edit_blueprint_graph",
  "context_id": "ctx_example",
  "target_assets": ["/Game/Blueprints/BP_StoneGate"],
  "execution_policy": {
    "dry_run_mode": "full",
    "should_compile": true,
    "should_save": false
  },
  "steps": [
    {
      "step_id": "step_001",
      "capability": "graph_write",
      "target": {
        "asset_path": "/Game/Blueprints/BP_StoneGate",
        "graph": "BH_StoneGateActivation"
      },
      "write": {
        "strategy": "owned_graph_edit",
        "ops": [
          {
            "op": "ensure_entry",
            "entry_type": "custom_event",
            "name": "InitializeStoneGate"
          }
        ]
      },
      "constraints": {
        "allow_modify_user_nodes": false,
        "ownership_scope": "blueprinthelper_owned"
      }
    }
  ]
}
```

When UE Task Runtime lowers this GraphWrite IR to an existing command cluster, the `custom_event` `ensure_entry` lowers to `append_blueprint_graph` with generated nodes and links. That adapter payload is runtime-internal and is not the primary Agent-facing TaskPlan example. Replace/Patch/Merge TaskSpecs produce separate `graph_write` steps with one structural op each, for example `replace_body`, `set_pin_default`, or `insert_flow`.

As of the 2026-05-05 smoke rerun, those Replace/Patch/Merge steps are contract/compiler-ready but Bridge/UE execution is still blocked. They must be treated as preview-first experimental paths until the blocker is cleared.

## 9.1 Partial Failure And Topology Blocking

TaskRuntime execution policy is:

1. Dry-run all executable steps first.
2. If any dry-run blocks, do not execute writes.
3. If dry-run passes, execute steps in stable TaskPlan order.
4. If a step fails during execution, mark that step failed and mark downstream dependent steps blocked by topology.
5. Continue only steps whose dependencies are completed and not blocked.
6. Do not promise global rollback by default.

TaskPlan dependency field:

```json
{
  "step_id": "step_020",
  "depends_on": ["step_010"]
}
```

TaskRunJournal step status values are:

```text
completed
failed
blocked
skipped
```

`blocked` means a prior dependency failed or was blocked. `skipped` means the runtime intentionally did not run the step for a non-error policy reason.

Blocked step journal fields:

```json
{
  "step_id": "step_020",
  "status": "blocked",
  "blocked_by_step_ids": ["step_010"],
  "blocked_reason": "dependency_failed",
  "error": null
}
```

Partial failure recovery summary:

```json
{
  "status": "partial_failure",
  "recovery": {
    "recommended_action": "inspect_task_result_then_submit_followup_taskspec",
    "safe_to_retry": false,
    "rollback_available": false,
    "notes": []
  }
}
```

The recovery object is user-readable guidance, not an automatic rollback command. Transaction-level undo/redo/replay is a separate future contract.

## 10. Validation Policy

Validation policy is deliberately named the same way across TaskSpec and TaskPlan:

| Layer | Fields |
|---|---|
| TaskSpec | `validation.should_compile`, `validation.should_save` |
| TaskPlan | `execution_policy.should_compile`, `execution_policy.should_save` |
| UE Runtime | `FBlueprintHelperValidationSummary.bShouldCompile`, `bShouldSave` |

Legacy `compile` and `save` keys inside `validation` are rejected. They are not aliases.

## 11. Capability Catalog

The capability field source for TaskPlan expansion is:

```text
Resources/v0.3.6/DoneImplementaion
```

The directory name is intentionally kept as-is because it is the current repository path.

Current UE TaskRuntime GraphWrite contract covers the operations listed below. The Agent-authored TaskSpec first slice may still gate which strategies the MCP/Python Task Compiler emits; this catalog is not permission for ordinary Agents to call low-level tools directly or author TaskPlan by hand.

### 11.0 GraphWrite TaskPlan IR And Lowering Adapter

`BlueprintHelper.TaskPlan.v1` is a compiler-owned structured edit language / IR. Ordinary Agents submit `BlueprintHelper.TaskSpec.v1`; they must not author TaskPlan directly.

The primary GraphWrite TaskPlan IR uses stable structural fields:

| Field | Meaning |
|---|---|
| `step_id` | Stable step id for diagnostics and journal references |
| `capability` | `graph_write` |
| `target.asset_path` | Target Blueprint asset |
| `target.graph` | Target graph or graph family |
| `write.strategy` | Structural write strategy such as `owned_graph_edit` |
| `write.ops[]` | Ordered structural edit ops |
| `constraints.allow_modify_user_nodes` | Whether user-owned graph nodes may be modified |
| `constraints.ownership_scope` | Ownership boundary, for example `blueprinthelper_owned` |

Initial GraphWrite IR ops include:

```text
ensure_entry
replace_body
set_pin_default
set_node_comment
set_node_position
insert_flow
```

The compiler/runtime lowering contract currently includes:

```text
ensure_entry(custom_event) -> append_blueprint_graph
replace_body -> replace_blueprint_graph
set_pin_default / set_node_comment / set_node_position -> patch_blueprint_graph
insert_flow -> merge_blueprint_graph
```

GraphWrite replace/patch/merge structural ops are emitted by the compiler as `capability/write/ops` TaskPlan IR. The runtime-owned adapter operation names may appear only in child results, runtime data, or TaskRunJournal facts.

Current smoke-verified Bridge execution is narrower than the contract: `ensure_entry(custom_event)` can execute through `append_blueprint_graph` only for `append_new_owned_graph` with a fresh graph name. Replace/Patch/Merge lowering must remain blocked or reported when preview/execute cannot reach the UE command successfully.

The existing UE GraphWrite commands are Runtime lowering adapter targets, not the primary TaskPlan abstraction:

| Operation | Required target fields | Required args fields | Optional args fields | Bridge dry-run placement |
|---|---|---|---|---|
| `append_blueprint_graph` | `asset_path`, `graph` | `nodes`, `links` | `feature_name` | root `dry_run` |
| `replace_blueprint_graph` | `asset_path`, `graph`, `replace_scope` | `selector`, `replacement.nodes`, `replacement.links` | `options.strict`, `options.preserve_layout` | `options.dry_run` |
| `patch_blueprint_graph` | `asset_path`, `graph`, `patch_scope` | `patch_type`, `patched_ref`, `patch` | `expected_old_state` | root `dry_run` |
| `merge_blueprint_graph` | `asset_path`, `graph`, `merge_scope`, `insert_strategy` | `anchor`, `inserted` | `sequence_order` | root `dry_run` |

These operations are TaskRuntime / Existing Capability Cluster steps. They do not expand the default Agent tool surface.

### 11.1 Task Runtime Clusters

| Cluster | Runtime adapters / internal operations | v0.3.6 source documents | Agent exposure |
|---|---|---|---|
| Graph Write | `append_blueprint_graph`, `replace_blueprint_graph`, `patch_blueprint_graph`, `merge_blueprint_graph` | `BlueprintHelper_AppendBlueprintGraph_UE_CPP_Implementation_Plan_20260503.md`, `BlueprintHelper_ReplaceBlueprintGraph_UE_CPP_Implementation_Plan_20260503.md`, `BlueprintHelper_PatchBlueprintGraph_UE_CPP_Implementation_Plan_20260503.md`, `BlueprintHelper_MergeBlueprintGraph_UE_CPP_Implementation_Plan_20260503.md` | TaskSpec only |
| Graph Cleanup / Ownership | `cleanup_blueprinthelper_block`, `convert_block_to_user_owned`, `rollback_cleanup_transaction` | `BlueprintHelper_CleanupBlueprintHelperBlock_UE_CPP_Implementation_Plan_20260503.md`, `BlueprintHelper_ConvertBlockToUserOwned_UE_CPP_Implementation_Plan_20260503.md`, `BlueprintHelper_RollbackCleanupTransaction_UE_CPP_Implementation_Plan_20260503.md` | TaskPlan internal |
| Blueprint Variables | `add_blueprint_member_variables` runtime adapter for `ensure_member_variable`; wider read/default/local/remove commands remain internal until dry-run contracts are fixed | `BlueprintHelper_BlueprintVariables_Defaults_LocalVariables_UE_CPP_Implementation_Plan_20260503.md` | TaskSpec only for `ensure_member_variable`, otherwise TaskPlan internal |
| Function/Event Signature | `ensure_function` runtime adapter for composite interface implementation; broader read/add/set/remove function/event signatures remain future work | `BlueprintHelper_FunctionEventSignature_UE_CPP_Implementation_Plan_20260503.md` | TaskPlan internal |
| UMG Widget Blueprint | `read_widget_blueprint`, `set_widget_tree`, `set_widget_properties` | `BlueprintHelper_UMG_WidgetBlueprint_UE_CPP_Implementation_Plan_20260503.md` | TaskPlan internal |
| DataAsset | `read_data_asset`, `set_data_asset_properties` | `BlueprintHelper_DataAsset_UE_CPP_Implementation_Plan_20260503.md` | TaskPlan internal |
| DataTable | `read_data_table`, `update_data_table_rows` | `BlueprintHelper_DataTable_UE_CPP_Implementation_Plan_20260503.md` | TaskPlan internal |
| Compile / Save | `compile_blueprint_asset`, `save_asset` | `BlueprintHelper_CompileBlueprintAsset_UE_CPP_Implementation_Plan_20260503.md`, `BlueprintHelper_SaveAsset_UE_CPP_Implementation_Plan_20260503.md` | TaskPlan internal |

Custom Event entry declaration is a Function/Event Signature responsibility at the TaskPlan / UE service boundary. GraphWrite append semantics may still use `ensure_entry` with `entry_type = "custom_event"` as structured GraphWrite IR, but the entry declaration / signature creation must be lowered through `blueprint_signature` or an internal BlueprintSignatureService call before GraphWrite writes the body.

Do not introduce a new Agent-facing custom-event atomic tool. `blueprint_signature.ensure_custom_event` is allowed as TaskPlan-internal IR, and `append_blueprint_graph` may internally delegate Custom Event entry creation to the signature service. Ordinary Agents still express the intent through TaskSpec semantics such as `append_new_owned_graph` or `create_blueprint_feature`.

Function/Event Signature expansion must cover creation, modification, and removal of function signatures, Custom Event signatures, interface function versus interface event entries, event dispatcher signatures, and override/native event entries. Function or event body logic, graph nodes, links, calls, binds, and unbinds remain GraphWrite responsibility.

### 11.2 Support Clusters

| Cluster | Purpose | v0.3.6 source documents | Agent exposure |
|---|---|---|---|
| Runtime Profile | Safety/profile facts used before TaskSpec and execution | `BlueprintHelper_RuntimeProfile_UE_CPP_Implementation_Plan_20260503.md` | Agent read |
| Logic Read | LogicMD / LogicJson by asset, graph, function, event, custom event, or block target | `BlueprintHelper_LogicRead_Grouped_UE_FieldMapping_20260502.md` | Agent read |
| Diagnostics / Discovery | Asset discovery, editor navigation, debug export, internal dependency analysis, project context, setup state | `BlueprintHelper_Diagnostics_UE_CPP_Implementation_Plan_20260503.md`, `BlueprintHelper_AssetDiscovery_EditorNavigation_UE_CPP_Implementation_Plan_20260503.md`, `BlueprintHelper_DebugExport_LargePayload_UE_CPP_Implementation_Plan_20260503.md`, `BlueprintHelper_InternalDependencyAnalysis_UE_ImplementationPlan_20260503.md`, `BlueprintHelper_ProjectContext_SetupState_UE_CPP_Implementation_Plan_20260503.md` | Agent read or debug |
| Transaction Journal | Task result, transaction query, rollback audit | `BlueprintHelper_TransactionJournalQuery_UE_CPP_Implementation_Plan_20260503.md` | Task result or debug |
| Editor Lifecycle | Risk commands for launching or closing editor | `BlueprintHelper_EditorLifecycle_RiskCommand_UE_CPP_Implementation_Plan_20260503.md` | Debug or risk command |
| Common Envelope | Shared result shape and error normalization | `BlueprintHelper_ToolResultBase_CommonEnvelope_UE_CPP_Implementation_Plan_20260503.md` | Protocol internal |

LogicMD keeps the v0.3.6 grouped logic information shape. It is a read-only human-readable logic view and must not carry TaskSpec drafts. LogicJson remains a read-only structured logic view and must not carry `taskspec_hints`. LogicJson `node_ref` / `link_ref` values are local read-view references and are not automatically TaskSpec-compatible write anchors unless a later contract explicitly maps them.

Known current bug, 2026-05-05: LogicJson `target_type=custom_event` lookup searches only EventGraph and ignores custom graphs. Until fixed, read custom graph context by graph target first or use LogicMD for verification.

## 12. Extension Policy

Any new TaskSpec or TaskPlan capability must update all of these together:

1. `BlueprintHelper_MCP_Server/src/task-contract.ts`
2. `BlueprintHelper_MCP_Server/src/task-contract.test.ts`
3. `BlueprintHelper_MCP_Server/src/task-protocol.fixtures.ts`
4. `BlueprintHelper_MCP_Server/python/blueprinthelper_task/*`
5. UE Task Runtime validation or execution code if the TaskPlan shape changes.
6. This contract document.

Do not silently add fields that alter semantics. Optional metadata may pass through schema validation, but new executable semantics require contract and fixture updates.
