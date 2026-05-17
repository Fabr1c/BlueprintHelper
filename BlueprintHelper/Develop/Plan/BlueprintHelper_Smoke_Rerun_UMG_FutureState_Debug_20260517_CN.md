# BlueprintHelper Smoke Rerun - UMG / Future State / Debug 2026-05-17

## Scope

本轮只覆盖用户指定的三条线：

1. UMG WidgetTree execute smoke。
2. UMG / DataTable dry-run 未来状态。
3. Debug / DebugBundle 手动环对应的可自动化后端验证。

执行方式：不使用 MCP；全部通过 `UnrealEditor-Cmd.exe` Automation 和 UBT 编译验证。

## Result Summary

| Scope | Report | Result |
|---|---|---|
| UMG WidgetTree execute smoke | `Saved/Automation/UMGWidgetTreeExecute_20260517_003/index.json` | 1/1 passed, warnings=0 |
| UMG dry-run future state | `Saved/Automation/FutureStateDryRun_UMG_20260517_002/index.json` | 1/1 passed, warnings=0 |
| DataTable dry-run future state | `Saved/Automation/FutureStateDryRun_DataTable_20260517_005/index.json` | 1/1 passed, warnings=0 |
| Debug / DebugBundle diagnostics | `Saved/Automation/DebugRuntimeDiagnostics_20260517_002/index.json` | 9/9 passed, warnings=0 |
| Review reject action ring | `Saved/Automation/ReviewRejectAction_20260517_002/index.json` | 9/9 passed, warnings=0 |
| Review needs_action -> DebugCase | `Saved/Automation/ReviewDebugNeedsAction_20260517_001/index.json` | 1/1 passed, warnings=0 |
| Review reject_failed -> DebugCase | `Saved/Automation/ReviewDebugFailed_20260517_001/index.json` | 1/1 passed, warnings=0 |
| Lifecycle accept regression touched by test identity fix | `Saved/Automation/ReviewAcceptLifecycle_20260517_001/index.json` | 1/1 passed, warnings=0 |

Compile evidence:

- `Saved/BuildLogs/UBT-SmokeRerun-20260517-r9.log`: `TemplateEditor Win64 Development` succeeded.

## Bugs Found And Fixed

1. UMG execute initially failed because newly created widgets were attached to the WidgetTree but not registered in `WidgetVariableNameToGuidMap`; UMG compile then reported that `SmokeText` was added without a GUID.
   - Fixed in `FBlueprintHelperWidgetService::AddWidget()` by setting transactional flags and calling `UWidgetBlueprint::OnVariableAdded()`.
   - `RemoveWidget()` now mirrors this through `OnVariableRemoved()`.

2. DataTable dry-run initially passed with warnings because transient in-memory DataTables were resolved through `StaticLoadObject()` before checking loaded objects.
   - Fixed DataTable asset resolution to use loaded object lookup before load.
   - Fixed TaskRuntime dirty-target lookup similarly.

3. Non-graph TaskPlan preview was trying to resolve every target as a Blueprint before checking whether a step actually contained `call_function` statements.
   - Fixed by delaying Blueprint/Graph resolution until call_function statements are present.

4. Review reject needs_action did not fire for current-state hash mismatches when injected reject options supplied a current hash.
   - Fixed by comparing current hash with `RecordedAfterHash` before rollback in both injected and default reject paths.
   - This preserves user/late writes and allows needs_action DebugCase linkage.

5. Review lifecycle tests were asserting transaction ids as change ids, but current Review identity appends the visual group key to `ChangeId`.
   - Tests now derive root/child `ChangeId` from loaded `PendingChanges`.

## Current Status

- D1 UMG / DataTable dry-run future-state automation: closed for the automated scope covered by `DryRunUsesPlannedWidgetState` and `DryRunUsesPlannedRowState`.
- UMG WidgetTree execute smoke: closed for the commandlet smoke path.
- B1 Debug / DebugBundle backend automation: closed for DebugCase ids, DebugBundle summary/redaction, Review summary artifact, reject needs_action, and reject_failed linkage.

Remaining boundary:

- This run did not use the live ReviewPanel UI. Visual confirmation of actual Slate row highlights, focus capture timing, and manual button click flow remains part of the separate ReviewPanel live UI smoke track, not this automation ring.
