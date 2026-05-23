# BlueprintHelper GraphWrite Architecture Gaps Audit 2026-05-22

## Current Conclusion

当前代码不能再标记为“所有 GraphWrite 架构 gap 均已消失”。本轮复核确认，旧 public API、旧 graph body、多数 legacy fallback 已经收敛，但仍有少量主路径边界没有完全符合 `BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md` 的架构口径。

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

## Open Gaps

### P6 Verification Blockers - 2026-05-22

Status update after regression fix: `BlueprintHelper.GraphWrite` now passes in `Saved/Automation/GraphWrite80_P6_GraphWrite_Regression_002/index.json` with 116 tests successful (`107` succeeded + `9` succeeded with warnings), 0 failed, and 0 not run. The blocker table below is retained as closed history for `_001`; it no longer blocks P6 closure.

P6 能力行本身已经闭环：`BlueprintHelper.GraphWrite.Capability80` 在 `Saved/Automation/GraphWrite80_P6_Full_002/index.json` 中为 5 succeeded / 0 failed；最终指标为 Capability coverage 5/5、GraphWrite correctness 17/18、Call correctness 8/8、Silent wrong graph 0。

`Saved/Automation/GraphWrite80_P6_GraphWrite_Regression_001/index.json` 曾记录 91 succeeded / 14 failed / 9 succeeded with warnings；这些条目已在 `_002` 中关闭。下表保留为 closed history，并继续说明不能用删除测试、伪造 evidence 或声明 unsupported 语义成功的方式关闭此类问题。

| Blocker | Exact file / entry | Why it blocks 80% capability/correctness closure | Why not completed/deleted in P6 | Next required plan |
|---|---|---|---|---|
| CLOSED: ActionResolution projected-context contract failure | Test: `BlueprintHelper.GraphWrite.ActionResolution.Contract.ClustersConsumeProjectedContext`; source entries reported under `Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/*.cpp` | Closed in `GraphWrite80_P6_GraphWrite_Regression_002`; cluster scan now exempts the reusable Context implementation boundary. | No longer a P6 blocker. | Keep future cluster files from rebuilding the Context pipeline directly. |
| CLOSED: FunctionAction operator and resolver stress failures | Tests: `BlueprintHelper.GraphWrite.ActionResolution.FunctionAction.OperatorDispatch`; `BlueprintHelper.GraphWrite.CallFunctionResolver.Stress.*`; `BlueprintHelper.GraphWrite.CallFunctionResolver.GeneratorDisplayNameSpawnsPrintString`; `BlueprintHelper.GraphWrite.CallFunctionResolver.GeneratorQualifiedNameSpawnsPrintString` | Closed in `GraphWrite80_P6_GraphWrite_Regression_002`; operator dispatch uses real UE spawner evidence, call query projection uses `target`, and supplemental-only Blueprint-authored tests now forbid success without spawner/action evidence. | No longer a P6 blocker. | Gap 5 remains separate for delegate/bind positive spawner support. |
| CLOSED: Direct append / replace service regression | Tests: `BlueprintHelper.GraphWrite.Append.OwnershipWritesMetadataWithoutManagedComment`; `BlueprintHelper.GraphWrite.Append.ReusesSignatureEntry`; `BlueprintHelper.GraphWrite.Replace.CustomEventBodyReconnectsEntryExec`; file: `BlueprintHelperGraphWriteToolResultBaseTests.cpp` | Closed in `GraphWrite80_P6_GraphWrite_Regression_002`; direct service payloads now follow valid current SemanticIR contracts and ownership metadata is asserted through signature-entry reuse where block refs are required. | No longer a P6 blocker. | Keep direct service tests aligned with supported SemanticIR entry/body contracts. |
| CLOSED AS P6 BLOCKER: EventDelegate declared capability contract mismatch | Test: `BlueprintHelper.GraphWrite.LegacyMainline.EventDelegateDeclaredCapabilityMatchesSuccessPath`; source: `Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp` | Closed in `GraphWrite80_P6_GraphWrite_Regression_002`; contract now matches P5 semantics and guards against fake delegate success. | Gap 5 remains open because component-bound/bind still return `unsupported_intent` when complete evidence is present. | Create a delegate/bind spawner-family plan before claiming positive component-bound/bind support. |

### Gap 2. GraphStatementBuilder still owns local demand and cluster projection logic

状态：未关闭

证据：
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
- 文件内仍存在 `BuildSingleActionContextDemand`。
- 文件内仍存在 `ResolveSpawnerClusterForSemanticKind`，维护 semantic kind 到 cluster kind 的本地映射。

为什么仍是 gap：
- 设计口径要求 GraphStatement / SemanticIR 表达语义需求，ActionContext Pipeline 统一收集 demand、snapshot、inference、projection。
- 当前实现已停止在 builder 内本地 `Build` scope，但 builder 仍保留局部 demand/projection 兜底逻辑，边界还没有完全收敛到 `ContextDemandCollector` / `BundleProjector`。

关闭条件：
- GraphStatementBuilder 不再构造本地 `FBlueprintHelperActionContextDemand`。
- semantic kind 到 cluster kind 的映射只保留在统一 collector/projector/resolver 边界。
- 契约测试覆盖 GraphStatementBuilder 不得出现 `BuildSingleActionContextDemand`、`ResolveSpawnerClusterForSemanticKind` 或直接拼接 request semantic/cluster 的路径。

### Gap 3. Canonical singleton direct spawn boundary is not explicit enough

状态：已关闭（仅限 canonical singleton direct spawn boundary）

Closure scope - 2026-05-23:
- CLOSED only for canonical singleton direct spawn boundary behind `FBlueprintHelperSingletonControlFlowEvidenceProvider`.
- `FBlueprintHelperSingletonControlFlowEvidenceProvider::TryBuildCanonicalRequest` owns singleton kind -> semantic kind/query mapping.
- `FBlueprintHelperSingletonControlFlowEvidenceProvider::ResolveCanonical` owns `ActionResolutionResult`, stable id, candidate evidence, and `SelectedSpawner` creation.
- `GenericAssetStructControlActionResolver` continues to call the provider for `Select` / `Control`.
- Wide-surface semantics still cannot use singleton direct spawn as fallback; broad create/convert/schedule semantics remain outside this closure.

Closure evidence:
- Task 1 API green: `D:\UEProjects\Template\Saved\Automation\GraphWrite_SingletonBoundary_API_GREEN_002\index.json`, 1 succeeded, 0 failed, 0 warnings.

当前结论：
- `branch`、`sequence`、`return`、以及确认唯一的 `select` 属于 canonical singleton semantic。
- direct spawn 只发生在 `FBlueprintHelperSingletonControlFlowEvidenceProvider` 的 canonical secondary semantic mapping 内。
- 该闭环不改变 `SpawnerClusterKind` 一级分发规则，也不关闭 Generic broad create/convert/schedule 语义缺口。
- Gap 2 和 Gap 5 仍按各自章节保持开放，除非后续文档另行记录。

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
- Gap 4 已关闭；不代表 Generic broad create/convert/schedule 语义完成。

### Gap 5. EventDelegate component-bound / bind complete spawner boundary is unresolved

Status: open after P5.

Evidence:
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperEventDelegateActionCluster.cpp`
- `ComponentBoundEvent` and `Bind` are now owned only far enough to return precise missing-evidence diagnostics.
- When component/binding object, delegate name, and delegate signature evidence are complete, the cluster returns `unsupported_intent` instead of constructing a spawner.

Why this remains a gap:
- P5 confirmed custom events through `UBlueprintEventNodeSpawner`, but did not establish a safe UE spawner-family route for `UK2Node_ComponentBoundEvent`, bind/assign delegate nodes, and their required component/delegate signature fields.
- Claiming success without a positive `SelectedSpawner != null` automation test would overstate delegate support and risk a hardcoded or legacy fallback path.

Close conditions:
- Resolve the correct UE spawner family and required evidence model for component-bound events and delegate bind/assign.
- Add positive automation with complete projected evidence, `SelectedSpawner != null`, stable id/candidate evidence, and no fallback success.
- Keep missing-evidence diagnostics (`component_missing`, `binding_object_missing`, `delegate_signature_missing`) passing.

## Removed From Gap List

以下项目在本轮复核中不再作为未关闭 gap 保留：

- Public GraphWrite API 不再暴露 `FParsedNode`、`FParsedLink`、`FParsedMacroReference`、`SpawnMacroNode`。
- Public GraphWrite pipeline headers 中不再保留旧 parsed-node parser/linker/spawner/mutation plan surface。
- AgentFace GraphWrite public statement surface 已收敛为 `call`、`set`、`set_property`、`let`、`control`。
- `branch`、`sequence`、`return` 已从 Agent-facing statement kind surface 移除，仅作为 compiler-owned internal body shape。
- Agent guide、Codex reference、Claude reference 已同步为 `kind:"call"` + `target`，`call_function.name` 仅作为 unsupported legacy 说明保留。
- 指定 ActionResolution / FunctionResolution / GraphStatement 范围未再发现 `TObjectIterator<UClass>`、`TObjectIterator<UScriptStruct>`、`TObjectIterator<UFunction>`。
- resolvable expression 路径未再发现 `expr_call`、`expr_op`、`expr_construct`、`expr_deconstruct` placeholder token。
- EventDelegate cluster 不再声明尚无成功路径闭环的 `ComponentBoundEvent` / `Bind` 为已支持能力。
- parsed-node mutation plan 已隔离到 private legacy pipeline，执行入口对 parsed-node node plan fail-fast 为 `parsed_node_plan_unsupported`。


- **Gap 1 (closed in P1):** `ControlFragmentBuilder` manual control/manual semantic fallback has been removed from `BlueprintHelperControlFragmentBuilder.cpp`.
- When `ActionContextScope` is missing it now returns `action_context_scope_required` and does not synthesize `manual_control_context` / `manual_control_semantic`.
- Control request path now uses `ActionRequest.ClusterKind` and `ActionRequest.Semantic.Kind` with bundle/projector/evidence flow.
## Last Verification Scope

本次同步基于静态复核和子代理只读审计结果，不等同于重新完成 UE 编译或 Editor Bridge smoke。

已复核范围：
- `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/**`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/**`
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/**`
- `AgentFaceService/task-core/**`
- `AgentFaceService/agent-guide/**`
- `CodexPlugin/skills/blueprint-helper/references/**`
- `ClaudePlugin/skills/blueprint-helper/references/**`

关闭全部 gap 前必须重新运行：

```powershell
npm.cmd run build
npm.cmd run test:node
python -m unittest discover -s python/tests -t python
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
```

如涉及运行时 GraphWrite 行为，还必须补充 Editor Bridge preview smoke，确认 public `kind:"control"` 输入在 TaskPlan/Bridge 中 lower 到 `BlueprintLogicSpec.v2` 的 internal `sequence` / `branch` / `return` body shape，并且不会触发 `statement_kind_unsupported`。
