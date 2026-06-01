# RestoreDevice TaskSpec Spawn E2E Debug 2026-06-01

## Scope

- Source gap doc: `BlueprintHelper/Develop/Gap/RestoreDevice_TaskSpec_Spawn_Diff.md`
- Goal: verify whether the listed RestoreDevice spawn/diff bugs are fixed by a real editor-backed E2E run.
- Environment: `D:\UEProjects\Template`, UE `E:\UE_5.6`, BlueprintHelper CLI after reinstall.

## Original TaskPlan Replay

The original low-level `BlueprintHelper.TaskPlan.v1` from the gap doc was extracted and sent directly through Bridge preview because the current public CLI accepts agent-facing `BlueprintHelper.TaskSpec.v1`, not compiled TaskPlan JSON.

Evidence:

- Extracted plan: `.tmp/restore_device_e2e_20260601_095650/restore_device_taskplan.json`
- Direct preview result: `.tmp/restore_device_e2e_20260601_095650/preview.bridge.json`

Result:

- Bridge call reached `preview_task_plan`.
- Preview was blocked before graph execution because the original target asset/class is absent locally:
  - `/Game/Gameplay/Core/LocalMpMode/BP_BallMazeGameMode_LocalMultiPlayer`
  - `InputRoutingSubsystem`
  - `BindInputDeviceToPlayer`

This means the exact historical RestoreDevice asset E2E cannot be reproduced on this machine.

## Local Equivalent E2E

A local Blueprint asset was created to cover the same bug surfaces:

- Asset: `/Game/BlueprintHelperTemp/BP_RestoreDevice_E2E_20260601_095650`
- Event: `CE_RestoreDevice_E2E_20260601_095650`
- Variables:
  - `ControllerDeviceMap`: `map<int,int>`
  - `LastDeviceId`: `int`
  - `LastLocalPlayerIndex`: `int`
- Logic pattern repeated for player indices `1`, `2`, `3`:
  - `ControllerDeviceMap.Contains(player)`
  - branch then executes `ControllerDeviceMap.Find(player)`
  - `result_symbol` feeds `Set LastDeviceId`
  - `Set LastLocalPlayerIndex` receives the player index literal

Artifacts:

- Create asset preview: `.tmp/restore_device_e2e_20260601_095650/create_asset.preview.full.json`
- Create asset execute: `.tmp/restore_device_e2e_20260601_095650/create_asset.execute.full.json`
- Feature preview: `.tmp/restore_device_e2e_20260601_095650/feature.preview.full.json`
- Feature execute: `.tmp/restore_device_e2e_20260601_095650/feature.execute.full.json`
- Readback summary: `.tmp/restore_device_e2e_20260601_095650/e2e_readback_summary.json`
- Review query: `.tmp/restore_device_e2e_20260601_095650/query_review_records_by_asset.full.json`

## Findings

### Fixed or Passing

1. Build gate passed before E2E:
   - `Build.bat TemplateEditor Win64 Development ...`
   - Result: succeeded / target up to date.
2. Feature preview succeeded:
   - `requested_node_count=33`
   - `spawned_node_count=22`
   - `requested_link_count=12`
   - `created_link_count=23`
3. Branch exec preservation for pure map query is fixed in readback:
   - `nodes[1].then -> nodes[2].execute`
   - `nodes[4].then -> nodes[5].execute`
   - `nodes[7].then -> nodes[8].execute`
4. Focused dynamic cast automation passed:
   - `BlueprintHelper.GraphWrite.CallFunctionResolver.ConvertExpression.DynamicCastIsPure`
   - Automation log result: `Test Completed. Result={成功}`
5. Review record creation is not empty in this local E2E:
   - Query returned 2 records for the E2E asset.
   - Feature record status: `pending`
   - `visible_change_count=4`
   - Operation kinds include `ensure_custom_event` and `append_blueprint_graph`.

### Still Failing

1. `Map.Find` result pin binding is still wrong in real E2E.

   Feature execute failed compilation with three errors:

   ```text
   Blueprint compile failed for /Game/BlueprintHelperTemp/BP_RestoreDevice_E2E_20260601_095650 with 3 error(s).
   整数类型的 Return Value 和属性ReturnValue（属于BoolProperty类型）不匹配
   整数类型的 Return Value 和属性ReturnValue（属于BoolProperty类型）不匹配
   整数类型的 Return Value 和属性ReturnValue（属于BoolProperty类型）不匹配
   ```

   Interpretation: the E2E path still binds the requested integer `result_symbol` to the boolean `ReturnValue` output instead of the map `Value` output.

2. Later literal defaults are still wrong.

   Readback summary for `Set LastLocalPlayerIndex` defaults:

   ```json
   ["0", "0", "0"]
   ```

   Expected values from the TaskSpec were:

   ```json
   ["1", "2", "3"]
   ```

   Interpretation: default literal propagation is still not fixed for this real multi-statement E2E path.

## Current Conclusion

The bugs in `RestoreDevice_TaskSpec_Spawn_Diff.md` are not all fixed.

Confirmed fixed:

- Pure query exec preservation around branch-then flow.
- Focused `dynamic_cast` expression lowering as pure cast.
- Review records are generated for the local graph-write E2E, so this run does not reproduce the previous empty Review chain.

Confirmed still failing:

- `map.find.result_symbol` still resolves to `ReturnValue` in the real execute path.
- Literal/default propagation still collapses later player index values to `0`.

Next fix should target the UE GraphWrite/runtime node binding path used by real execute/readback, not only the TypeScript lowering fixture path.

## Follow-up Fix and Verification (2026-06-01)

The two remaining failures above have now been reproduced with focused automation, fixed, and re-verified through a fresh real CLI E2E.

### Root Cause 1: integer literals became invalid int pin defaults

Evidence:

- Failing focused test before the fix: `BlueprintHelper.GraphWrite.ContainerAction.SemanticIR`.
- Symptom: JSON number literals were stored as UE import text such as `7.000000`.
- Real E2E symptom: `Set LastLocalPlayerIndex` pins read back as `["0", "0", "0"]` instead of `["1", "2", "3"]`.

Cause:

- `FBlueprintHelperGraphSemanticIRUtils::JsonValueToString(EJson::Number)` used `LexToString(Value->AsNumber())`.
- JSON numbers are read as `double`, so integer values were serialized with a fractional suffix.
- UE int pins did not accept that import text and kept their default `0`.

Fix:

- Integer-valued JSON numbers are now emitted without a fractional suffix.
- Non-integer numbers still use sanitized float formatting.

### Root Cause 2: `map.find.result_symbol` used the wrong output pin on existing-entry TaskRuntime flows

Evidence:

- Failing real E2E before the fix: `.tmp/restore_device_e2e_20260601_103936/feature.execute.full.json`.
- Compile error repeated three times:

```text
整数类型的 Return Value 和属性ReturnValue（属于BoolProperty类型）不匹配
```

- Readback of that failed graph showed the integer defaults were already correct, so this was independent from the literal-default bug.
- New focused RED test: `BlueprintHelper.GraphWrite.ContainerAction.RestoreDeviceSequentialTaskPlanShape`.
  - It creates an existing CustomEvent entry.
  - It uses `options.reconstruct_existing_nodes=true`.
  - It uses the same sequential three-branch shape emitted by TaskRuntime `ensure_entry`.

Cause:

- Container vocabulary correctly defines `map.find` as `result -> Value`.
- `ApplyContainerActionRolePinAliases` only projected role aliases to `PinBindings` and `DataInputs`, not `DataOutputs`.
- The generic output alias `result` could therefore remain bound to `ReturnValue`.
- `ApplyContainerActionResolvedPinTypesToNode` also applied the resolved result type to every output pin whenever `bReturnsValue` was true. For `map.find`, this incorrectly changed the bool `ReturnValue` pin to int.

Fix:

- Output role aliases now project to `DataOutputs` and can override generic aliases.
- `result` role pin typing now resolves through the container result type.
- Generic return-pin typing is restricted to specs whose result kind is `ReturnValue`; output-pin result operations such as `map.find` no longer rewrite every output pin.

### Verification

Focused automation after the fix:

```text
BlueprintHelper.GraphWrite.ContainerAction.SemanticIR -> exit 0
BlueprintHelper.GraphWrite.ContainerAction.RestoreDeviceShape -> exit 0
BlueprintHelper.GraphWrite.ContainerAction.MapFindResultSymbolValuePin -> exit 0
BlueprintHelper.GraphWrite.ContainerAction.RestoreDeviceSequentialTaskPlanShape -> exit 0
```

Build gate:

```text
Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReload
Result: Succeeded
```

Fresh real CLI E2E:

- Asset: `/Game/BlueprintHelperTemp/BP_RestoreDevice_E2E_20260601_105744`
- Artifacts:
  - `.tmp/restore_device_e2e_20260601_105744/create_asset.preview.full.json`
  - `.tmp/restore_device_e2e_20260601_105744/create_asset.execute.full.json`
  - `.tmp/restore_device_e2e_20260601_105744/feature.preview.full.json`
  - `.tmp/restore_device_e2e_20260601_105744/feature.execute.full.json`
  - `.tmp/restore_device_e2e_20260601_105744/read_graph_logic_json.full.json`
  - `.tmp/restore_device_e2e_20260601_105744/e2e_readback_summary.json`
  - `.tmp/restore_device_e2e_20260601_105744/query_review_records_by_asset.full.json`

E2E result:

```json
[
  { "file": "create_asset.preview.full.json", "exit_code": 0, "ok": true, "status": "preview_passed" },
  { "file": "create_asset.execute.full.json", "exit_code": 0, "ok": true, "status": "executed" },
  { "file": "feature.preview.full.json", "exit_code": 0, "ok": true, "status": "preview_passed" },
  { "file": "feature.execute.full.json", "exit_code": 0, "ok": true, "status": "executed" }
]
```

Readback summary:

```json
{
  "read_exit_code": 0,
  "read_ok": true,
  "read_status": "completed",
  "last_player_defaults": ["1", "2", "3"],
  "map_find_nodes": 3,
  "set_last_device_nodes": 3
}
```

Review evidence:

- `blueprinthelper_query_review_records` by asset returned `record_count=2`.

### Updated Conclusion

The two remaining bugs from the earlier E2E are now fixed in the local verified path:

1. `LastLocalPlayerIndex` literal defaults no longer collapse to `0`.
2. `map.find.result_symbol` no longer compiles as an int `ReturnValue` / bool `ReturnValue` mismatch in the real existing-entry TaskRuntime path.
