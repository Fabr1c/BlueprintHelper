# BlueprintHelper GraphWrite Architecture Gaps Audit 2026-05-22

## Current Conclusion

2026-05-24 update: the original audit Gap 1-5 records remain closed for their historical scope, but this file now also tracks follow-up Gap 3/4/5 entries added below. Do not read the historical closure statement as full four-cluster completion.

当前代码在本审计文档列出的 GraphWrite 架构 gap 已全部关闭。该结论只表示本文档中的 Gap 1-5 已按各自边界收敛，不等同于四个工具簇全部达到“完全完成”；`asset_action` create、type-promotion spawner projection、timer/delegate/latent node evidence、Field real Bridge execute smoke、Event Signature collaboration smoke 和 Function shared adapter/lifecycle 仍按后续路线图继续推进。

当前期望主链路仍然是：

```text
TaskSpec GraphBody
-> SemanticIR
-> ActionContextPipeline
-> ActionResolutionCore
-> UE NodeSpawner evidence
-> shared adapter
-> FragmentDAG
-> Composer/Linker/MutationCoordinator
-> UE Mutator
```

## Legacy 删除门禁

后续 GraphWrite 80% 能力推进采用 legacy 严格模式：只要代码路径保留旧语义、旧 fallback、旧模型、旧 transaction/review 解释，默认视为 legacy，不能复用进新主链路。

判定为旧路径后默认直接删除。若暂时不能删除，必须在本 gap 文档中保留一条明确记录，包含：

1. 旧路径文件与入口。
2. 当前仍不能删除的具体原因。
3. 它为什么不能进入新主链路。
4. 删除前置条件。

### P1 Legacy deletion gate evidence - 2026-05-22

| Legacy path | Why it cannot be deleted in P1 | Why it cannot enter the new mainline | Deletion prerequisite |
|---|---|---|---|
| `BlueprintGraphJsonParser.h/.cpp` | `BlueprintGraphMutationPlanBuilder.cpp`, `BlueprintGraphGenerationPipeline.cpp`, `BlueprintGraphDefaultValueApplier.cpp`, `BlueprintGraphExistingNodeMapper.cpp`, `BlueprintGraphLocalVariableService.cpp`, `BlueprintGraphLinker.cpp`, `BlueprintGraphNodeUtility.cpp`, and `BlueprintMultiGraphGenerationPipeline.cpp` still include or depend on parser/parsed DTO types. | It preserves JSON parsed-node DTO parsing, including `make_struct` / `compare` legacy tokens; P1 kept it isolated because removing it would require replacing the private mutation-plan/parser contract. | Migrate remaining private parser consumers to SemanticIR/action-context request builders, then delete parser functions and parsed DTO references. |
| `BlueprintGraphLinker.h/.cpp` | `BlueprintGraphGenerationPipeline.cpp` still uses `ConnectFragmentDataEdges` for the SemanticIR FragmentDAG mainline, while parsed-link overloads are still referenced by `BlueprintGraphMutationPlanExecutor.cpp`. | The file mixes the new FragmentDAG data-edge linker with legacy `FParsedLink` explicit-link overloads; only `ConnectFragmentDataEdges` is mainline-safe. | Split/rename the FragmentDAG linker boundary, migrate executor parsed-link paths to fail-fast without linker overloads, then delete legacy overloads. |
| `FParsedNode` / `FParsedLink` private DTOs | `BlueprintGraphMutationPlan.h`, `BlueprintGraphMutationPlanBuilder.h/.cpp`, `BlueprintGraphMutationPlanExecutor.cpp`, `BlueprintGraphJsonParser.cpp`, and `BlueprintGraphLinker.h/.cpp` still compile-use the parsed DTOs. | Parsed DTOs represent the legacy graph-json pipeline, not `TaskSpec GraphBody -> SemanticIR -> ActionContextPipeline`. P1 keeps execution fail-fast via `parsed_node_plan_unsupported`. | Remove parsed-node mutation-plan builder/executor entry points or replace them with SemanticIR request models. |
| `BlueprintGraphNodeSpawner.h/.cpp` | Deleted in P1 after stale include cleanup; no remaining `BlueprintGraphNodeSpawner` or `SpawnMacroNode` source reference. | N/A. | Closed for P1. |
| `BlueprintHelperGenericAssetStructControlActionResolver.cpp` direct `make_struct` tokens | Kept because direct struct spawning is P4/P5 scope and already lives under GenericAssetStructControlAction resolver evidence, not the removed manual control fallback. | It must not become a fallback for wide-surface semantics without ActionContext/Generic cluster evidence. | P4/P5 should document or reshape the direct struct boundary and remove/rename `make_struct` search-gate tokens if the boundary is accepted. |
| `call_function.name` diagnostic strings | Kept in `BlueprintHelperCallFunctionResolver.cpp` and `BlueprintHelperMergeBlueprintGraphService.cpp` as failure/diagnostic messages only. | These strings do not create success without resolver evidence. | Rename diagnostics only if future gates require zero string matches. |

## Closed / Historical Gap Records

### P6 Verification Blockers - 2026-05-22

Status update after regression fix: `BlueprintHelper.GraphWrite` now passes in `Saved/Automation/GraphWrite80_P6_GraphWrite_Regression_002/index.json` with 116 tests successful (`107` succeeded + `9` succeeded with warnings), 0 failed, and 0 not run. The blocker table below is retained as closed history for `_001`; it no longer blocks P6 closure.

P6 能力行本身已经闭环：`BlueprintHelper.GraphWrite.Capability80` 在 `Saved/Automation/GraphWrite80_P6_Full_002/index.json` 中为 5 succeeded / 0 failed；最终指标为 Capability coverage 5/5、GraphWrite correctness 17/18、Call correctness 8/8、Silent wrong graph 0。

`Saved/Automation/GraphWrite80_P6_GraphWrite_Regression_001/index.json` 曾记录 91 succeeded / 14 failed / 9 succeeded with warnings；这些条目已在 `_002` 中关闭。下表保留为 closed history，并继续说明不能用删除测试、伪造 evidence 或声明 unsupported 语义成功的方式关闭此类问题。

| Blocker | Exact file / entry | Why it blocks 80% capability/correctness closure | Why not completed/deleted in P6 | Next required plan |
|---|---|---|---|---|
| CLOSED: ActionResolution projected-context contract failure | Test: `BlueprintHelper.GraphWrite.ActionResolution.Contract.ClustersConsumeProjectedContext`; source entries reported under `Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/*.cpp` | Closed in `GraphWrite80_P6_GraphWrite_Regression_002`; cluster scan now exempts the reusable Context implementation boundary. | No longer a P6 blocker. | Keep future cluster files from rebuilding the Context pipeline directly. |
| CLOSED: FunctionAction operator and resolver stress failures | Tests: `BlueprintHelper.GraphWrite.ActionResolution.FunctionAction.OperatorDispatch`; `BlueprintHelper.GraphWrite.CallFunctionResolver.Stress.*`; `BlueprintHelper.GraphWrite.CallFunctionResolver.GeneratorDisplayNameSpawnsPrintString`; `BlueprintHelper.GraphWrite.CallFunctionResolver.GeneratorQualifiedNameSpawnsPrintString` | Closed in `GraphWrite80_P6_GraphWrite_Regression_002`; operator dispatch uses real UE spawner evidence, call query projection uses `target`, and supplemental-only Blueprint-authored tests now forbid success without spawner/action evidence. | No longer a P6 blocker. | Keep function-class spawner invocation aligned with shared adapter/lifecycle evidence. |
| CLOSED: Direct append / replace service regression | Tests: `BlueprintHelper.GraphWrite.Append.OwnershipWritesMetadataWithoutManagedComment`; `BlueprintHelper.GraphWrite.Append.ReusesSignatureEntry`; `BlueprintHelper.GraphWrite.Replace.CustomEventBodyReconnectsEntryExec`; file: `BlueprintHelperGraphWriteToolResultBaseTests.cpp` | Closed in `GraphWrite80_P6_GraphWrite_Regression_002`; direct service payloads now follow valid current SemanticIR contracts and ownership metadata is asserted through signature-entry reuse where block refs are required. | No longer a P6 blocker. | Keep direct service tests aligned with supported SemanticIR entry/body contracts. |
| CLOSED AS P6 BLOCKER: EventDelegate declared capability contract mismatch | Test: `BlueprintHelper.GraphWrite.LegacyMainline.EventDelegateDeclaredCapabilityMatchesSuccessPath`; source: `Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp` | Closed in `GraphWrite80_P6_GraphWrite_Regression_002`; contract now matches P5 semantics and guards against fake delegate success. | Gap 5 is now separately closed by the 2026-05-23 EventDelegate use-site implementation. | Keep declared-capability tokens synchronized with resolver and fragment readback evidence. |

### Gap 2. GraphStatementBuilder still owns local demand and cluster projection logic

状态：已关闭（仅限 GraphStatementBuilder 本地 demand / semantic-cluster projection 职责收敛）

Closure scope - 2026-05-23:
- `BlueprintHelperGraphStatementBuilder.cpp` no longer defines `BuildSingleActionContextDemand`.
- `BlueprintHelperGraphStatementBuilder.cpp` no longer defines or calls `ResolveSpawnerClusterForSemanticKind`.
- Single-demand construction is owned by `FBlueprintHelperActionContextDemandCollector::BuildSingleDemand`.
- Semantic kind -> cluster kind mapping remains inside `FBlueprintHelperActionContextDemandCollector::ApplyDemandKinds`.
- GraphStatementBuilder still consumes `FBlueprintHelperActionContextScope::TryBuildRequest`; request projection remains owned by ActionContext bundle projection.

Closure evidence:
- `D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap2_SourceContract_GREEN_001\index.json`, 1 succeeded, 0 failed, 0 warnings.
- `D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap2_SingleDemand_GREEN_002\index.json`, 2 succeeded, 0 failed, 0 warnings.
- `D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap2_ActionContext_GREEN_001\index.json`, 11 succeeded, 0 failed, 0 warnings.
- `D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap2_Regression_001\index.json`, 108 succeeded, 10 succeeded with warnings, 0 failed, 0 not run.
- UE 5.6 `Build.bat TemplateEditor Win64 Development`, `Result: Succeeded`.
- `git diff --check`, exit code 0.

当前结论：
- Gap 2 已关闭；GraphStatementBuilder 不再拥有本地 action-context demand 构造或 semantic kind 到 cluster kind 的局部投影职责。
- 该闭环不改变四簇语义能力状态；Gap 5 已由 2026-05-23 EventDelegate use-site implementation 另行关闭。
- At this Gap 2 closure point, wide `create/convert/schedule` semantics were outside scope; later 2026-05-24 closure records below cover explicit broad-create first slice and Generic Convert/Schedule ownership boundary.

### Gap 3. Canonical singleton direct spawn boundary is not explicit enough

状态：已关闭（仅限 canonical singleton direct spawn boundary）

Closure scope - 2026-05-23:
- CLOSED only for canonical singleton direct spawn boundary behind `FBlueprintHelperSingletonControlFlowEvidenceProvider`.
- `FBlueprintHelperSingletonControlFlowEvidenceProvider::TryBuildCanonicalRequest` owns singleton kind -> semantic kind/query mapping.
- `FBlueprintHelperSingletonControlFlowEvidenceProvider::ResolveCanonical` owns `ActionResolutionResult`, stable id, candidate evidence, and `SelectedSpawner` creation.
- `GenericAssetStructControlActionResolver` continues to call the provider for `Select` / `Control`.
- Wide-surface semantics still cannot use singleton direct spawn as fallback; at this closure point, broad create/convert/schedule semantics remained outside this closure and were tracked by later 2026-05-24 records below.

Closure evidence:
- Task 1 API green: `D:\UEProjects\Template\Saved\Automation\GraphWrite_SingletonBoundary_API_GREEN_002\index.json`, 1 succeeded, 0 failed, 0 warnings.

当前结论：
- `branch`、`sequence`、`return`、以及确认唯一的 `select` 属于 canonical singleton semantic。
- direct spawn 只发生在 `FBlueprintHelperSingletonControlFlowEvidenceProvider` 的 canonical secondary semantic mapping 内。
- 该闭环不改变 `SpawnerClusterKind` 一级分发规则；当时未关闭的 Generic broad create/convert/schedule 后续由 2026-05-24 closure records 分别补充记录。
- Gap 2 已按本地 demand/projection 职责收敛关闭；Gap 5 已由 2026-05-23 EventDelegate use-site implementation 另行关闭。

### Gap 4. MutationCoordinator bypasses singleton evidence boundary for sequence node

状态：已关闭

Closure evidence - 2026-05-23:
- `BlueprintHelperGraphWriteMutationCoordinator.cpp` no longer constructs singleton semantic request/query/hash details.
- Mutation code requests `EBlueprintHelperSingletonControlFlowKind::Sequence` via `FBlueprintHelperSingletonControlFlowEvidenceProvider::ResolveCanonical`.
- Mutation still uses `FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner`.
- `BlueprintHelper.GraphWrite.TaskRuntime.Merge.BranchForkOwnedBlockCallReadBack` confirms the branch-fork sequence exists and is explainable by singleton provider evidence.
- Task 2 contract green: `D:\UEProjects\Template\Saved\Automation\GraphWrite_SingletonBoundary_MutationContract_GREEN_001\index.json`, 1 succeeded, 0 failed.
- Task 3 runtime green: `D:\UEProjects\Template\Saved\Automation\GraphWrite_SingletonBoundary_BranchForkRuntime_GREEN_001\index.json`, 0 failed, succeededWithWarnings=1 due existing asset load warnings.

当前结论：
- `sequence` 仍是 canonical singleton control node，direct spawn 策略合法，但策略所有权不再位于 MutationCoordinator。
- branch fork 所需 sequence node 创建复用 singleton provider evidence，并通过 shared spawner adapter 实际生成节点。
- candidate/debug/review evidence 能解释该 sequence node 是 mutation 编排产物，同时其 spawner strategy 来自统一 singleton boundary。
- Gap 4 已关闭；当时不代表 Generic broad create/convert/schedule 语义完成，后续状态以 2026-05-24 closure records 为准。

### Gap 5. EventDelegate use-site complete spawner boundary

Status: closed - 2026-05-23.

Closure scope:
- `component_bound_event` resolves through first-stage `ComponentBoundEvent` with component/delegate projected evidence and `UBlueprintBoundEventNodeSpawner`.
- `delegate.bind`, `delegate.assign`, `delegate.unbind`, `delegate.unbind_all`, and `delegate.call` resolve through first-stage `Delegate` plus second-stage `delegate_operation`.
- `delegate.unbind` and `delegate.unbind_all` remain explicit; missing handler evidence for `delegate.unbind` does not downgrade to clear/unbind-all.
- GraphWrite/EventDelegate writes use-site nodes only. It does not call or duplicate Signature-owned `ensure_function`, `ensure_custom_event`, `ensure_event_dispatcher`, or `ensure_override_event`.
- Handler declarations/signatures remain Signature-owned; GraphWrite only references existing projected handler evidence.

Implementation notes:
- `delegate.assign` is intentionally not a normal `SelectedSpawner != null` path because UE's AssignDelegate spawner can auto-create a `UK2Node_CustomEvent`. The resolver returns a manual assign-factory candidate (`ue_delegate_manual_assign_factory`) and the fragment builder constructs `UK2Node_AssignDelegate` without `PostPlacedNewNode`.
- Non-assign delegate operations use `UBlueprintDelegateNodeSpawner`; component-bound events use `UBlueprintBoundEventNodeSpawner`; all use-site emission goes through the GraphStatement fragment boundary.

Closure evidence:
- `D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_EventDelegateResolver_GREEN_002\index.json`, 10 succeeded, 0 failed, 0 not run.
- `D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_EventDelegateFragment_GREEN_002\index.json`, 6 succeeded, 0 failed, 0 not run.
- `D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_ActionContextAll_GREEN_001\index.json`, 13 succeeded, 0 failed, 0 not run.
- `D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_SourceContract_GREEN_001\index.json`, 5 succeeded, 0 failed, 0 not run.
- `D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_LegacyMainline_GREEN_001\index.json`, 8 succeeded, 0 failed, 0 not run.
- `D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_Capability80_GREEN_001\index.json`, 5 succeeded, 0 failed, 0 not run.
- `D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_Regression_FINAL_001\index.json`, 122 succeeded, 10 succeeded with warnings, 0 failed, 0 not run.
- AgentFace Python tests: 57 OK.
- AgentFace Node tests: 150 pass, 0 fail.
- UE 5.6 `Build.bat TemplateEditor Win64 Development`, `Result: Succeeded`.

## Removed From Gap List

以下项目在本轮复核中不再作为未关闭 gap 保留：

- Public GraphWrite API 不再暴露 `FParsedNode`、`FParsedLink`、`FParsedMacroReference`、`SpawnMacroNode`。
- Public GraphWrite pipeline headers 中不再保留旧 parsed-node parser/linker/spawner/mutation plan surface。
- AgentFace GraphWrite public statement surface 已收敛为 `call`、`set`、`set_property`、`let`、`control`。
- `branch`、`sequence`、`return` 已从 Agent-facing statement kind surface 移除，仅作为 compiler-owned internal body shape。
- Agent guide、Codex reference、Claude reference 已同步为 `kind:"call"` + `target`，`call_function.name` 仅作为 unsupported legacy 说明保留。
- 指定 ActionResolution / FunctionResolution / GraphStatement 范围未再发现 `TObjectIterator<UClass>`、`TObjectIterator<UScriptStruct>`、`TObjectIterator<UFunction>`。
- resolvable expression 路径未再发现 `expr_call`、`expr_op`、`expr_construct`、`expr_deconstruct` placeholder token。
- EventDelegate cluster 不再声明无成功路径闭环的 component-bound / delegate use-site 语义为已支持能力；Gap 5 closure 后仅声明已有 resolver/fragment/readback 证据的 use-site 语义。
- parsed-node mutation plan 已隔离到 private legacy pipeline，执行入口对 parsed-node node plan fail-fast 为 `parsed_node_plan_unsupported`。


- **Gap 1 (closed in P1):** `ControlFragmentBuilder` manual control/manual semantic fallback has been removed from `BlueprintHelperControlFragmentBuilder.cpp`.
- When `ActionContextScope` is missing it now returns `action_context_scope_required` and does not synthesize `manual_control_context` / `manual_control_semantic`.
- Control request path now uses `ActionRequest.ClusterKind` and `ActionRequest.Semantic.Kind` with bundle/projector/evidence flow.
## Last Verification Scope

本次同步基于单线程实现、源码 contract、AgentFace 测试、UE 5.6 编译和 Unreal Automation 复核；未执行 Editor Bridge preview smoke。

已复核范围：
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/**`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/**`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/**`
- `AgentFaceService/task-core/**`
- `AgentFaceService/agent-guide/**`
- `CodexPlugin/skills/blueprint-helper/references/**`
- `ClaudePlugin/skills/blueprint-helper/references/**`

本轮已运行：

```powershell
npm.cmd run build
npm.cmd run test:node
python -m unittest discover -s python/tests -t python
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_Gap5_Regression_FINAL_001'
```

如涉及运行时 GraphWrite 行为，还必须补充 Editor Bridge preview smoke，确认 public `kind:"control"` 输入在 TaskPlan/Bridge 中 lower 到 `BlueprintLogicSpec.v2` 的 internal `sequence` / `branch` / `return` body shape，并且不会触发 `statement_kind_unsupported`。

## Open Follow-up Gaps Added 2026-05-24

The historical Gap 1-5 records above remain closed for their original scope. The following entries are new follow-up gaps from the 2026-05-24 main-thread truth audit and must be tracked separately until code, tests, Bridge smoke, docs, and UE 5.6 compile agree.

| Follow-up gap | Status | Exact scope | Evidence found | Required closure |
|---|---|---|---|---|
| Gap 3: Function shared adapter / lifecycle convergence | OPEN | Function-class spawner invocation still needs a stronger shared adapter/lifecycle boundary beyond the current resolver-level convergence. | `FBlueprintHelperFunctionSemanticActionResolver` reuses the call resolver and `FBlueprintHelperActionNodeSpawnerAdapter` exists, but `BlueprintHelperGraphStatementBuilder.cpp` still owns much of the call fragment spawning/lifecycle orchestration. | Add focused plan and implementation that moves Function call/convert/schedule spawn lifecycle into a reusable adapter/coordinator boundary, with source contract and runtime tests proving GraphStatementBuilder only composes fragments from projected evidence. |
| Gap 4: Field real Bridge execute smoke | OPEN | Field complex property path, linked typed pin inference, `component_ref`, and `field_access` have C++ and compiler coverage, but the real CLI/Bridge execute smoke is blocked by missing fixture asset. | `Saved/BlueprintHelper/Cli/preview_1779602466387_0001/result.json` reports `target_blueprint_not_found`; `Saved/BlueprintHelper/Cli/cli_1779602489237/result.json` reports `task_preview_blocked` and `execute_task did not write assets`. | Create or generate `/Game/BlueprintHelper/Smoke/BP_GraphWriteFunctionFieldSmoke`, rerun preview and execute through BlueprintHelper CLI/Bridge, verify asset write/readback, and record passing artifact paths. |
| Gap 5: Event Signature collaboration smoke / taxonomy boundary | OPEN | GraphWrite/EventDelegate must keep `custom_event`, `override_event`, and `native_event` declaration lifecycle Signature-owned, but broader Signature-to-GraphWrite collaboration still needs real execute smoke and existing-use-site asset coverage. | Contract tests forbid `ensure_custom_event`, `ensure_override_event`, and `native_event` lifecycle in EventDelegate source; compiler keeps `blueprint_signature.ensure_custom_event -> graph_write` dependency. | Add a Signature-owned declaration plus GraphWrite body/use-site execute smoke covering custom event, override/native event declaration, and delegate use-site references without moving declaration taxonomy into GraphWrite/EventDelegate. |

## 2026-05-24 Generic Broad Create Closure

- Closed for explicit broad-create first slice: `spawn_actor`, `create_widget`, `construct_object`, `make_array`, `make_map`, `make_set`.
- Closure evidence: AgentFace TS/Python compiler tests, UE 5.6 compile, `BlueprintHelper.GraphWrite.ActionResolution.Contract`, `BlueprintHelper.GraphWrite.ActionResolution.Generic.Create`, `BlueprintHelper.GraphWrite.ActionResolution.Generic`, `BlueprintHelper.GraphWrite.LegacyMainline`, and full `BlueprintHelper.GraphWrite` automation.
- `asset_action` remains intentionally open unless projected ActionDatabase / selected spawner evidence exists; current behavior is `needs_more_semantic_context`, not fake success.
- This closure does not change the open follow-up Gap 3/4/5 entries above.

## 2026-05-24 Generic Convert/Schedule Closure

- Closed for explicit Generic-side ownership boundary: `Convert + transform_operation` and `Schedule + schedule_operation` now route through `GenericAssetStructControlActionCluster` instead of FunctionAction or struct fallback.
- Function-owned `convert_function`, `schedule_function`, and `latent_or_async_function` remain in `FunctionActionCluster`.
- `dynamic_cast` and `class_cast` can resolve to cast-node spawner evidence when target class evidence is present.
- `type_promotion`, `timer_delegate_node`, and `latent_or_async_node` remain honest missing-context paths until projected type-promotion, timer/delegate, or latent node spawner evidence exists; current behavior is `needs_more_semantic_context`, not fake success.
- Ambiguous Function+Generic second-stage ownership is rejected with `ambiguous_convert_schedule_owner`.
- Closure evidence: AgentFace TS/Python compiler tests, UE 5.6 compile, `BlueprintHelper.GraphWrite.ActionResolution`, `BlueprintHelper.GraphWrite.ActionContext`, and full `BlueprintHelper.GraphWrite` automation; latest full report is `Saved/Automation/GraphWrite_GenericConvertSchedule_Final_20260524_001/index.json` with 155 succeeded, 11 succeeded with warnings, 0 failed, 0 not run.
- This closure does not change the open follow-up Gap 3/4/5 entries above.

## 2026-05-24 Struct / TypeStructure construct-deconstruct gap sync

- [x] 已关闭：`construct/deconstruct` taxonomy 不再依赖 broad `create` 或旧 `make_struct/break_struct` AgentFace token；当前源码 GraphWrite active path grep `make_struct|break_struct|create_operation` 无命中。
- [x] 已关闭：`GenericAssetStructControlActionResolver` 不再承载 construct/deconstruct 旧逻辑，当前仅保留 select/control NodeSpawner candidate 入口。
- [x] 已关闭：新增 `FBlueprintHelperStructTypeStructureActionResolver`，以 `SemanticFamily=Struct|TypeStructure` + `TypeOperation=Construct|Deconstruct` 解析 UE evidence。
- [x] 已验证：construct/deconstruct Vector CLI preview/execute/compile 均通过。
- [ ] 剩余差距：linked typed pin 推断未做独立 smoke；当前验证覆盖显式 type 与 missing-type 阻断。
- [ ] 剩余差距：ambiguity candidate list 未做独立 smoke；当前验证覆盖唯一成功和 `needs_more_semantic_context`。
