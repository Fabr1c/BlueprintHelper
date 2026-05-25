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
