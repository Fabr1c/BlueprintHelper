# BlueprintHelper GraphWrite Architecture Gaps Audit 2026-05-22

## 审计范围

本次审计只读检查 GraphWrite 与当前架构口径的偏差：

- ActionResolution 一级请求只能按 `SpawnerClusterKind` 分发。
- AgentFace semantic kind 只能作为 `SemanticConstraints`。
- 主路径应为 `SemanticIR -> ActionContextPipeline -> ActionResolutionCore -> UE NodeSpawner evidence -> shared adapter -> FragmentDAG -> Composer/Linker -> UE Mutator`。
- 不应继续保留旧 `ParsedNode` 主路径、旧 fallback、旧 handler、旧 AgentFace shape、直接 `UK2Node_*` 创建或局部硬编码分支。

## P0 差距

### 1. Merge 服务仍未走统一 GraphWrite 主链

位置：`BlueprintHelperMergeBlueprintGraphService.cpp`

证据：`ExecuteWrite`、`ResolveInsertedLogic`、`ApplyAppendAfter`、`ApplyInsertBetween`、`ApplyBranchFork` 仍直接创建 `UK2Node_CallFunction*` 并手工连线。

偏差：绕过 `ActionContextPipeline -> ActionResolutionCore -> NodeFragment -> FragmentDAG -> Composer/Linker`。

期望：Merge 应输入语义意图，由统一 GraphStatement/FragmentDAG/Composer 执行插入、分支与重连。

### 2. Patch 服务仍直接变更 UE 图元

位置：`BlueprintHelperPatchBlueprintGraphService.cpp`

证据：`ApplyPatch`、`ApplySetPinDefault`、`ApplyConnectPins`、`ApplyDisconnectLink`、`ApplyReplaceLink` 直接 `Modify`、`Set`、`BreakLinkTo`、`TryCreateConnection`。

偏差：Patch 没走统一 coordinator/mutator，Review/Debug/evidence 语义存在继续分裂风险。

期望：引入 PatchCoordinator，将 patch type 映射为标准化操作意图，再由 GraphLinker/Composer 执行。

### 3. Select 仍直接绑定 `UK2Node_Select`

位置：`BlueprintHelperSelectFragmentBuilder.cpp`

证据：`Build` 中直接 `UBlueprintNodeSpawner::Create(UK2Node_Select::StaticClass())`。

偏差：绕过 `GenericAssetStructControlActionCluster` 的 resolver/evidence 选择。

期望：`select` 构造 `SemanticKind=Select` 的 ActionResolution request，由 Generic cluster 返回 spawner evidence，再通过 shared adapter invoke。

### 4. Control flow 仍是专用 builder + 本地伪 ActionResult

位置：`BlueprintHelperControlFragmentBuilder.cpp`

证据：`Branch / Return / Sequence` 经 `RequireDedicatedControlBuilderBoundary` 后，本地 `UBlueprintNodeSpawner::Create(NodeClass)`，并构造本地 `Resolved` 状态。

偏差：控制流仍未真正进入统一 resolver，`branch` 在实现上保持特殊路径。

期望：`control` 保留 `ControlKind=Branch/Return/Sequence` 作为 semantic constraint，由 Generic cluster 解析；专用 builder 只做多节点 DAG 编排。

## P1 差距

### 5. 主流 Pipeline 仍使用 `FParsedNode` 作为语义桥接模型

位置：`BlueprintGraphGenerationPipeline.cpp`

证据：`SpawnSemanticStatementFragment` 将 semantic statement 映射成 `FParsedNode` 再传 builder。

偏差：`FParsedNode` 是旧 graph-json/node-shape 模型，不应继续作为主路径内部模型。

期望：新增或收敛到 statement/action IR request，让 builder 消费语义模型。

### 6. Statement/Expression 仍靠硬编码 kind 分支

位置：`BlueprintGraphGenerationPipeline.cpp`、`BlueprintHelperGraphStatementBuilder.cpp`

证据：`Call / Set / SetProperty / Branch / Return / Construct / Deconstruct / Select` 等仍通过硬编码 `if/case Kind` 进入不同 builder。

偏差：扩展新语义时需要继续改 pipeline 分支，不符合 registry/resolver/builder 扩展范式。

期望：建立 `SemanticKind -> FragmentBuilder` registry，pipeline 只负责 context、identity 和 builder lookup。

### 7. DAG 层仍把可解析表达式先建成 placeholder

位置：`BlueprintHelperGraphFragmentDagBuilderUtils.cpp`

证据：`Call / Op / Construct / Deconstruct` 表达式仍走 `BuildPlaceholderExpression`。

偏差：与 ActionResolution 可生成真实 fragment 的路径不一致，DAG 语义不稳定。

期望：只有真正不可解析时才 placeholder；可解析表达式应直接形成真实 fragment/evidence。

### 8. EventDelegateActionCluster 声明占有 bind/component_bound_event，但无成功路径

位置：`BlueprintHelperEventDelegateActionCluster.cpp`

证据：`event` 有 custom event spawner；`component_bound_event` / `bind` 只返回缺 context。

偏差：簇能力声明和实际能力不一致。

期望：补 ActionContextPipeline 的 component/delegate/signature evidence，再接 `UBlueprintBoundEventNodeSpawner` / `UBlueprintDelegateNodeSpawner`。

### 9. Generic resolver 仍有过宽全局扫描

位置：`BlueprintHelperGenericAssetStructControlActionResolver.cpp`

证据：`ResolveStructType` / `ResolveNativeStructFunction` 使用 `TObjectIterator<UScriptStruct/UFunction>`。

偏差：候选域不是由 projected context 收缩，容易误匹配。

期望：用 projected owner/module/type hints 收缩候选；宽搜索只能作为显式诊断模式。

### 10. FunctionAction 候选构建仍过宽

位置：`BlueprintHelperCallFunctionResolverUtils.cpp`

证据：`BuildCandidateUniverse` 仍大范围枚举 class/function library。

偏差：没有充分通过 TaskSpec/Graph/TypedPin/Target context 约束 UE action 候选。

期望：先由 projected context 构建候选域，再进入 UE action/filter；全量扫描降级为低置信诊断。

## P2 差距

### 11. FieldVariable 的 get_property/set_property 边界仍偏弱

位置：`BlueprintHelperFieldVariableActionResolver.cpp`

证据：`GetProperty/SetProperty` 和 `Get/Set` 基本共用 `field_name / TargetPath / Query` 查找链。

偏差：复杂 property path、owner class、分层路径语义没有显式建模。

期望：保留底层 variable spawner 复用，但 property 语义要有 path 分段、owner evidence、类型验证。

### 12. MutationPlan 路径仍公开，但执行已退化为 unsupported 诊断

位置：`BlueprintGraphMutationPlan.h`、`BlueprintGraphMutationPlanExecutor.cpp`

证据：类型和 executor 仍存在，但 `SpawnNodes` 只产出 `parsed_node_plan_unsupported`。

偏差：这是可达但无真实语义的旧兼容债。

期望：若不再作为 public path，直接删除；若保留，入口 fail-fast 并从主路径隔离。

### 13. Facade 仍暴露旧 ParsedNode/Macro spawn 入口

位置：`BlueprintGraphWriteFacade.h`、`BlueprintGraphWriteFacade.cpp`、`BlueprintGraphNodeSpawner.cpp`

证据：`FParsedNode / FParsedLink / SpawnMacroNode(FParsedNode)` 仍是 public-ish 入口。

偏差：外部仍可能命中旧 node-shape 模型。

期望：移到隔离 internal 模块，或删除公开入口。

### 14. docs/schema 仍有旧 `call_function / set_member_variable` 口径残留

位置：`AgentFaceService/docs/CLI_Tools_API_Reference.md`、`AgentFaceService/docs/TaskSpec_TaskPlan_Contract_20260504.md`

证据：文档仍描述 `call_function`、`set_member_variable` 可用或兼容。

偏差：和旧 AgentFace shapes 不兼容、直接报错的当前口径冲突。

期望：更新 docs/schema，只保留旧格式 unsupported 的说明。

## Code Review 补充：快速接入旧路径/架构偏离确认

### 15. AgentFace 仍公开 `branch/sequence/return` 顶层 statement kind

位置：`AgentFaceService/task-core/src/task/schema/task-contract.ts`、`AgentFaceService/task-core/src/task/compiler/task-compiler.ts`、`AgentFaceService/task-core/python/blueprinthelper_task/compiler/graph_write_append.py`

证据：`statement_kinds` 与 compiler 校验仍把 `branch`、`sequence`、`return` 作为顶层 statement kind；Python 编译链与 TS 编译链对 `sequence` 的支持口径不一致。

偏差：最新架构要求执行流统一为 `control` semantic intent，`branch/sequence/return` 只能作为 `SemanticConstraints` 内的控制流细分，不应作为公开主入口继续扩展。

期望：AgentFace schema / TS compiler / Python compiler 统一迁移到 `kind: "control"` + control constraint；移除 `sequence` 这类快速接入式顶层 kind，旧字段不再做兼容兜底。

### 16. AgentFace 文档仍残留 `call_function` 兼容口径

位置：`AgentFaceService/agent-guide/Workflows/05_Edit_Blueprint_Workflow.md`、`AgentFaceService/agent-guide/Reference/04_Tool_Surface_Field_Templates_20260512.md`

证据：文档仍出现 `kind: "call_function"` / `call_function.name` 示例或 “legacy-compatible remains accepted” 口径；实际编译器已将 `call_function` 视为 unsupported。

偏差：文档继续引导 Agent 生成旧 node shape，和“旧实现不兼容、不保留 fallback”的当前架构冲突。

期望：删除旧示例与兼容描述，只保留 `kind: "call"` + target / semantic constraints 的当前写法。

### 17. Control flow 仍用专用 builder 快路径承接 Branch/Return/Sequence

位置：`BlueprintHelperControlFragmentBuilder.cpp`

证据：`BuildBranch`、`BuildReturn`、`BuildSequence` 仍是独立入口；`BuildSequence` 甚至直接以空 `ActionContextScope` 调 `ResolveControlActionProvider`。

偏差：控制流应该由 GenericAssetStructControlActionCluster 消费 projected context 并返回 spawner evidence；专用 builder 只能作为多节点 DAG 编排边界，不能代替簇内 resolver。

期望：`control` 统一进入 Generic cluster；单节点 Branch/Return/Sequence 先解析 UE spawner evidence，再由 shared adapter invoke；多节点 control DAG 才进入专用 builder，并写明不可由 UE NodeSpawner 单独表达的原因。

### 18. GraphStatementBuilder 仍局部重建 ActionContext

位置：`BlueprintHelperGraphStatementBuilder.cpp`

证据：`BuildSingleActionContextDemand`、`TryBuildProjectedActionRequestFromContext` 在 builder 内部构造 `ActionContextDemand`，当外部 scope 不存在时会本地 `FBlueprintHelperActionContextScope::Build`。

偏差：设计文档要求 `ActionContextPipeline` 是上下文构建唯一入口，`BundleProjector` 是 `ResolvedActionContextBundle -> ActionResolutionRequest` 的唯一投影边界；Builder 不应私自重建 context。

期望：Builder 只消费已投影的 request/evidence；缺少 context 时返回明确诊断，不能在 builder 内重新 snapshot / inference / project。

### 19. Select 虽使用 shared adapter，但仍有较多 UK2Node_Select 本地 pin 适配

位置：`BlueprintHelperSelectFragmentBuilder.cpp`

证据：`ApplyIndexPinType`、`ApplyResultPinType`、literal defaults collection 等逻辑直接操作 `UK2Node_Select`。

偏差：Select 属于 Generic cluster 的 semantic constraints；如果这些 pin normalization / post-spawn defaults 属于通用 lifecycle，应收敛到 shared adapter / composer lifecycle，而不是沉积在 Select 专用 builder。

期望：保留必要的 Select 专用语义，但把通用 post-spawn defaults、pin normalization、post-link lifecycle 下沉到 shared adapter / composer lifecycle；Select builder 只保留 UE 无法泛化的最小差异。

### 20. FunctionAction 与 GenericAction 仍有全局反射扫描

位置：`BlueprintHelperCallFunctionResolverUtils.cpp`、`BlueprintHelperGenericAssetStructControlActionResolver.cpp`

证据：`BuildCandidateUniverse`、`ResolveClassByTypeName`、`ResolveStructByTypeName`、`ResolveStructType`、`ResolveNativeStructFunction` 仍使用 `TObjectIterator<UClass/UScriptStruct/UFunction>` 或大范围扫描。

偏差：这会绕过 projected context 对候选域的约束，属于旧式“扫描补全”路径；设计要求先由 TaskSpec / Blueprint / Graph / typed pin / target context 收缩候选，再进入 UE ActionDatabase / ActionFilter / NodeSpawner。

期望：全局扫描只能作为显式诊断/低置信模式或移除；主路径必须从 projected context 构建候选域并返回可重建 spawner evidence。

### 21. EventDelegateActionCluster 声明能力与成功路径不一致

位置：`BlueprintHelperEventDelegateActionCluster.cpp`

证据：`OwnsSemanticKind` 声明 `ComponentBoundEvent`、`Bind`，但 `Resolve` 对这些语义仅返回 `needs_more_semantic_context`，没有 `UBlueprintBoundEventNodeSpawner` / `UBlueprintDelegateNodeSpawner` 成功路径。

偏差：簇对外声明覆盖能力但内部未闭环，容易让上游认为该语义已完成；这是半通道接入而非完整架构实现。

期望：补齐 component/delegate/signature evidence 后再声明成功路径；未完成前文档和测试都应明确标为 gap，而不是能力完成。

### 22. FragmentDAG placeholder 仍可能污染主路径

位置：`BlueprintHelperGraphFragmentDagBuilderUtils.cpp`

证据：`call/op/construct/deconstruct` 等可解析表达式仍可能先构造 placeholder；unknown statement 也会生成 placeholder fragment 后继续下游。

偏差：placeholder 只应代表真正不可解析或 UE ActionDatabase 不可表达的例外；主流语义生成 placeholder 会弱化 preview/execute 证据一致性。

期望：可解析表达式必须形成真实 fragment/evidence；不可解析时 fail-fast 并输出明确 diagnostics，不继续以 placeholder 伪装成功。

## 总体判断

- ActionResolutionCore / 四簇一级分发：基本符合新架构。
- GraphStatement builder / control / select：仍有明显本地 spawner/direct node 残留。
- Pipeline / Merge / Patch / ParsedNode：旧模型仍可达，是最大结构债。
- docs/tests/schema：仍有旧口径，需要同步清理，否则后续 Agent 会继续生成旧输入。

## 建议修复顺序

1. 先处理 Merge/Patch 可达旧主链。
2. 再迁移 select/control 到 Generic cluster + shared adapter。
3. 再移除 `FParsedNode` 作为主路径 builder 入参。
4. 最后收紧 Function/Generic 的全局扫描和 docs/schema 旧口径。
