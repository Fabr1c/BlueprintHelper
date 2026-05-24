# GraphWrite Four Cluster E2E Smoke Bug Record 2026-05-24

## Recording Rules

- This document records only plugin implementation defects found or reproduced by the four-cluster E2E smoke: GraphWrite semantic defects, TaskSpec schema/compiler defects, readback defects, automation defects, and build behavior defects.
- Do not record non-plugin issues here, including an editor not being started, unmet Bridge prerequisites, local PowerShell execution policy, command invocation mistakes, or unrelated dirty worktree items.
- Controlled diagnostics are not bugs when they match the smoke plan. Example: Generic `type_promotion` returning `needs_more_semantic_context`.

## Bug Index

| ID | Severity | Owner area | Status | Summary |
|---|---|---|---|---|
| BUG-001 | P1 | GraphWrite FieldVariableActionCluster / ActionContext owner evidence | FIXED | Function+Field positive smoke previously blocked because component property write `DoorMesh.RelativeRotation.Roll` lost owner evidence. |
| BUG-002 | P1 | AgentFace TaskSpec compiler / EventDelegate public lowering + C++ EventDelegate use-site target binding | FIXED | EventDelegate positive smoke previously failed at public kind lowering, then at delegate target pin binding; both paths now pass. |

## BUG-001 Closure

- Fixed behavior: `field_scope=property_path` keeps owner root `DoorMesh` as `TargetPath`, keeps member path `RelativeRotation.Roll` as `PropertyPath/Query`, and projects component owner evidence.
- Code boundary: fix stays in ActionContext demand/inference. `FieldVariableActionResolver` strict owner evidence guard was not weakened.
- Targeted evidence:
  - `D:\UEProjects\Template\Saved\Automation\GraphWrite_Bug001_ActionContext_FieldPropertyPath_20260524_003\index.json`
  - Result: `succeeded=1`, `failed=0`, `notRun=0`
- E2E evidence:
  - `BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/Results/02_function_field_graph.json.preview.json`
  - `BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/Results/02_function_field_graph.json.execute.json`
  - Result: preview `preview_passed`, execute `executed` on `/Game/BlueprintHelper/Smoke/BP_GraphWriteFourClusterSmoke_20260524_FixRun02`

## BUG-002 Closure

- Fixed behavior:
  - Agent-facing `component_bound_event` and `delegate.bind/assign/unbind/unbind_all/call` compile through TS task-core into canonical internal GraphWrite statements.
  - Agent-authored internal `kind=delegate` remains rejected at compiler boundary.
  - `append_new_owned_graph` EventDelegate entries preserve Signature-owned custom event declarations and execute GraphWrite use-sites after signature steps.
  - C++ EventDelegate DAG/build path accepts canonical `ComponentBoundEvent` / `Delegate` kinds.
  - Delegate use-site fragments connect component binding object to UE delegate node target pin, including UE `PN_Self` target-pin form.
- Code boundary: GraphWrite still owns use-site only; `custom_event` declaration remains Signature-owned.
- Targeted evidence:
  - `D:\UEProjects\Template\Saved\Automation\GraphWrite_Bug002_EventDelegateGraphStatement_20260524_005\index.json`: `succeeded=6`, `failed=0`
  - `D:\UEProjects\Template\Saved\Automation\GraphWrite_Bug002_EventDelegateActionResolution_20260524_001\index.json`: `succeeded=10`, `failed=0`
  - `D:\UEProjects\Template\Saved\Automation\GraphWrite_Bug002_DelegateBoundary_20260524_003\index.json`: `succeeded=3`, `failed=0`
  - `AgentFaceService/task-core` node tests: `159/159` passed after the final smoke rerun.
- E2E evidence:
  - `BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/Results/03_event_delegate_graph.json.preview.json`
  - `BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/Results/03_event_delegate_graph.json.execute.json`
  - Result: preview `preview_passed`, execute `executed` on `/Game/BlueprintHelper/Smoke/BP_GraphWriteFourClusterSmoke_20260524_FixRun02`

## Remaining Plugin Bugs From This Smoke

None.

Generic expected diagnostics remain accepted planned blockers, not bugs:

- `05_generic_expected_diagnostics.json` preview returns `preview_blocked` because `type_promotion` requires projected spawner evidence.
- This does not fake success through FunctionAction, struct fallback, parsed-node mutation, or unsupported legacy path.
