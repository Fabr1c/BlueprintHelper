# GraphWrite Four Cluster E2E Smoke Record 2026-05-24

## Result

Status: PASS

Latest rerun command:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\PlanArtifacts\GraphWriteFourClusterE2ESmoke_20260524\run_four_cluster_smoke.ps1"
```

Latest result root:

```text
D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\PlanArtifacts\GraphWriteFourClusterE2ESmoke_20260524\Results
```

The latest run completed the full smoke script:

- positive specs `00_create_smoke_actor.json` through `04_generic_graph.json` previewed and executed successfully
- negative spec `05_generic_expected_diagnostics.json` remained `preview_blocked` with expected missing-context diagnostics
- the script-ran `BlueprintHelper.GraphWrite` automation completed with `failed=0`
- the script-ran UE 5.6 build completed with `Result: Succeeded`

## Result Matrix

| Spec | Preview | Execute | Notes |
|---|---|---|---|
| `00_create_smoke_actor.json` | `preview_passed` | `executed` | smoke actor asset created/reused |
| `01_prepare_smoke_fixture.json` | `preview_passed` | `executed` | fixture components, variables, and baseline graph prepared |
| `02_function_field_graph.json` | `preview_passed` | `executed` | Function + Field cluster smoke passed |
| `03_event_delegate_graph.json` | `preview_passed` | `executed` | EventDelegate use-site smoke passed |
| `04_generic_graph.json` | `preview_passed` | `executed` | Generic create/convert/type-promotion smoke passed |
| `05_generic_expected_diagnostics.json` | `preview_blocked` | N/A | expected negative fixture remains blocked |

## Automation Evidence

| Scope | Artifact | Result |
|---|---|---|
| Full GraphWrite automation from smoke script | `D:\UEProjects\Template\Saved\Automation\GraphWrite_FourClusterE2ESmoke_20260524_001\index.json` | PASS: `succeeded=157`, `succeededWithWarnings=11`, `failed=0`, `notRun=0` |
| Focused further-fix automation | `D:\UEProjects\Template\Saved\Automation\GraphWrite_FurtherFix_Focused_003\index.json` | PASS: `succeeded=4`, `failed=0`; covers call `semantic_kind`, shallow runtime payload, context evidence BuildRequest, and coordinator contract |
| Generic `asset_action` ActionDatabase-backed success | `D:\UEProjects\Template\Saved\Automation\GraphWrite_AssetAction_GREEN_001\index.json` | PASS: `succeeded=2`, `succeededWithWarnings=1`, `failed=0`; warning scope is EOS/network-only noise |
| Generic `type_promotion` typed evidence success | `D:\UEProjects\Template\Saved\Automation\GraphWrite_TypePromotion_GREEN_002\index.json` | PASS: `succeeded=2`, `failed=0` |
| UE 5.6 build | `C:\Users\CharlieNotFound\AppData\Local\UnrealBuildTool\Log.txt` | PASS: latest build reports `Result: Succeeded` |

## Fixture Update

| File | Change |
|---|---|
| `04_generic_graph.json` | Includes a positive Generic `type_promotion` statement with `context_evidence`: `type_promotion_stable_id=type_promotion:Add:int:real`, `type_promotion_operator=Add`, `source=int`, `target=real`, `result=real`. |
| `05_generic_expected_diagnostics.json` | Keeps no-evidence diagnostics for `type_promotion`, `timer_delegate_node`, and `latent_or_async_node`. |

`asset_action` positive CLI smoke remains intentionally absent. The smoke fixture still does not have an honest ActionDatabase projection step that can generate a real `asset_action_stable_id`, so positive proof remains C++ automation instead of a fake CLI success row.

## Crash Follow-Up

An intermediate rerun reached `01_prepare_smoke_fixture.json` and then the Editor process asserted in `BuildCallFunctionFragment` while copying `FBlueprintHelperGraphFragmentBuildRequest`. That failure was not recorded in the bug document as a smoke fixture bug. It was fixed in the code path and covered by:

- `GraphFragmentBuildRequestRuntimePayload`: verifies BuildRequest no longer stores recursive SemanticIR payload
- `CallFragmentSemanticKindOwnership`: now exercises registry-level call fragment construction with literal args
- `FunctionFragmentLifecycleCoordinator`: verifies call/action-provider fragments pass the build request by scoped reference and retain shared coordinator ownership

The final smoke rerun after that fix completed successfully.

## Historical Harness Notes

Earlier reruns failed before validating the updated fixtures because the Editor/Bridge was not running:

```text
bridge_unavailable
Bridge connection error: connect ECONNREFUSED 127.0.0.1:54321
```

That was a harness lifecycle issue only and was not written to the bug document.

## Conclusion

The four-cluster end-to-end smoke is now closed for this plan:

- Function, Field, EventDelegate, and Generic positive specs preview and execute
- Generic no-evidence diagnostics remain honest negative coverage
- `asset_action` and `type_promotion` positive evidence remains ActionDatabase / FTypePromotion backed
- call fragment `semantic_kind` ownership and shallow runtime payload regressions are covered by focused automation
- no non-plugin harness issue has been added to the bug document
