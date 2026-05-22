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

## Open Gaps

### Gap 1. ControlFragmentBuilder still has manual ActionRequest fallback

状态：未关闭

证据：
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFragmentBuilder.cpp`
- `ResolveControlActionProvider` 在缺少 `ActionContextScope` 时仍手工填充 `FBlueprintHelperActionResolutionRequest`。
- 该 fallback 写入 `manual_control_context:` 和 `manual_control_semantic:`，绕过了完整的 ActionContext projection。

为什么仍是 gap：
- 设计口径要求缺少 projected context 时 fail-fast，而不是在 builder 内修补一个身份不完整的 request。
- 这与 `ActionContextPipeline -> BundleProjector -> ActionResolutionCore` 的单一路径不一致。

关闭条件：
- `ControlFragmentBuilder` 缺少 `ActionContextScope` 时返回明确错误，例如 `action_context_scope_required`。
- 移除 `manual_control_context` / `manual_control_semantic` fallback。
- 增加契约测试，禁止 control builder 手工拼 `ActionRequest.ClusterKind`、`ActionRequest.Semantic.Kind` 或 manual context hash。

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

状态：未关闭

证据：
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionResolver.cpp`
- `MakeGenericNodeSpawnerResult` 通过 `UBlueprintNodeSpawner::Create(NodeClass)` 创建 spawner。
- `ResolveSelectNodeSpawner` 和 `ResolveControlNodeSpawner` 直接传入 `UK2Node_Select::StaticClass()`、`UK2Node_IfThenElse::StaticClass()`、`UK2Node_FunctionResult::StaticClass()`、`UK2Node_ExecutionSequence::StaticClass()`。

为什么仍是 gap：
- direct spawn 本身不是问题。`branch`、`sequence`、`return` 这类 canonical singleton control node 没有宽候选空间，不需要像 `call` / `op` / property / delegate 这类 wide-surface semantic 一样完整依赖 ActionDatabase 搜索链路。
- 当前问题是 direct spawn boundary 没有被文档化为 GenericAssetStructControlActionCluster 内部的二级语义映射策略，容易被误解为绕过一级分发。
- direct spawn 不能改变 `SpawnerClusterKind` 一级分发规则，也不能成为 wide-surface semantic 的搜索失败 fallback。

关闭条件：
- 明确 `branch`、`sequence`、`return`、以及确认唯一的 `select` 属于 canonical singleton semantic。
- direct spawn 只能发生在 `SpawnerClusterResolver` 按 `GenericAssetStructControlAction` 完成一级分发之后，作为 cluster 内部的二级 semantic mapping。
- direct spawn 必须返回统一 `ActionResolutionResult` / spawner evidence，包含 semantic kind、singleton kind、node class path、stable id、reason。
- 契约测试覆盖：builder / pipeline / mutation coordinator 不能直接发起 singleton direct spawn；wide-surface semantic 不能把 direct spawn 当 fallback。

### Gap 4. MutationCoordinator bypasses singleton evidence boundary for sequence node

状态：未关闭

证据：
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationCoordinator.cpp`
- `SpawnSequenceNode` 直接调用 `UBlueprintNodeSpawner::Create(UK2Node_ExecutionSequence::StaticClass())`。
- `ApplyBranchForkSemanticBody` 使用该 helper 创建 merge sequence node。

为什么仍是 gap：
- `sequence` 是 canonical singleton control node，可以 direct spawn；问题不在 direct spawn 这个策略，而在 MutationCoordinator 自己拥有了该策略。
- MutationCoordinator 应只负责 mutation orchestration，不能绕过 `GenericAssetStructControlActionCluster -> singleton semantic mapping -> ActionResolutionResult/evidence -> shared adapter` 的统一边界。

关闭条件：
- branch fork 所需 sequence node 创建必须复用 Generic cluster 的 singleton evidence，或调用一个明确的 singleton spawner evidence provider。
- candidate/debug/review evidence 能解释该 sequence node 是 mutation 编排产物，同时其 spawner strategy 来自统一 singleton boundary。
- 增加契约测试，避免 mutation coordinator 新增裸 `UK2Node_*` direct create path。

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
