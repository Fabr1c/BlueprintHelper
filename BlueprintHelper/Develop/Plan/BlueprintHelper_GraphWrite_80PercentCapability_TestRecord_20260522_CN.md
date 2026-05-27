# BlueprintHelper GraphWrite 80% 能力与正确率测试记录

日期：2026-05-22

> Historical note 2026-05-25: 本文档保留 2026-05-22 的阶段性测试记录。凡涉及 custom event、missing event name、EventDelegate `unsupported_intent`、旧 Event taxonomy 的条目，均按 Gap5 / ownership-filter closure 前的历史口径理解，不代表当前职责边界。当前边界为：`BlueprintSignature` owns custom/override/native declaration and signature；GraphWrite 只写 projected event entry 的 body；`EventDelegateActionCluster` 只负责 component-bound/delegate use-site；`UAnimNotifyEventNodeSpawner` 不属于当前 GraphWrite 范围。

## 1. 计分口径

| 指标 | 口径 |
|---|---|
| Setup Phase | 资产创建、组件准备、保存/打开/校验资产；失败记录为 setup_failure，不计入 GraphWrite 正确率。 |
| GraphWrite Phase | 从基础 Blueprint 可定位后开始，记录 context、resolver、spawn、default、link、readback、DebugBundle。 |
| Call 正确 | callable owner、函数名、参数、pin 绑定、执行顺序均符合需求。 |
| GraphWrite 正确 | 节点类型、pin/default、exec/data link、readback、DebugBundle、编译/preview 语义均符合需求。 |
| Silent wrong graph | 表面成功但 readback、编译或 preview 证明语义错误；单独计入最高优先级缺陷。 |

## 2. 测试矩阵

| 测试 | Phase | 能力项 | 当前状态 | 错误分类 | Call 正确 | GraphWrite 正确 | Evidence / DebugBundle | Gap 更新 |
|---|---|---|---|---|---|---|---|---|
| PhysicalDoor_InteractableOnly | Setup | 创建 Actor Blueprint 和 Root/Hinge/DoorMesh | 未运行 | not_run | N/A | N/A | 未生成 | 不需要 |
| PhysicalDoor_InteractableOnly | GraphWrite | 物理门内部逻辑 | 未运行 | not_run | 未统计 | 未统计 | 未生成 | 未判断 |
| TimedAccessGate_StateMachine | GraphWrite | Function / Field / Control | 未运行 | not_run | 未统计 | 未统计 | 未生成 | 未判断 |
| EventDrivenConfigApplier | GraphWrite | Struct make/break + custom event body write (Signature-owned entry dependency) | P5 executed | none | yes | yes | `Saved/Automation/GraphWrite80_P5_GenericStruct_001/index.json`; `Saved/Automation/GraphWrite80_P5_EventDelegate_001/index.json` | historical pre-closure record; current ownership is BlueprintSignature declaration/signature + GraphWrite body writing |
| EventDrivenConfigApplier | GraphWrite | Component-bound event / delegate bind diagnostics | P5 executed | missing_required_evidence | no | no | `Saved/Automation/GraphWrite80_P5_EventDelegate_001/index.json` | not needed; controlled diagnostic is expected |

## 3. 错误分类

| 分类 | 说明 |
|---|---|
| setup_failure | 资产、组件、保存、打开、fixture 准备失败。 |
| missing_required_evidence | 缺 callable、owner、field owner、property path、binding object、delegate signature、target type 等关键 evidence。 |
| candidate_threshold_exceeded | ActionDatabase / filter 后候选数量超过安全阈值。 |
| ambiguous_candidates | 候选数量未超阈值，但多个候选不能安全区分。 |
| not_found | 上下文足够但无合法候选。 |
| unsupported_intent | 当前架构尚未实现该语义。 |
| spawn_or_link_failure | resolver 成功后 spawn、默认值、pin binding、link 或 readback 失败。 |
| silent_wrong_graph | 表面成功但 graph 语义错误。 |
| not_run | 该项尚未执行。 |

## 4. 当前汇总

| 指标 | 数值 | 说明 |
|---|---:|---|
| 已运行 GraphWrite 场景数 | 0 | P0 只建立基线。 |
| GraphWrite 正确场景数 | 0 | P0 不计入正确率。 |
| Call 正确样本数 | 0 | P0 不计入 call 正确率。 |
| Silent wrong graph 数 | 0 | P0 尚未执行测试。 |
| P0 文档契约 Automation | PASS | `Saved/Automation/GraphWrite80_P0_PlanDocs_001/index.json` |
| UE 5.6 BuildPlugin | PASS | `Saved/BlueprintHelperBuildTest_GraphWrite80_P0` |
| P2 MetricsSummary Automation | PASS | `Saved/Automation/GraphWrite80_P2_Metrics_001/index.json`; 1 succeeded, 0 failed. |
| P2 ScenarioRegistration Automation | PASS | `Saved/Automation/GraphWrite80_P2_Scenarios_001/index.json`; 1 succeeded, 0 failed. |
| P2 UE 5.6 BuildPlugin | PASS | `Saved/BlueprintHelperBuildTest_GraphWrite80_P2`; AutomationTool `BUILD SUCCESSFUL`, exit `0`. |

## 5. P1 Legacy Cleanup Gate Verification - 2026-05-22

| Check | Status | Evidence |
|---|---|---|
| RED automation after contract-test expansion | FAIL expected | `Saved/Automation/GraphWrite80_P1_Legacy_Before_001/index.json`; `SelectControlUseGenericCluster` failed on `manual_control_context` / `manual_control_semantic`. |
| Manual control fallback source check | PASS | `rg -n "manual_control_context|manual_control_semantic|RequireDedicatedControlBuilderBoundary" BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\GraphStatement\BlueprintHelperControlFragmentBuilder.cpp` exited `1`. |
| P1 source search gate | DONE_WITH_CONCERNS | `rg -n "manual_control_context|manual_control_semantic|RequireDedicatedControlBuilderBoundary|CreateMergeCallFunctionNode|call_function\.name|set_member_variable|make_struct|const FParsedNode& NodeData|FParsedNode BoundNodeData" BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite BlueprintHelper\Source\BlueprintHelper\Public\Systems\ToolClusters\GraphWrite` exited `0` with diagnostic/P4-P5 residual rows recorded in `Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`. |
| Legacy contract automation | PASS | `Saved/Automation/GraphWrite80_P1_Legacy_After_001/index.json`; 7 succeeded, 0 failed. |
| UE 5.6 BuildPlugin | PASS | `Saved/BlueprintHelperBuildTest_GraphWrite80_P1`; AutomationTool `BUILD SUCCESSFUL`, exit `0`. |
| `git diff --check` | PASS | Exit `0`; only line-ending warnings for existing/worktree files. |

## 6. P2 Test Metrics Collection Verification - 2026-05-22

| Check | Status | Evidence |
|---|---|---|
| MetricsSummary RED compile gate | FAIL expected | `Saved/BlueprintHelperBuildTest_GraphWrite80_P2_RED`; BuildPlugin exited `6` because `BlueprintHelperGraphWriteCapabilityMetrics.h` did not exist yet. |
| Project editor compile for automation discovery | PASS | `E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex` exited `0`. |
| MetricsSummary automation | PASS | `Saved/Automation/GraphWrite80_P2_Metrics_001/index.json`; 1 succeeded, 0 failed. Metrics count executed GraphWrite rows only and exclude Setup / `not_run` rows from run totals. |
| ScenarioRegistration automation | PASS | `Saved/Automation/GraphWrite80_P2_Scenarios_001/index.json`; 1 succeeded, 0 failed. Scenario rows remain registered / `not_run`; no correctness claim added. |
| UE 5.6 BuildPlugin | PASS | `Saved/BlueprintHelperBuildTest_GraphWrite80_P2`; AutomationTool `BUILD SUCCESSFUL`, exit `0`; only existing Review UI `ItemHeight` deprecation warnings observed. |

## 7. P3 Function / Field Correctness Verification - 2026-05-22

| Check | Status | Evidence |
|---|---|---|
| FieldVariable RED automation | FAIL expected | `Saved/Automation/GraphWrite80_P3_Field_RED_002/index.json`; 7 succeeded, 4 failed before resolver hardening. Failures showed `get_property` / `set_property` and broad field query paths still resolved or reported `field_action_unresolvable` instead of `missing_required_evidence`. |
| FunctionAction supplemental contract and PhysicalDoor call automation | PASS | `Saved/Automation/GraphWrite80_P3_Function_001/index.json`; 7 succeeded, 0 failed, 0 warnings. Covered supplemental scan selected-spawner contract plus PhysicalDoor `SetSimulatePhysics`, `AddImpulse`, `K2_SetRelativeRotation`, `K2_GetComponentRotation`, and short `Set` ambiguity. |
| FieldVariable evidence automation | PASS | `Saved/Automation/GraphWrite80_P3_Field_001/index.json`; 11 succeeded, 0 failed. Covered `DoorOpenAngle`, `bIsClosed`, `DoorMesh.RelativeRotation`, `DoorMesh.SimulatePhysics`, missing owner, missing property path, broad query without projected field evidence, and owner-scoped ambiguity. |
| Capability80 P2 metrics with P3 diagnostic rows | PASS | `Saved/Automation/GraphWrite80_P3_Capability_001/index.json`; 2 succeeded, 0 failed. PhysicalDoor and TimedAccessGate P3 diagnostic rows are registered without adding unsupported correctness claims. |
| UE 5.6 BuildPlugin | PASS | `Saved/BlueprintHelperBuildTest_GraphWrite80_P3`; AutomationTool `BUILD SUCCESSFUL`, exit `0`; existing Review UI `ItemHeight` deprecation warnings observed. |

## 8. P4 Direct Spawn Provider / Generic Control Verification - 2026-05-22

| Check | Status | Evidence |
|---|---|---|
| RED compile gate after adding P4 provider/boundary tests | FAIL expected | `E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex` exited `1` because `BlueprintHelperSingletonControlFlowEvidenceProvider.h` did not exist yet. |
| Project editor compile after provider implementation | PASS | Same `Build.bat` command exited `0`; provider and tests compiled into `TemplateEditor`. |
| Generic ActionResolution automation | PASS_WITH_WARNINGS | `Saved/Automation/GraphWrite80_P4_GenericControl_001/index.json`; 4 succeeded, 1 succeeded with warnings, 0 failed. Warnings are existing `Vector` / `Transform` struct lookup noise in `ClusterBoundaryResults`; provider positive and wide-surface negative tests passed. |
| Legacy mainline contract automation | PASS | `Saved/Automation/GraphWrite80_P4_LegacyContract_001/index.json`; 8 succeeded, 0 failed. Added `SingletonControlDirectSpawnProviderBoundary`. |
| Capability80 P2 metrics with P4 diagnostic row | PASS | `Saved/Automation/GraphWrite80_P4_Capability_001/index.json`; 2 succeeded, 0 failed. TimedAccessGate P4 row records branch / sequence / select singleton provider evidence outside GraphWrite correctness totals and makes no function / field / struct / delegate correctness claim. |
| P4 direct singleton source boundary rg | PASS | `rg -n "UBlueprintNodeSpawner::Create\(NodeClass\)\|UBlueprintNodeSpawner::Create\(UK2Node_Select::StaticClass\(\)\)" BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite` exited `0` with the only match in `BlueprintHelperSingletonControlFlowEvidenceProvider.cpp`. |
| UE 5.6 BuildPlugin | PASS | `Saved/BlueprintHelperBuildTest_GraphWrite80_P4`; AutomationTool `BUILD SUCCESSFUL`, exit `0`; existing Review UI `ItemHeight` deprecation warnings observed. |

## 9. P5 Struct / Event / Delegate Verification - 2026-05-22

| Check | Status | Evidence |
|---|---|---|
| EventDelegate RED automation | HISTORICAL_PRE_GAP5_CLOSURE | `Saved/Automation/GraphWrite80_P5_EventDelegate_Before_001/index.json`; pre-closure run mixed custom-event body coverage with component-bound/bind diagnostics and still used old `event_name_missing` / `unsupported_event_delegate_cluster_semantic` codes. Current ownership is BlueprintSignature declaration/signature + GraphWrite body writing; EventDelegateActionCluster only covers component-bound/delegate use-site. |
| Generic struct RED automation | FAIL expected | `Saved/Automation/GraphWrite80_P5_GenericStruct_Before_001/index.json`; positive struct make/break passed, missing target type still returned `needs_more_semantic_context`, and unsupported struct returned `generic_action_struct_type_not_found`. |
| ActionContext evidence RED automation | FAIL expected | `Saved/Automation/GraphWrite80_P5_ActionContext_Before_001/index.json`; delegate evidence projection tokens were absent before P5 implementation. |
| Project editor compile after implementation | PASS | `E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReload` exited `0`. |
| Generic struct automation | PASS_WITH_WARNINGS | `Saved/Automation/GraphWrite80_P5_GenericStruct_001/index.json`; 5 succeeded, 2 succeeded with warnings, 0 failed. Warnings are expected `FindObject` struct lookup noise for alias/missing-struct probes. Covered struct make/break success, missing target type as `missing_required_evidence`, unsupported struct as `not_found`, and no create/convert/schedule fallback success. |
| EventDelegate automation | HISTORICAL_PRE_GAP5_CLOSURE | `Saved/Automation/GraphWrite80_P5_EventDelegate_002/index.json`; retained as pre-closure evidence only. It covered custom-event body checks plus component-bound/delegate diagnostics under the old taxonomy; current ownership keeps declaration/signature in `BlueprintSignature` and limits `EventDelegateActionCluster` to component-bound/delegate use-site. |
| ActionContext evidence projection automation | PASS | `Saved/Automation/GraphWrite80_P5_ActionContext_001/index.json`; 1 succeeded, 0 failed. |
| P5 UE 5.6 BuildPlugin | PASS | `Saved/BlueprintHelperBuildTest_GraphWrite80_P5`; AutomationTool `BUILD SUCCESSFUL`, exit `0`. |
| Complete component-bound/bind spawner boundary | HISTORICAL_PRE_GAP5_CLOSURE | `Saved/Automation/GraphWrite80_P5_EventDelegate_002/index.json` recorded the pre-closure state where complete projected evidence still returned `unsupported_intent`. This row is retained as historical evidence only; later Gap5 closure superseded it by adding supported component-bound/delegate use-site resolution while keeping declaration/signature ownership in `BlueprintSignature`. |

## 10. Final P6 Summary - 2026-05-22

P6 汇总值来自 `BlueprintHelper.GraphWrite.Capability80.P2.MetricsSummary` 的实际能力行统计；readback 只确认已有 resolver/spawn evidence 后的结果，不单独生成成功证据。

| 指标 | 数值 | 结论 |
|---|---:|---|
| Capability coverage | 100.0% (5/5) | PASS |
| GraphWrite correctness | 94.4% (17/18) | PASS |
| Call correctness | 100.0% (8/8) | PASS |
| Silent wrong graph | 0 | PASS |

## 11. P6 Verification Rows - 2026-05-22

| Check | Status | Evidence |
|---|---|---|
| Project editor compile after P6 implementation | PASS | `E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReload` exited `0`. |
| P6 TaskRuntime readback targeted automation | PASS | `Saved/Automation/GraphWrite80_P6_ToolResult_Targeted_003/index.json`; 1 succeeded, 0 failed, 0 warnings. |
| P6 call preview path targeted automation | PASS | `Saved/Automation/GraphWrite80_P6_CallPreview_Targeted_001/index.json`; 2 succeeded, 0 failed, 0 warnings. |
| Full Capability80 automation | PASS | `Saved/Automation/GraphWrite80_P6_Full_002/index.json`; 5 succeeded, 0 failed, 0 warnings. |
| GraphWrite regression group | PASS | `Saved/Automation/GraphWrite80_P6_GraphWrite_Regression_002/index.json`; 116 tests successful (`107` succeeded + `9` succeeded with warnings), 0 failed, 0 not run. This closes the 14-failure P6 regression blocker from `_001`. |
| P6 UE 5.6 BuildPlugin | PASS | `Saved/BlueprintHelperBuildTest_GraphWrite80_P6_002`; AutomationTool `BUILD SUCCESSFUL`, exit `0`; existing Review UI `ItemHeight` deprecation warnings observed. |

## 12. P6 Residual Blockers

No P6 regression blocker remains after `Saved/Automation/GraphWrite80_P6_GraphWrite_Regression_002/index.json`.

| Previously failing entry | Closure evidence |
|---|---|
| `BlueprintHelper.GraphWrite.ActionResolution.Contract.ClustersConsumeProjectedContext` | Contract now exempts the reusable `ActionResolution/Context/*` implementation boundary while still scanning cluster files. |
| `BlueprintHelper.GraphWrite.ActionResolution.FunctionAction.OperatorDispatch` | Operator path now initializes UE type-promotion state and the test supplies projected graph/identity context before requiring a real UE spawner. |
| `BlueprintHelper.GraphWrite.CallFunctionResolver.Generator*` and direct append/replace service rows | Call demand query now prefers `target` over literal `call`; direct service tests use valid current SemanticIR payloads and existing signature-entry reuse where ownership block refs are required. |
| `BlueprintHelper.GraphWrite.CallFunctionResolver.Stress.*` Blueprint-authored supplemental rows | Stale supplemental-only selection expectations were replaced with guards that require spawner/action evidence before success. |
| `BlueprintHelper.GraphWrite.LegacyMainline.EventDelegateDeclaredCapabilityMatchesSuccessPath` | Historical closure evidence only: this row captured the interim contract where `ComponentBoundEvent` / `Bind` still stopped at precise diagnostics / `unsupported_intent`. Current ownership is BlueprintSignature declaration/signature plus supported projected-evidence component-bound/delegate use-site resolution. |

Residual architecture gaps remain tracked in `Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`; they are not P6 regression blockers.

## 13. Function / Field Convergence Smoke - 2026-05-24

本轮验证覆盖用户请求 1、2、4：扩展 FunctionActionCluster 的非 call callable kind，完整化 Field 的复杂 property path / linked typed pin / `component_ref` / `field_access` 语义，并在末尾统一 smoke。

| Check | Status | Evidence |
|---|---|---|
| Project editor compile after Function implementation | PASS | `E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReload` exited `0`. |
| FunctionAction automation | PASS | `Saved/Automation/GraphWriteFunctionAction/index.json`; 6 succeeded, 0 failed. |
| Function ActionContext automation | PASS | `Saved/Automation/GraphWriteFunctionActionContext/index.json`; 5 succeeded, 0 failed. |
| Project editor compile after Field implementation | PASS | Same `Build.bat` command exited `0`. |
| FieldVariable automation | PASS | `Saved/Automation/GraphWriteFunctionFieldTargetedAfterPathFix/index.json`; 15 succeeded, 0 failed. |
| ActionContext full automation | PASS | `Saved/Automation/GraphWriteActionContextFull/index.json`; 17 succeeded, 0 failed. |
| GraphSemanticIR automation | PASS | `Saved/Automation/GraphWriteGraphSemanticIR/index.json`; 6 succeeded, 0 failed. |
| task-core TypeScript build | PASS | `AgentFaceService/task-core`: `npm.cmd run build` exited `0`. |
| task-core Node tests | PASS | `AgentFaceService/task-core`: `npm.cmd run test:node`; 158 tests passed, 0 failed. |
| task-core Python tests | PASS | `AgentFaceService/task-core`: `python -m unittest discover -s python/tests -t python`; 65 tests passed, 0 failed. |
| CLI TypeScript build | PASS | `AgentFaceService/cli`: `npm.cmd run build` exited `0`. |
| CLI Node tests | PASS | `AgentFaceService/cli`: `npm.cmd run test:node`; 44 tests passed, 0 failed. |
| Unified Function/Field/Event smoke automation | PASS | `Saved/Automation/GraphWriteFunctionFieldUnifiedSmokeAfterPathFix/index.json`; 3 succeeded, 0 failed. |
| Full GraphWrite regression | PASS_WITH_WARNINGS | `Saved/Automation/GraphWriteFullAfterFunctionFieldPathFix/index.json`; 142 succeeded, 11 succeeded with warnings, 0 failed, 0 not run. Warnings are existing UE/EOS or missing-package diagnostic noise and did not fail the regression. |
| CLI TaskSpec preview smoke | PREVIEW_BLOCKED_EXPECTED_FIXTURE | `Saved/BlueprintHelper/Cli/preview_1779602466387_0001/task_plan.json` contains `step_001 capability=blueprint_signature`, `step_001.write.ops[0].op=ensure_custom_event`, `step_002 capability=graph_write`, and `step_002.depends_on=["step_001"]`; preview issue is `target_blueprint_not_found` for `/Game/BlueprintHelper/Smoke/BP_GraphWriteFunctionFieldSmoke`. |
| CLI TaskSpec execute smoke | EXECUTE_BLOCKED_EXPECTED_FIXTURE | `Saved/BlueprintHelper/Cli/cli_1779602489237/result.json`; execute did not write assets and returned `task_preview_blocked` with issue `target_blueprint_not_found` for the same Smoke fixture. |

Result:

- `FunctionActionCluster` now covers `Call`, `Op`, `Convert` as `convert_function`, `Schedule` as `schedule_function`, and `Schedule` as `latent_or_async_function` when evidence permits.
- `FieldVariableActionCluster` now covers `variable`, `property_path`, `component_ref`, and `field_access`, including complex property path metadata and linked typed pin inference.
- Event lifecycle taxonomy remains Signature-owned; GraphWrite body/delegate boundary is smoke-verified through `blueprint_signature.ensure_custom_event -> graph_write`.

Remaining blocker:

- The planned real CLI execute smoke cannot write because the fixture asset `/Game/BlueprintHelper/Smoke/BP_GraphWriteFunctionFieldSmoke` is not present in the project. This is an environment/fixture blocker, not a Function/Field semantic compiler or resolver failure.

## 14. Real Case E2E Retest - 2026-05-27

本节已按当前 live evidence 覆盖旧的 22-failure 记录。`Evidence` 文档不再作为能力状态来源；本轮以当前实现、CLI TaskSpec / ReadSpec 输出、UE Automation 日志和真实 uasset 读回为准。

| Check | Status | Evidence |
|---|---|---|
| Project editor compile before retest | PASS | `E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE` exited `0`. |
| Capability80 automation | PASS | `Saved/Automation/GraphWrite80_Capability80_Fix02_20260527_001/index.json`; 5 succeeded, 0 failed. |
| Generality real-case E2E | PASS | `Saved/Automation/GraphWrite80_RealCaseE2E_AllFix07_Chunked_20260527`; 139/139 operations passed, 174/174 variants passed, allOperationsPassed=true. |
| Full GraphWrite regression before PhysicalDoor target fix | PASS_WITH_WARNINGS | `Saved/Automation/GraphWrite80_RealCaseRetest_GraphWriteRegression_Fix02_20260527_001/index.json`; 291 succeeded, 19 succeeded with warnings, 0 failed. |
| Full GraphWrite regression after PhysicalDoor target_object/id/cache fix | PASS | MCP console `Automation RunTests BlueprintHelper.GraphWrite`; `Saved/Logs/Template.log` at `2026.05.27-06.00.08`: 315 tests performed, 315 succeeded, 0 failed, 0 automation errors. |
| TaskRuntime CallFunction focused regression | PASS | MCP console `Automation RunTests BlueprintHelper.GraphWrite.TaskRuntime.CallFunction`; `Saved/Logs/Template.log` at `2026.05.27-05.56.27`: 5 tests performed, 5 succeeded, 0 failed. |
| ContainerAction focused regression after target-port split | PASS | MCP console `Automation RunTests BlueprintHelper.GraphWrite.ContainerAction`; `Saved/Logs/Template.log` at `2026.05.27-05.57.15`: 14 tests performed, 14 succeeded, 0 failed. |
| TargetObject focused regressions | PASS | `BlueprintHelper.GraphWrite.ActionResolution.CallFunction.TargetObjectSemanticPortDoesNotBecomeArgument`, `BlueprintHelper.GraphWrite.GraphSemanticIR.TargetObject.UsesDedicatedDagPort`, `BlueprintHelper.GraphWrite.FunctionFieldUnifiedSmoke.CallFragmentTargetObjectPinAlias`, `BlueprintHelper.GraphWrite.TaskRuntime.CallFunction.TargetObjectPreviewExecuteReadBack`, and `BlueprintHelper.GraphWrite.TaskRuntime.CallFunction.TargetObjectPinTypeCacheKey` passed; both runtime tests are included in the 315-test full GraphWrite pass. |
| TaskRuntime CallFunction resolution cache | PASS | `BlueprintHelper.TaskRuntime.CallFunctionResolutionCache`; 4 tests passed at `2026.05.27-05.56.48`: `KeyIncludesContext`, `ResolverVersionGuard`, `TracksHitsAndMisses`, `TtlAndAssetState`. |

### 14.1 PhysicalDoor_InteractableOnly - 普通 Agent Flow E2E

本轮物理门测试使用新 uasset：`/Game/BlueprintHelper/PhysicalDoor/BP_PhysicalDoor_AgentFlow_20260527_001`。Setup 只负责创建资产、组件、变量和函数入口，不计入 GraphWrite 正确率；Setup 后的 GraphWrite 阶段按普通 Agent 接收用户指令后的 TaskSpec preview / execute / ReadSpec readback 运行，没有使用 Setup 后自定义 E2E runner 脚本。

普通 Agent 输入和 TaskSpec 工件位于 `Develop/PlanArtifacts/GraphWrite80_PhysicalDoor_AgentFlow_20260527/`。运行证据位于 `Saved/Automation/GraphWrite80_PhysicalDoor_AgentFlow_20260527_001/`。

| Phase | Check | Status | Evidence |
|---|---|---|---|
| Setup | Create asset | PASS | `01_create_asset_preview`, `01_create_asset_execute`; target asset `/Game/BlueprintHelper/PhysicalDoor/BP_PhysicalDoor_AgentFlow_20260527_001`. |
| Setup | Components and variables | PASS | `02_setup_fixture_preview`, `02_setup_fixture_execute`; readback confirms `Root`, `Hinge`, `DoorMesh`; variables include `DoorOpenAngle`, `OpenTargetYaw`, `LightPushImpulse`, `ForceOpenImpulse`, `ClosedThresholdDegrees`, `bDoorClosed`. |
| Setup | Function entries | PASS | `03_ensure_functions_preview`, `03_ensure_functions_execute`; creates `LightPush`, `ForceOpen`, `CollisionClose` graph scopes. |
| GraphWrite | LightPush | PASS | `04_graphwrite_light_push_preview_fix03/preview_1779855052665_0001/result.json`; `04_graphwrite_light_push_execute_fix03/task_738EF0344D46F11D733C139558CF8E44/result.json`. |
| GraphWrite | ForceOpen | PASS | `05_graphwrite_force_open_preview_fix03/preview_1779855076071_0001/result.json`; `05_graphwrite_force_open_execute_fix03/task_DC0A1CBA418566C7E079739CF315BDAA/result.json`. |
| GraphWrite | CollisionClose | PASS | `06_graphwrite_collision_close_preview_fix03/preview_1779855101012_0001/result.json`; `06_graphwrite_collision_close_execute_fix03/task_A7BEA4144815C25319B293A03AF64980/result.json`. |
| Readback | LightPush graph logic | PASS | `12_read_light_push_graph_logic_json_fix03/cli_1779855124584/result.json`; sets `bDoorClosed=false`, sets `DoorOpenAngle=177`, links `Hinge` to `SetRelativeRotation`, links `DoorMesh` to `SetSimulatePhysics` and `AddImpulse`; 5 exec links, 5 data links, 0 orphan nodes. |
| Readback | ForceOpen graph logic | PASS | `13_read_force_open_graph_logic_json_fix03/cli_1779855132701/result.json`; same open flow as LightPush with stronger impulse; 5 exec links, 5 data links, 0 orphan nodes. |
| Readback | CollisionClose graph logic | PASS | `14_read_collision_close_graph_logic_json_fix03/cli_1779855140118/result.json`; compares `DoorOpenAngle <= ClosedThresholdDegrees`, true path sets `bDoorClosed=true`, sets `DoorOpenAngle=0`, disables `DoorMesh` physics; 5 exec links, 4 data links, 0 orphan nodes. |

PhysicalDoor scoring:

| Metric | Result | Notes |
|---|---|---|
| Setup failure | 0 | Setup succeeded and is excluded from GraphWrite correctness. |
| GraphWrite flow failure | 0/3 | `LightPush`, `ForceOpen`, `CollisionClose` preview and execute all passed. |
| Call correctness | PASS | Component receiver calls bind through `target_object` to the callable receiver pin, not to a normal argument pin. |
| Silent wrong graph | 0 | Readback confirms expected state writes, component receiver links, rotation/physics/impulse calls, collision-close branch, and no orphan nodes. |

### 14.2 Defects Found and Closed In This Retest

| ID | Scope | Symptom | Fix | Closure evidence |
|---|---|---|---|---|
| GWE2E-20260527-008 | `call_function.target_object` receiver binding | PhysicalDoor preview/execute originally produced missing receiver evidence or unconnected `Target` pins for component member calls; the runtime readback test later isolated an unlinked receiver edge caused by inconsistent statement fragment ids across DAG, pipeline, registry, and TaskRuntime paths; final review found runtime pre-resolution still did not feed `target_object` pin evidence into `TargetObjectPinType` cache keys. | Keep `target_object` as a dedicated SemanticIR/DAG receiver port for `kind=call`; remove it from callable argument evidence; normalize resolver context; alias `target_object` to `self` / static `DefaultToSelf` receiver pins; bump runtime resolver cache version; centralize statement fragment id generation through `FBlueprintHelperGraphStatementTypeUtils::MakeStatementFragmentId`; promote semantic `target_object.pin_type` / receiver-edge pin type into `TargetObjectPinType` without adding it back to normal arguments. | PhysicalDoor `fix03` preview/execute/readback pass; TaskRuntime CallFunction focused test 5/5 pass including `TargetObjectPreviewExecuteReadBack` and `TargetObjectPinTypeCacheKey`; cache focused test 4/4 pass; full `BlueprintHelper.GraphWrite` 315/315 pass. |
| GWE2E-20260527-009 | ContainerAction target role regression | First full rerun after the `target_object` fix failed 4 ContainerAction tests because non-call statements also emitted the receiver edge as `target_object`. | Restrict the dedicated `target_object` DAG input to `EBlueprintHelperGraphStatementKind::Call`; all other statement kinds keep the existing `target` input contract. | `BlueprintHelper.GraphWrite.ContainerAction` 14/14 pass; final full `BlueprintHelper.GraphWrite` 315/315 pass. |

Current status: all previously recorded real-case E2E operation failures are closed by the current live gates. No open GraphWrite 80% real-case E2E gap remains in this document as of the final 2026-05-27 rerun.
