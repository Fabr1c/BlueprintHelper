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
