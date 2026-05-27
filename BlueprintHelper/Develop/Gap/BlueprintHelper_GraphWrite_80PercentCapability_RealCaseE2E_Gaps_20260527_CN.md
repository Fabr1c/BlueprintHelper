# BlueprintHelper GraphWrite 80% 真实案例 E2E Gap 记录

日期：2026-05-27

本文件已按当前 live evidence 重写。旧版 Evidence 文档不再作为能力状态来源；Gap 状态以当前实现、真实 uasset E2E、CLI TaskSpec / ReadSpec 输出和 UE Automation rerun 为准。

## 1. 当前测试证据

| Gate | 结果 | Evidence |
|---|---|---|
| TemplateEditor build | PASS | `E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReloadFromIDE` exited `0`. |
| Capability80 automation | PASS | `Saved/Automation/GraphWrite80_Capability80_Fix02_20260527_001/index.json`; 5 succeeded, 0 failed. |
| Generality real-case E2E | PASS | `Saved/Automation/GraphWrite80_RealCaseE2E_AllFix07_Chunked_20260527`; 139/139 operations passed, 174/174 variants passed, allOperationsPassed=true. |
| PhysicalDoor AgentFlow E2E | PASS | `Saved/Automation/GraphWrite80_PhysicalDoor_AgentFlow_20260527_001`; new asset `/Game/BlueprintHelper/PhysicalDoor/BP_PhysicalDoor_AgentFlow_20260527_001`; setup preview/execute passed; GraphWrite `LightPush` / `ForceOpen` / `CollisionClose` preview+execute+readback passed. |
| TaskRuntime CallFunction focused rerun | PASS | MCP console `Automation RunTests BlueprintHelper.GraphWrite.TaskRuntime.CallFunction`; `Saved/Logs/Template.log` at `2026.05.27-05.56.27`; 5 tests performed, 5 succeeded, 0 failed. |
| ContainerAction focused rerun | PASS | MCP console `Automation RunTests BlueprintHelper.GraphWrite.ContainerAction`; `Saved/Logs/Template.log` at `2026.05.27-05.57.15`; 14 tests performed, 14 succeeded, 0 failed. |
| TargetObject runtime readback | PASS | MCP console `Automation RunTests BlueprintHelper.GraphWrite.TaskRuntime.CallFunction.TargetObjectPreviewExecuteReadBack`; included in the 5-test TaskRuntime CallFunction focused rerun. |
| TargetObject pin-type cache key | PASS | MCP console `Automation RunTests BlueprintHelper.GraphWrite.TaskRuntime.CallFunction.TargetObjectPinTypeCacheKey`; `Saved/Logs/Template.log` at `2026.05.27-05.55.54`; 1 test performed, 1 succeeded, 0 failed. |
| TaskRuntime CallFunction cache focused rerun | PASS | MCP console `Automation RunTests BlueprintHelper.TaskRuntime.CallFunctionResolutionCache`; `Saved/Logs/Template.log` at `2026.05.27-05.56.48`; 4 tests performed, 4 succeeded, 0 failed. |
| Full GraphWrite regression | PASS | MCP console `Automation RunTests BlueprintHelper.GraphWrite`; `Saved/Logs/Template.log` at `2026.05.27-06.00.08`; 315 tests performed, 315 succeeded, 0 failed, 0 automation errors. |

## 2. Gap 状态

| ID | Severity | Scope | Previous symptom | Closure evidence | Status |
|---|---|---|---|---|---|
| GWE2E-20260527-001 | High | Extended `container_action` roles | Public role evidence was not consumed by the C++ SemanticIR role parser. | Generality E2E `139/139` operations pass; ContainerAction focused rerun `14/14` pass; full GraphWrite `315/315` pass. | FIXED |
| GWE2E-20260527-002 | High | EventDelegate component-bound/bind/assign/unbind/clear | Handler/signature evidence was placeholder or semantically invalid. | Generality E2E `139/139` operations pass; full GraphWrite `315/315` pass. | FIXED |
| GWE2E-20260527-003 | Medium | `generic_ops.create.make_map` | Make-map evidence did not match the current create resolver contract. | Generality E2E `139/139` operations pass; full GraphWrite `315/315` pass. | FIXED |
| GWE2E-20260527-004 | Medium | Struct make/break and `field.struct_member_set` | Struct fields and field struct facts were missing. | Generality E2E `139/139` operations pass; full GraphWrite `315/315` pass. | FIXED |
| GWE2E-20260527-005 | Medium | Type promotion, schedule, asset action evidence | Generated evidence did not match current resolver/projector contracts. | Generality E2E `139/139` operations pass; full GraphWrite `315/315` pass. | FIXED |
| GWE2E-20260527-006 | Medium | `op.intpoint_equal` | Fixture used incompatible operands / unavailable hard-coded function target. | Generality E2E `139/139` operations pass; full GraphWrite `315/315` pass. | FIXED |
| GWE2E-20260527-007 | High | TaskRuntime CallFunction and P6 readback | Runtime execute/readback regression caused focused GraphWrite failures. | Full GraphWrite regression before PhysicalDoor target fix passed as `291 succeeded`, `19 succeededWithWarnings`, `0 failed`; final full rerun passes `315/315`. | FIXED |
| GWE2E-20260527-008 | High | `call_function.target_object` receiver binding | PhysicalDoor component member calls could treat `target_object` as a normal argument, or fail to connect the callable receiver `Target` pin; runtime readback later isolated an unlinked receiver edge caused by inconsistent statement fragment ids across DAG, pipeline, registry, and TaskRuntime paths; final review found runtime pre-resolution cache keys still lacked `TargetObjectPinType` evidence. | Dedicated `target_object` call DAG port, resolver context normalization, runtime preview skip, receiver-pin alias, cache version bump, centralized statement fragment id generation through `FBlueprintHelperGraphStatementTypeUtils::MakeStatementFragmentId`, and `TargetObjectPinType` propagation from semantic `target_object.pin_type` / receiver-edge pin evidence without reclassifying it as a normal argument; PhysicalDoor `fix03` preview/execute/readback pass; TaskRuntime CallFunction focused rerun `5/5` pass; cache focused rerun `4/4` pass; full GraphWrite `315/315` pass. | FIXED |
| GWE2E-20260527-009 | High | ContainerAction target role after target-object fix | First full rerun after the call receiver fix failed 4 ContainerAction tests because non-call statements emitted receiver data edges as `target_object` instead of `target`. | `BuildSimpleStatement` now keeps `target_object` only for `kind=call`; non-call statements keep `target`; ContainerAction focused rerun `14/14` pass; full GraphWrite `315/315` pass. | FIXED |

## 3. PhysicalDoor AgentFlow Evidence

| Phase | Evidence | Result |
|---|---|---|
| User instruction artifact | `Develop/PlanArtifacts/GraphWrite80_PhysicalDoor_AgentFlow_20260527/00_user_instruction.md` | Simulates ordinary Agent instruction; Setup after asset creation is not used as a custom E2E runner. |
| Setup | `01_create_asset`, `02_setup_fixture`, `03_ensure_functions` preview/execute outputs | PASS; creates the new uasset, `Root` / `Hinge` / `DoorMesh`, state variables, and graph scopes. |
| LightPush | `04_graphwrite_light_push_preview_fix03`, `04_graphwrite_light_push_execute_fix03`, `12_read_light_push_graph_logic_json_fix03` | PASS; opens to `177`, enables DoorMesh physics, applies light impulse, 0 orphan nodes. |
| ForceOpen | `05_graphwrite_force_open_preview_fix03`, `05_graphwrite_force_open_execute_fix03`, `13_read_force_open_graph_logic_json_fix03` | PASS; same open flow with stronger impulse, 0 orphan nodes. |
| CollisionClose | `06_graphwrite_collision_close_preview_fix03`, `06_graphwrite_collision_close_execute_fix03`, `14_read_collision_close_graph_logic_json_fix03` | PASS; branches on `DoorOpenAngle <= ClosedThresholdDegrees`, closes state, disables DoorMesh physics, 0 orphan nodes. |

PhysicalDoor scoring: setup failures `0`; GraphWrite flow failures `0/3`; call correctness `PASS`; silent wrong graph `0`.

## 4. Closure Rule

Gap entries are closed only because current live gates passed: real-case generality E2E, PhysicalDoor ordinary Agent flow, focused ContainerAction rerun, and full GraphWrite rerun. Future regressions must reopen or add a new dated Gap row rather than relying on this closed state.
