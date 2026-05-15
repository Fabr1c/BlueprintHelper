# BlueprintHelper New Project Full SmokeRun

> 2026-05-14 状态转移：本文中的未达期待、待验证项和阻塞项已迁移到 [BlueprintHelper_UnmetExpectations_Consolidated_20260514_CN.md](BlueprintHelper_UnmetExpectations_Consolidated_20260514_CN.md)。本文保留为历史上下文；开放项跟踪迁移完成，后续当前状态以总账为准。

Date: 2026-05-10

Purpose: run BlueprintHelper end to end in a clean Unreal project, using disposable assets, TaskSpec-first writes, UE Automation, ReviewPanel, and Debug/DebugBundle checks. This runbook is project-agnostic; replace all angle-bracket placeholders before running.

## 0. Run Variables

Use one unique run id and keep every disposable asset under one folder.

```text
ENGINE_DIR=<UE_ENGINE_DIR>
PROJECT_ROOT=<absolute path to new project>
PROJECT_FILE=<absolute path to NewProject.uproject>
PROJECT_NAME=<NewProject target name without .uproject>
PLUGIN_ROOT=<PROJECT_ROOT>/Plugins/BlueprintHelper
RUN_ID=BHSmoke_20260510_01
SMOKE_ROOT=/Game/BlueprintHelperSmoke/${RUN_ID}
REPORT_ROOT=<PROJECT_ROOT>/Saved/Automation/BlueprintHelperFullSmoke_${RUN_ID}
```

Canonical disposable assets:

```text
${SMOKE_ROOT}/BP_BHSmokeActor
${SMOKE_ROOT}/BPI_BHSmokeInteract
${SMOKE_ROOT}/ST_BHSmokeDamageRow
${SMOKE_ROOT}/DT_BHSmokeDamage
${SMOKE_ROOT}/WBP_BHSmokePanel
${SMOKE_ROOT}/DA_BHSmokeData
```

## 1. Pass/Fail Rules

- All normal writes must use TaskSpec-first: read context, preview, execute, get task result.
- Preview is the write gate. If preview is blocked, execute is forbidden unless the case is a negative test.
- Expected negative cases must return a non-empty `issues[]` with `code`, `path`, and `message`.
- Every successful execute must return a non-empty `task_run_id`; `blueprinthelper_get_task_result` must load the UE TaskRunJournal.
- `validation.should_compile` and `validation.should_save` must be explicit for every write.
- Compile validation is only for Blueprint-backed assets. `structure`, `data_table`, `data_asset` instances, `input_action`, `input_mapping_context`, and plain UObject property writes must use `validation.should_compile=false` and pass by read-back. A Blueprint class used as a DataAsset class is still `asset_type=blueprint_class` and must use `validation.should_compile=true`.
- `no_op` is acceptable for idempotent fixture creation when read-back proves the existing asset type and content match the requested fixture. Do not classify `no_op` on ST/DT/DA as a compile failure.
- MCP responses must not expose DebugBundle local paths, raw payloads, source content, tokens, settings, or `debug_export_refs`.
- ReviewRecord may store `debug_case_ids[]`; it must not inline DebugBundle payload or local bundle paths.
- A full pass requires no UE Automation failures, no MCP contract regression failures, no orphaned graph flow after read-back, and no path leak in Review/Debug summaries.

## 2. Baseline Build And Local Regression

Run from the plugin repo or copied project plugin folder. Treat the UE plugin and MCP server as separate build artifacts: Unreal `BuildPlugin` packages the UE plugin directory, but it does not compile or package the sibling `ClaudePlugin/mcp` server by itself.

Before the smoke, verify:

- UE side: the target project has rebuilt the BlueprintHelper editor module for the target engine/project.
- Packaged UE plugin side: `Resources/AgentGuide/...` exists in the package. `Config/FilterPlugin.ini` must package resources recursively.
- MCP side: `ClaudePlugin/mcp/build/index.js` exists and was produced from the current TypeScript sources.

```powershell
& "$env:ENGINE_DIR\Build\BatchFiles\Build.bat" `
  <PROJECT_NAME>Editor Win64 Development `
  -Project="<PROJECT_FILE>" `
  -WaitMutex -NoHotReload
```

Run MCP regression:

```powershell
Set-Location "<PLUGIN_ROOT>\ClaudePlugin\mcp"
npm.cmd run build
python -m unittest discover -s python/tests -t python
npm.cmd run test:node
```

If `npm.cmd run build` is blocked by existing `build/` permissions, use a separate output directory for diagnosis only:

```powershell
npx.cmd tsc --outDir build-smoke
node.exe --test build-smoke/task-tools.regression.test.js
```

Do not count the temporary-output run as a replacement for the normal build unless the permission blocker is recorded.

## 3. UE Automation Ring

Run each group as a separate Editor-Cmd process. Do not chain many `Automation RunTests` commands into one `ExecCmds`; some reports only capture the first queue.

```powershell
$Editor = "<ENGINE_DIR>\Binaries\Win64\UnrealEditor-Cmd.exe"
$Project = "<PROJECT_FILE>"
$Report = "<REPORT_ROOT>"

& $Editor $Project -Unattended -NullRHI -NoSplash -NoSound `
  -ExecCmds="Automation RunTests BlueprintHelper.ObjectFirst; Quit" `
  -TestExit="Automation Test Queue Empty" `
  -ReportOutputPath="$Report\ObjectFirst"

& $Editor $Project -Unattended -NullRHI -NoSplash -NoSound `
  -ExecCmds="Automation RunTests BlueprintHelper.AssetFactory; Quit" `
  -TestExit="Automation Test Queue Empty" `
  -ReportOutputPath="$Report\AssetFactory"

& $Editor $Project -Unattended -NullRHI -NoSplash -NoSound `
  -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite; Quit" `
  -TestExit="Automation Test Queue Empty" `
  -ReportOutputPath="$Report\GraphWrite"

& $Editor $Project -Unattended -NullRHI -NoSplash -NoSound `
  -ExecCmds="Automation RunTests BlueprintHelper.Signature; Quit" `
  -TestExit="Automation Test Queue Empty" `
  -ReportOutputPath="$Report\Signature"

& $Editor $Project -Unattended -NullRHI -NoSplash -NoSound `
  -ExecCmds="Automation RunTests BlueprintHelper.TaskPlan; Quit" `
  -TestExit="Automation Test Queue Empty" `
  -ReportOutputPath="$Report\TaskPlan"

& $Editor $Project -Unattended -NullRHI -NoSplash -NoSound `
  -ExecCmds="Automation RunTests BlueprintHelper.Review.UI; Quit" `
  -TestExit="Automation Test Queue Empty" `
  -ReportOutputPath="$Report\Review_UI"

& $Editor $Project -Unattended -NullRHI -NoSplash -NoSound `
  -ExecCmds="Automation RunTests BlueprintHelper.Review.Action; Quit" `
  -TestExit="Automation Test Queue Empty" `
  -ReportOutputPath="$Report\Review_Action"

& $Editor $Project -Unattended -NullRHI -NoSplash -NoSound `
  -ExecCmds="Automation RunTests BlueprintHelper.Review.Integration; Quit" `
  -TestExit="Automation Test Queue Empty" `
  -ReportOutputPath="$Report\Review_Integration"

& $Editor $Project -Unattended -NullRHI -NoSplash -NoSound `
  -ExecCmds="Automation RunTests BlueprintHelper.RuntimeDiagnostics.Debug; Quit" `
  -TestExit="Automation Test Queue Empty" `
  -ReportOutputPath="$Report\Debug"
```

Targeted must-pass names to confirm in reports:

```text
BlueprintHelper.GraphWrite.TaskRuntime.Merge.BranchForkOwnedBlockCallReadBack
BlueprintHelper.Review.UI.LoadPendingVisibleChangesUsesRecordQuery
BlueprintHelper.Review.Action.AcceptTargetsPersistsActionHistory
BlueprintHelper.Review.Action.RejectSucceedsWithMatchingHashAndRollbackData
BlueprintHelper.Review.Action.RejectAllPersistsToctouNeedsAction
BlueprintHelper.Review.Action.RejectAllIteratesPendingTargets
BlueprintHelper.Review.Integration.RejectNeedsActionCreatesDebugCase
BlueprintHelper.Review.Integration.RejectFailedCreatesDebugCase
BlueprintHelper.RuntimeDiagnostics.Debug.BundleSummaryExportIncludesReviewSummaryArtifact
BlueprintHelper.RuntimeDiagnostics.Debug.BundleSummaryExportRedactsSensitiveArtifacts
```

## 4. MCP Preflight Ring

In the new project, start the Editor or use `blueprint_open_editor` only as preflight.

Required MCP checks:

```text
blueprinthelper_read_agent_guide
blueprint_get_runtime_profile
blueprinthelper_diagnostics
blueprinthelper_diagnostics_runtime
```

Pass criteria:

- Bridge reachable.
- Runtime profile lists TaskSpec-first capability surface.
- If write permission is disabled because write session is missing, run a preview first, then call `blueprinthelper_request_write_session`, then execute.
- Do not pass or request raw Bridge tokens.

## 5. TaskSpec Execution Protocol

For every TaskSpec below:

1. Call `blueprinthelper_preview_task` with `{ "task_spec": { ... } }`.
2. Require `passed=true` and `blocked=false`, unless the step is explicitly negative.
3. Call `blueprinthelper_execute_task` with the same root shape.
4. Record `task_run_id`.
5. Call `blueprinthelper_get_task_result`.
6. Read back context where applicable.

Record every operation in this table:

| Step | Task type | Asset | Preview | Execute | task_run_id | Read-back evidence | Notes |
|---|---|---|---|---|---|---|---|
| A1 | create_asset | BP_BHSmokeActor | | | | | |

## 6. Fixture Creation Ring

### 6.1 Actor Blueprint

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1",
    "task_type": "create_asset",
    "feature_name": "CreateSmokeActor",
    "target": {
      "asset_path": "/Game/BlueprintHelperSmoke/<RUN_ID>/BP_BHSmokeActor",
      "target_type": "asset"
    },
    "behavior": {
      "asset_strategy": "ensure_asset",
      "asset": {
        "asset_type": "blueprint_class",
        "parent_class": "Actor",
        "collision_policy": "reuse_if_exists"
      }
    },
    "execution_policy": {
      "dry_run_mode": "full",
      "on_missing_capability": "stop_and_report"
    },
    "validation": {
      "should_compile": true,
      "should_save": true
    }
  }
}
```

### 6.2 Interface, Struct, DataTable, Widget, DataAsset

Run equivalent `create_asset` TaskSpecs. Compile only assets that have a Blueprint compile step:

| Asset | `asset_type` | Required extra fields | `should_compile` | Pass criteria |
|---|---|---|---:|---|
| `${SMOKE_ROOT}/BPI_BHSmokeInteract` | `blueprint_interface` | none | `true` | asset exists and compile succeeds |
| `${SMOKE_ROOT}/ST_BHSmokeDamageRow` | `structure` | `fields=[Damage:int, DisplayName:string]` | `false` | fields exist by read-back |
| `${SMOKE_ROOT}/DT_BHSmokeDamage` | `data_table` | `row_struct="${SMOKE_ROOT}/ST_BHSmokeDamageRow"` | `false` | DataTable uses row struct by read-back |
| `${SMOKE_ROOT}/WBP_BHSmokePanel` | `widget_blueprint` | `parent_class=UserWidget` | `true` | WidgetBlueprint exists and compile has no fatal error |
| `${SMOKE_ROOT}/BP_BHSmokeDataAssetClass` | `blueprint_class` | `parent_class=PrimaryDataAsset` | `true` | Blueprint class exists, parent is PrimaryDataAsset, compile succeeds |
| `${SMOKE_ROOT}/DA_BHSmokeData` | `data_asset` | `data_asset_class="${SMOKE_ROOT}/BP_BHSmokeDataAssetClass"` | `false` | DataAsset exists and its class is the generated class from `BP_BHSmokeDataAssetClass` |

For ST/DT/DA instance assets, do not request compile and do not fail the smoke for missing compile output. DataAsset instances must specify a concrete `UDataAsset` subclass through `data_asset_class`; in a new project smoke, create the PrimaryDataAsset Blueprint class first and compile that class. If the create step returns `no_op` because the fixture already exists, run read-back and only fail when the asset type or content is wrong.

## 7. Blueprint Capability Ring

### 7.1 Components

Target: `${SMOKE_ROOT}/BP_BHSmokeActor`

TaskSpec:

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1",
    "task_type": "edit_blueprint_components",
    "feature_name": "SmokeComponents",
    "target": {
      "asset_path": "/Game/BlueprintHelperSmoke/<RUN_ID>/BP_BHSmokeActor",
      "target_type": "blueprint"
    },
    "behavior": {
      "component_strategy": "component_tree",
      "changes": [
        {
          "kind": "ensure_component_present",
          "name": "SmokeScene",
          "class": "SceneComponent",
          "on_name_conflict": "reuse"
        },
        {
          "kind": "ensure_component_present",
          "name": "SmokeMesh",
          "class": "StaticMeshComponent",
          "attach": {
            "parent": "SmokeScene",
            "rule": "keep_relative"
          },
          "on_name_conflict": "reuse"
        }
      ]
    },
    "execution_policy": {
      "dry_run_mode": "full",
      "on_missing_capability": "stop_and_report"
    },
    "validation": {
      "should_compile": true,
      "should_save": true
    }
  }
}
```

Read-back: component tree contains `SmokeScene` and `SmokeMesh`.

### 7.2 Variables

Run `edit_blueprint_variables` on `BP_BHSmokeActor`:

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1",
    "task_type": "edit_blueprint_variables",
    "feature_name": "SmokeVariables",
    "target": {
      "asset_path": "/Game/BlueprintHelperSmoke/<RUN_ID>/BP_BHSmokeActor",
      "target_type": "blueprint"
    },
    "behavior": {
      "variable_strategy": "member_variables",
      "variables": [
        {
          "kind": "ensure_member_variable",
          "name": "SmokeHealth",
          "pin_type": {
            "category": "int"
          },
          "value": 100
        },
        {
          "kind": "ensure_member_variable",
          "name": "SmokeLabel",
          "pin_type": {
            "category": "string"
          },
          "value": "NewProjectSmoke"
        }
      ]
    },
    "execution_policy": {
      "dry_run_mode": "full",
      "on_missing_capability": "stop_and_report"
    },
    "validation": {
      "should_compile": true,
      "should_save": true
    }
  }
}
```

Read-back: member variables and defaults exist. If independent `edit_blueprint_variables` is blocked, record it as the known capability gap and require composite variable coverage in Ring 11.

### 7.3 Signature

Run `edit_blueprint_signature` on `BP_BHSmokeActor`:

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1",
    "task_type": "edit_blueprint_signature",
    "feature_name": "SmokeSignature",
    "target": {
      "asset_path": "/Game/BlueprintHelperSmoke/<RUN_ID>/BP_BHSmokeActor",
      "target_type": "blueprint"
    },
    "behavior": {
      "signature_strategy": "signature_edit",
      "changes": [
        {
          "kind": "ensure_function",
          "function_name": "BHSmoke_CalcDamage",
          "inputs": [
            { "name": "BaseDamage", "type": "int" }
          ],
          "outputs": [
            { "name": "Result", "type": "int" }
          ],
          "name_collision_policy": "reuse_if_exists"
        },
        {
          "kind": "ensure_custom_event",
          "event_name": "BHSmoke_OnInteract",
          "graph_name": "EventGraph",
          "inputs": [
            { "name": "InstigatorName", "type": "string" }
          ],
          "name_collision_policy": "reuse_if_exists"
        },
        {
          "kind": "ensure_event_dispatcher",
          "dispatcher_name": "BHSmoke_OnTriggered",
          "inputs": [
            { "name": "Amount", "type": "int" }
          ],
          "signature_mismatch_policy": "block"
        },
        {
          "kind": "ensure_override_event",
          "event_name": "ReceiveAnyDamage",
          "event_kind": "native_event",
          "execute_policy": "create_if_missing"
        }
      ]
    },
    "execution_policy": {
      "dry_run_mode": "full",
      "on_missing_capability": "stop_and_report"
    },
    "validation": {
      "should_compile": true,
      "should_save": true
    }
  }
}
```

Read-back: function graph, custom event, dispatcher, and native event entry are visible.

### 7.4 Class Settings

Run `edit_blueprint_class_settings`:

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1",
    "task_type": "edit_blueprint_class_settings",
    "feature_name": "SmokeClassSettings",
    "target": {
      "asset_path": "/Game/BlueprintHelperSmoke/<RUN_ID>/BP_BHSmokeActor",
      "target_type": "blueprint"
    },
    "behavior": {
      "class_settings_strategy": "class_settings",
      "interfaces": {
        "ensure_present": [
          "/Game/BlueprintHelperSmoke/<RUN_ID>/BPI_BHSmokeInteract"
        ]
      },
      "class_defaults": [
        {
          "kind": "set_object_property",
          "property_path": "bCanBeDamaged",
          "value": false
        }
      ]
    },
    "execution_policy": {
      "dry_run_mode": "full",
      "on_missing_capability": "stop_and_report"
    },
    "validation": {
      "should_compile": true,
      "should_save": true
    }
  }
}
```

Read-back: interface is implemented; `bCanBeDamaged=false`.

## 8. GraphWrite Ring

### 8.1 Append Owned Graph

Create an owned graph/custom event block:

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1",
    "task_type": "edit_blueprint_graph",
    "feature_name": "SmokeGraphAppend",
    "target": {
      "asset_path": "/Game/BlueprintHelperSmoke/<RUN_ID>/BP_BHSmokeActor",
      "target_type": "blueprint"
    },
    "scope_policy": {
      "graph_name": "EventGraph",
      "allow_modify_user_nodes": false
    },
    "behavior": {
      "graph_strategy": "append_new_owned_graph",
      "entries": [
        {
          "entry_type": "custom_event",
          "name": "BHSmoke_Anchor",
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
                    "value": "BHSmoke Anchor"
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
      "should_save": true
    }
  }
}
```

Read back LogicJson for `EventGraph` and record:

```text
ANCHOR_BLOCK_ID=<block id for BHSmoke_Anchor>
ANCHOR_GROUP_ENTRY_NODE_PATH=<group entry node path>
ANCHOR_NODE_REF=<entry node ref or selected exec node ref>
ANCHOR_PIN_REF=<exec output pin ref>
ANCHOR_LINK_REF=<successor link ref if present>
```

### 8.2 Replace Owned Graph

Replace the `BHSmoke_Anchor` custom event body with a new message. Pass criteria: the custom event remains, body is replaced, no orphaned nodes remain.

### 8.3 Patch Owned Graph

Patch at least one node comment and one pin default using refs from LogicJson. Pass criteria: read-back shows the new comment/default and the target remains BlueprintHelper-owned.

### 8.4 Merge Branch Fork Owned Block Call

First create a second owned custom event block:

```text
BHSmoke_Inserted
```

Read back its `INSERTED_BLOCK_ID`.

Then execute `merge_owned_graph` with `branch_fork`:

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1",
    "task_type": "edit_blueprint_graph",
    "feature_name": "SmokeGraphMergeBranchFork",
    "target": {
      "asset_path": "/Game/BlueprintHelperSmoke/<RUN_ID>/BP_BHSmokeActor",
      "target_type": "blueprint"
    },
    "scope_policy": {
      "graph_name": "EventGraph",
      "allow_modify_user_nodes": false
    },
    "behavior": {
      "graph_strategy": "merge_owned_graph",
      "merges": [
        {
          "kind": "insert_flow",
          "scope": "owned_block_call",
          "insert_strategy": "branch_fork",
          "anchor": {
            "block_id": "<ANCHOR_BLOCK_ID>",
            "group_entry_node_path": "<ANCHOR_GROUP_ENTRY_NODE_PATH>",
            "node_ref": "<ANCHOR_NODE_REF>",
            "pin_ref": "<ANCHOR_PIN_REF>",
            "link_ref": "<ANCHOR_LINK_REF>"
          },
          "inserted": {
            "call_kind": "owned_block_call",
            "block_id": "<INSERTED_BLOCK_ID>"
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
      "should_save": true
    }
  }
}
```

Read-back pass criteria:

- A Sequence or equivalent branch distribution node is inserted.
- Anchor exec now points to the distribution node.
- Inserted owned block call is reachable.
- Original successor remains reachable.
- Anchor no longer directly bypasses the inserted distribution.

## 9. UMG And Data Ring

### 9.1 UMG Widget

Target: `${SMOKE_ROOT}/WBP_BHSmokePanel`

TaskSpec requirements:

- create root `RootCanvas` as `CanvasPanel`
- create child `SmokeText` as `TextBlock`
- update `SmokeText.Text` to `Hello BlueprintHelper Smoke`

Read-back: widget tree shows both widgets, parent/child relation is correct, text property matches.

Validation: `validation.should_compile=true` for WidgetBlueprint / UMG writes.

### 9.2 DataTable

Target: `${SMOKE_ROOT}/DT_BHSmokeDamage`

Validation: `validation.should_compile=false`; DataTable rows are verified by read-back, not by Blueprint compile.

TaskSpec rows:

```json
{
  "row_strategy": "row_edit",
  "rows": [
    {
      "action": "add",
      "row_name": "Sword",
      "fields": {
        "Damage": 42,
        "DisplayName": "Sword"
      }
    },
    {
      "action": "add",
      "row_name": "Axe",
      "fields": {
        "Damage": 99,
        "DisplayName": "Axe"
      }
    },
    {
      "action": "update",
      "row_name": "Sword",
      "fields": {
        "Damage": 55
      }
    }
  ]
}
```

Read-back: both rows exist; `Sword.Damage=55`; `Axe.Damage=99`.

### 9.3 Object Property

Run a valid object-property TaskSpec against a fixture that has a known writable property. Prefer a DataAsset or Blueprint class default created specifically for this smoke. Then run the negative invalid-value case in Ring 12.

If the target is a DataAsset or plain UObject, set `validation.should_compile=false` and validate by property read-back. Only Blueprint class default changes should request compile.

Pass criteria:

- Valid property preview passes and execute applies.
- Invalid property preview is blocked with non-empty issues and does not dirty the asset.

## 10. Ownership Lifecycle Ring

Use a `block_id` from the GraphWrite read-back.

Run `manage_blueprinthelper_ownership`:

- convert one owned block to user-owned
- read-back verifies it is no longer eligible for BlueprintHelper-owned patch/merge
- attempt a patch against it and expect preview blocked with non-empty issue
- if rollback cleanup transaction is available, run rollback and verify ownership restoration or record the rollback limitation

Pass criteria: ownership state changes are explicit and blocked operations are diagnosable.

## 11. Composite Feature Ring

Run one `create_blueprint_feature` on `BP_BHSmokeActor` combining:

- component `CompositeScene`
- variable `CompositeCounter`
- class default `bCanBeDamaged=true`
- graph behavior with an owned custom event `BHSmoke_CompositeEvent`

Pass criteria:

- Preview passes or, if blocked, returns non-empty `issues[]`.
- Execute produces one TaskRunJournal.
- Read-back confirms every generated capability slice that preview accepted.
- No empty message like `preview_task failed: , modified=false.`

## 12. Negative And Safety Ring

Run these expected-blocked cases:

| Case | Expected result |
|---|---|
| target asset does not exist | preview blocked, non-empty `target_blueprint_not_found` or equivalent |
| invalid object property value | preview blocked, non-empty path/message, asset not dirty |
| `merge_owned_graph + branch_fork` missing `sequence_order` | schema/preview blocked with non-empty issue |
| `owned_block_call` with missing `inserted.block_id` | schema/preview blocked with non-empty issue |
| `remove_signature` without reference context policy | blocked preflight, non-empty issue |
| write permission rejected in Editor approval dialog | execute not attempted or returns clean rejection summary |

Pass criteria: every blocked case has a useful issue and no write side effect.

## 13. ReviewPanel And Debug Ring

Generate pending Review content with a saved GraphWrite write:

- target `BP_BHSmokeActor`
- `replace_owned_graph` or `patch_owned_graph`
- `validation.should_compile=true`
- `validation.should_save=true`

Record:

```text
task_run_id=
review_record_id=
archive_session_id=
asset_path=
visible_change_id=
debug_case_ids=[]
```

Manual Editor checks:

- Open ReviewPanel.
- Load pending changes for `BP_BHSmokeActor`.
- Verify row highlights on the selected change.
- Verify selected row shows Accept/Reject actions.
- Accept one non-root change and verify ReviewRecord action history/status.
- Reject one change with matching hash and verify rollback/status.
- RejectAll against a multi-target record and verify all pending targets are processed.
- For an asset lifecycle root record, reject root and verify same-asset child review cleanup happens only after root success.
- Verify Graph diff block drawing has node guid or recorded bounds and does not use empty preview graph.

Debug checks:

- Force a reject `needs_action` by changing current hash before reject, or run the automation group.
- Force or automation-cover `reject_failed`.
- Verify ReviewRecord writes `debug_case_ids[]`.
- Call `blueprinthelper_get_debug_case` for each id and confirm summary-only output.
- Export DebugBundle summary as developer artifact.
- Verify manifest contains Review summary artifact.
- Verify ReviewRecord does not store DebugBundle local path.
- Verify MCP does not expose DebugBundle artifact reader, local bundle path, raw payload, source content, token, settings, or `debug_export_refs`.

Automation evidence may satisfy the non-visual Debug checks:

```text
BlueprintHelper.Review.Integration.RejectNeedsActionCreatesDebugCase
BlueprintHelper.Review.Integration.RejectFailedCreatesDebugCase
BlueprintHelper.RuntimeDiagnostics.Debug.BundleSummaryExportIncludesReviewSummaryArtifact
BlueprintHelper.RuntimeDiagnostics.Debug.BundleSummaryExportRedactsSensitiveArtifacts
```

## 14. Final Read-Back Matrix

Before declaring pass, collect these facts:

| Area | Required evidence |
|---|---|
| Build | project editor target builds |
| MCP contract | TypeScript, Python, Node tests pass |
| AssetFactory | BP, BPI, struct, DataTable, WidgetBlueprint, DataAsset Blueprint class, DataAsset instance exist |
| Components | component tree includes smoke components |
| Variables | member variables/defaults exist or known gap recorded |
| Signature | function, custom event, dispatcher, override event visible |
| Class settings | interface and class default applied |
| GraphWrite append/replace/patch/merge | read-back proves correct owned graph state |
| Branch fork | inserted logic and original successor reachable |
| UMG | widget tree and text property match |
| DataTable | rows and values match |
| Object property | valid write applies; invalid write blocked |
| Ownership lifecycle | convert/blocked/rollback behavior recorded |
| Composite | accepted slices execute and read back |
| TaskRunJournal | every execute result reloads from UE |
| ReviewPanel | pending load, accept, reject, reject all, visual routing checked |
| Debug | debug ids and bundle summary boundary checked |
| Privacy | no local path/token/raw payload/default DebugBundle leakage |

## 15. Final Report Template

```text
SmokeRun:
Project:
Plugin commit/branch:
Engine:
Date:
RUN_ID:

Build:
MCP regression:
UE Automation:

TaskSpec assets:
Task run ids:
Review record ids:
Debug case ids:
DebugBundle manifest ids:
Automation report root:

Pass:
Partial:
Fail:
Known gaps:
Unexpected regressions:
Follow-up fixes:
```

## 16. Cleanup

Cleanup is optional. If you clean, delete only the smoke folder and generated reports for this run id:

```text
/Game/BlueprintHelperSmoke/${RUN_ID}
<REPORT_ROOT>
```

Do not delete Review/Debug artifacts until their ids have been copied into the final report.
