# BlueprintHelper TaskSpec / TaskPlan Contract (2026-05-04)

## 1. Contract Status

This file is the human-readable contract for the first TaskSpec-first implementation slice.

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
| Task context | `BlueprintHelper.TaskContextPack.v1` |
| Agent input | `BlueprintHelper.TaskSpec.v1` |
| Compiler output | `BlueprintHelper.TaskPlan.v1` |
| Runtime result | `BlueprintHelper.TaskRunJournal.v1` |
| Task error | `BlueprintHelper.TaskError.v1` |
| Contract metadata | `BlueprintHelper.TaskProtocolContract.v1` |

## 3. Ownership

| Owner | Writes | Reads |
|---|---|---|
| Agent | `BlueprintHelper.TaskSpec.v1` | TaskContextPack, preview summary, task result |
| MCP/Python Task Compiler | `BlueprintHelper.TaskPlan.v1` | TaskSpec |
| UE Task Runtime | `BlueprintHelper.TaskRunJournal.v1` | TaskPlan |
| Existing Capability Clusters | Bridge/UE operation result facts | TaskPlan step args |

Rules:

- Agent must not submit a TaskPlan.
- MCP task tools must not expose low-level atomic tool planning as the default Agent workflow.
- UE Task Runtime must treat TaskPlan as the internal execution contract.
- Existing Bridge/UE operation results remain internal facts and are normalized into task-level results.

## 4. Agent-Facing Tools

Default Agent-facing tools are:

```text
blueprinthelper_get_runtime_profile
blueprinthelper_diagnostics
blueprinthelper_read_task_context
blueprinthelper_preview_task
blueprinthelper_execute_task
blueprinthelper_get_task_result
```

Low-level tools remain available as internal, debug, expert, or test entries.

## 5. First Supported Slice

Only this slice is fixed in the current contract:

| Field | Value |
|---|---|
| `task_type` | `edit_blueprint_graph` |
| `target.target_type` | `blueprint` |
| `behavior.graph_strategy` | `append_new_owned_graph` |
| `behavior.entries[].entry_type` | `custom_event` |
| `behavior.entries[].body.statements[].kind` | `call_function`, `set_member_variable` |
| `TaskPlan.steps[].capability` | `graph_write` |
| `TaskPlan.steps[].write.strategy` | `owned_graph_edit` |
| `TaskPlan.steps[].write.ops[0].op` | `ensure_entry` |
| `TaskPlan.steps[].constraints.allow_modify_user_nodes` | `false` |
| Max TaskPlan steps | `1` |

Current runtime lowering for this slice typically targets `append_blueprint_graph`, but `append_blueprint_graph`, `replace_blueprint_graph`, `patch_blueprint_graph`, and `merge_blueprint_graph` are Runtime lowering adapter targets only, not the primary TaskPlan contract surface.

The second executable slice is Blueprint Variables:

| Field | Supported value |
|---|---|
| `TaskSpec.task_type` | `edit_blueprint_variables` |
| `TaskSpec.behavior.variable_strategy` | `member_variables` |
| `TaskSpec.behavior.variables[].op` | `ensure_member_variable` |
| `TaskPlan.steps[].capability` | `blueprint_variable` |
| `TaskPlan.steps[].write.strategy` | `member_variables` |
| `TaskPlan.steps[].write.ops[].op` | `ensure_member_variable` |

Current Blueprint Variables lowering targets `add_blueprint_member_variables` for execute. Preview does not call that mutating adapter because the existing UE variable service does not expose a true adapter dry-run; UE Task Runtime returns a synthetic task-level dry-run after validating the TaskPlan payload.

Replace, patch, merge, function signature management, event signature management, asset creation, UMG, DataAsset, DataTable, and multi-step runtime plans are future contract extensions.

## 6. TaskSpec Required Fields

Agent must provide these fields for the first slice:

```text
schema
task_type
target.asset_path
scope_policy.graph_name
scope_policy.allow_modify_user_nodes
behavior.graph_strategy
behavior.entries[].entry_type
behavior.entries[].name
behavior.entries[].body.schema
behavior.entries[].body.statements[]
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
```

For a structured GraphWrite IR step, `steps[].operation` is forbidden. Adapter operation names such as `append_blueprint_graph` may appear only in runtime lowering details, internal child execution results, or TaskRunJournal facts.

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
          },
          {
            "op": "replace_body",
            "entry_name": "InitializeStoneGate",
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
      "constraints": {
        "allow_modify_user_nodes": false,
        "ownership_scope": "blueprinthelper_owned"
      }
    }
  ]
}
```

When UE Task Runtime lowers this GraphWrite IR to an existing command cluster, the `custom_event` `ensure_entry` commonly lowers to `append_blueprint_graph` with generated nodes and links. That adapter payload is runtime-internal and is not the primary Agent-facing TaskPlan example.

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

Current UE TaskRuntime executable support covers the Graph Write cluster operations listed below. The Agent-authored TaskSpec first slice may still gate which strategies the MCP/Python Task Compiler emits; this catalog is not permission for ordinary Agents to call low-level tools directly or author TaskPlan by hand.

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

Current executable runtime lowering in the first slice supports `ensure_entry(custom_event)` only. The other structural ops are reserved contract space for later GraphWrite expansion and must not be emitted by the compiler until the corresponding UE lowering is implemented and tested.

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
| UMG Widget Blueprint | `read_widget_blueprint`, `set_widget_tree`, `set_widget_properties` | `BlueprintHelper_UMG_WidgetBlueprint_UE_CPP_Implementation_Plan_20260503.md` | TaskPlan internal |
| DataAsset | `read_data_asset`, `set_data_asset_properties` | `BlueprintHelper_DataAsset_UE_CPP_Implementation_Plan_20260503.md` | TaskPlan internal |
| DataTable | `read_data_table`, `update_data_table_rows` | `BlueprintHelper_DataTable_UE_CPP_Implementation_Plan_20260503.md` | TaskPlan internal |
| Compile / Save | `compile_blueprint_asset`, `save_asset` | `BlueprintHelper_CompileBlueprintAsset_UE_CPP_Implementation_Plan_20260503.md`, `BlueprintHelper_SaveAsset_UE_CPP_Implementation_Plan_20260503.md` | TaskPlan internal |

Adding a Custom Event is not a separate signature-management capability in the current architecture. It is expressed as GraphWrite IR `ensure_entry` with `entry_type = "custom_event"` and is lowered to `append_blueprint_graph` when append semantics are required. There is no separate Agent-facing custom-event mutation tool or event-listing tool in the default TaskSpec-first surface.

### 11.2 Support Clusters

| Cluster | Purpose | v0.3.6 source documents | Agent exposure |
|---|---|---|---|
| Runtime Profile | Safety/profile facts used before TaskSpec and execution | `BlueprintHelper_RuntimeProfile_UE_CPP_Implementation_Plan_20260503.md` | Agent read |
| Diagnostics / Discovery | Asset discovery, editor navigation, debug export, internal dependency analysis, project context, setup state | `BlueprintHelper_Diagnostics_UE_CPP_Implementation_Plan_20260503.md`, `BlueprintHelper_AssetDiscovery_EditorNavigation_UE_CPP_Implementation_Plan_20260503.md`, `BlueprintHelper_DebugExport_LargePayload_UE_CPP_Implementation_Plan_20260503.md`, `BlueprintHelper_InternalDependencyAnalysis_UE_ImplementationPlan_20260503.md`, `BlueprintHelper_ProjectContext_SetupState_UE_CPP_Implementation_Plan_20260503.md` | Agent read or debug |
| Transaction Journal | Task result, transaction query, rollback audit | `BlueprintHelper_TransactionJournalQuery_UE_CPP_Implementation_Plan_20260503.md` | Task result or debug |
| Editor Lifecycle | Risk commands for launching or closing editor | `BlueprintHelper_EditorLifecycle_RiskCommand_UE_CPP_Implementation_Plan_20260503.md` | Debug or risk command |
| Common Envelope | Shared result shape and error normalization | `BlueprintHelper_ToolResultBase_CommonEnvelope_UE_CPP_Implementation_Plan_20260503.md` | Protocol internal |

## 12. Extension Policy

Any new TaskSpec or TaskPlan capability must update all of these together:

1. `BlueprintHelper_MCP_Server/src/task-contract.ts`
2. `BlueprintHelper_MCP_Server/src/task-contract.test.ts`
3. `BlueprintHelper_MCP_Server/src/task-protocol.fixtures.ts`
4. `BlueprintHelper_MCP_Server/python/blueprinthelper_task/*`
5. UE Task Runtime validation or execution code if the TaskPlan shape changes.
6. This contract document.

Do not silently add fields that alter semantics. Optional metadata may pass through schema validation, but new executable semantics require contract and fixture updates.
