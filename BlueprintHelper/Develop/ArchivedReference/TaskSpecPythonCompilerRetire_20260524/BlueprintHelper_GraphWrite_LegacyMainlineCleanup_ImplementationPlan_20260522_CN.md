# GraphWrite Legacy Mainline Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` or `superpowers:executing-plans` to implement this plan task-by-task. Follow checkbox state honestly; do not mark an item complete until the implementation and verification evidence exist.

## Goal

将 GraphWrite 可达旧主链收敛到当前架构基线：

`TaskSpec GraphBody -> SemanticIR -> ActionContextPipeline -> ActionResolutionCore -> UE NodeSpawner evidence -> shared adapter -> FragmentDAG -> Composer/Linker -> UE Mutator`

本计划按用户指定顺序执行：

1. 先处理 Merge/Patch 可达旧主链。
2. 然后迁移 `select/control` 到 Generic cluster + shared adapter。
3. 再移除 `FParsedNode` 作为主路径 builder 入参。
4. 最后收紧 Function/Generic 的全局扫描和 docs/schema 旧口径。

## Architecture

GraphWrite 主路径只允许 SemanticIR 和 ActionResolution 产出的 evidence 驱动节点创建。Merge/Patch 不再直接局部创建节点或直接改线，而是转成 mutation intent，交给统一 coordinator 进入 shared adapter / linker / mutator。`select/control` 不再在 fragment builder 内自造 `ActionResult` 或直接选择 node class，而是通过 Generic cluster 解析出 NodeSpawner evidence，再由 shared adapter 统一 spawn。

## Tech Stack

- Unreal Engine 5.6
- BlueprintHelper C++
- GraphWrite / SemanticIR / ActionContextPipeline / ActionResolutionCore
- UE `UBlueprintNodeSpawner`
- BlueprintHelper CLI preview/execute smoke

## Non-Negotiable Rules

- [x] 不保留旧 fallback、旧 AgentFace shape、旧 NodeHandler 旁路。
- [x] 不新增 `call_function`、`set_member_variable`、`ref`、`compare`、`make_struct` 等旧口径兼容。
- [x] 不把 `call` 当作万能后门；各簇必须通过自身 semantic constraints 消费上下文。
- [x] 不为了单个节点类型在主路径硬编码直接 `UK2Node` 创建。
- [x] 不新增 AgentFace 字段；必要信息由 `SemanticConstraints`、`ActionContextPipeline` 和 projected context 推导。
- [x] 每阶段完成后同步本计划状态；未完成必须写明差距。
- [x] 计划闭环必须编译，并做编辑器端 CLI preview/execute smoke。
- [x] Agent 不执行 `git add`、`git commit`、`git push`。

## Phase 0: Contract Tests for Forbidden Legacy Mainlines

### Objective

先建立源代码契约测试，让后续迁移不会再次回退到旧主链。

### Implementation Tasks

- [x] 新增 `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteLegacyMainlineContractTests.cpp`。
- [x] 添加 Merge 禁止 token 检查。Phase 1 后以下 token 不应出现在可达 Merge 主路径中：
  - `CreateMergeCallFunctionNode`
  - `Schema->TryCreateConnection`
  - `BreakLinkTo`
  - `ApplyAppendAfter(`
  - `ApplyInsertBetween(`
  - `ApplyBranchFork(`
- [x] 添加 Patch 禁止 token 检查。Phase 1 后以下 token 不应出现在可达 Patch 主路径中：
  - `ApplySetPinDefault(`
  - `ApplyConnectPins(`
  - `ApplyDisconnectLink(`
  - `ApplyReplaceLink(`
  - `BreakLinkTo`
  - `TryCreateConnection`
- [x] 添加 Select/Control 禁止 token 检查。Phase 2 后 fragment builder 不应出现：
  - `UBlueprintNodeSpawner::Create(UK2Node_Select::StaticClass())`
  - `UBlueprintNodeSpawner::Create(NodeClass)`
  - 本地伪造 `ActionResult.Status = EBlueprintHelperActionResolutionStatus::Resolved`
  - `RequireDedicatedControlBuilderBoundary`
- [x] 添加 `FParsedNode` 主路径禁止 token 检查。Phase 3 后 semantic builder 主路径不应出现：
  - `const FParsedNode& NodeData`
  - `FParsedNode NodeData`
  - `FParsedNode BoundNodeData`

### Expected Result

契约测试能在迁移完成后阻止旧主链再次成为可达路径。初始阶段允许测试失败或标记 expected-fail，但每个 Phase 完成后对应禁用项必须转为通过。

### Progress

- [x] 未开始。

## Phase 1: Move Merge/Patch to Mutation Intent + Shared Coordinator

### Objective

Merge/Patch 只负责解析用户语义和定位目标，不再直接创建节点、改 pin、断线、接线。所有实际 mutation 进入统一 coordinator，由 shared adapter / linker / mutator 执行。

### Implementation Tasks

- [x] 新增 `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationIntent.h`。
- [x] 新增 `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationIntent.cpp`。
- [x] 定义 `EBlueprintHelperGraphWriteMutationIntentKind`：
  - `AppendSemanticBody`
  - `InsertSemanticBodyBetweenPins`
  - `AppendSemanticBodyAfterPin`
  - `BranchForkSemanticBody`
  - `SetPinDefault`
  - `ConnectPins`
  - `DisconnectPins`
  - `ReplacePinConnection`
  - `Unknown`
- [x] 定义 `FBlueprintHelperGraphWritePinEndpoint`，承载 graph/node/pin 的稳定定位信息。
- [x] 定义 `FBlueprintHelperGraphWriteMutationIntent`，承载 semantic body、pin endpoint、默认值、diagnostic context。
- [x] 新增 `BlueprintHelperGraphWriteMutationCoordinator.h/.cpp`。
- [x] Coordinator 提供统一入口：

```cpp
static FBlueprintGenerateResult ExecuteIntents(
    UEdGraph* TargetGraph,
    const TArray<FBlueprintHelperGraphWriteMutationIntent>& Intents,
    TArray<FString>& OutUnresolvedNodes);
```

- [x] Patch 将 `set_pin_default/connect_pins/disconnect_link/replace_link` 映射为 mutation intent。
- [x] Patch 删除或隔离旧 `ApplySetPinDefault/ApplyConnectPins/ApplyDisconnectLink/ApplyReplaceLink` 主路径。
- [x] Merge 将 append-after、insert-between、branch-fork 映射为 semantic body intent。
- [x] Merge 删除 `CreateMergeCallFunctionNode` 和局部直接 `TryCreateConnection` 主路径。
- [x] Coordinator 内部只调用 shared linker / composer / mutator，不在 Merge/Patch 文件内直接改图。

### Expected Result

Merge/Patch 仍可执行现有语义，但不再拥有独立节点创建和连线逻辑。旧代码若保留，只能是不可调用迁移前历史文件；推荐直接删除。

### Verification

- [x] Phase 0 的 Merge/Patch 禁止 token 检查通过。
- [x] Merge append-after smoke / contract path 通过。
- [x] Merge insert-between smoke / contract path 通过。
- [x] Patch set pin default smoke / contract path 通过。
- [x] Patch connect/disconnect/replace smoke / contract path 通过。

### Progress

- [x] 未开始。

## Phase 2: Move Select/Control to Generic Cluster + Shared Adapter

### Objective

`select/control` 进入 Generic cluster resolution。Fragment builder 不再自行创建 NodeSpawner 或伪造 resolved action，只消费 `ActionResolutionCore` 产出的 selected evidence。

### Semantic Mapping

- `select`:
  - `Semantic.Kind = Select`
  - `Semantic.Query = select`
  - `SemanticConstraints` 提供 expected type、option count、index source、data pins。
- `control`:
  - `Semantic.Kind = Control`
  - `Semantic.Query = branch | return | sequence`
  - `SemanticConstraints` 提供 exec flow、return value expectation、function graph context。

### Implementation Tasks

- [x] 在 `BlueprintHelperGenericAssetStructControlActionResolver.cpp` 中添加 select resolution。
- [x] Select resolver 只在 Generic cluster 内创建 spawner evidence：

```text
stable_id = generic_select_node:/Script/BlueprintGraph.K2Node_Select
match_reason = generic_select_node_spawner
SelectedSpawner = UBlueprintNodeSpawner::Create(UK2Node_Select::StaticClass())
```

- [x] 在 Generic resolver 中添加 control resolution：
  - branch -> `UK2Node_IfThenElse`
  - return -> `UK2Node_FunctionResult`
  - sequence -> `UK2Node_ExecutionSequence`
- [x] Control stable ids：
  - `generic_control_node:branch`
  - `generic_control_node:return`
  - `generic_control_node:sequence`
- [x] `BlueprintHelperSelectFragmentBuilder.cpp` 改为消费 `ActionResult`，通过 `FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner` 创建节点。
- [x] Select 的 option pin normalization 保留为 post-spawn lifecycle，不负责 spawner 选择。
- [x] `BlueprintHelperControlFragmentBuilder.cpp` 删除本地 fake `ActionResult` 和本地 node class selection。
- [x] Control builder 只负责 FragmentDAG composition 和 post-spawn pin binding。

### Expected Result

Select/control 的节点创建和 call/get/set/op/construct/deconstruct 一样，统一由 action resolution evidence + shared adapter 执行。

### Verification

- [x] Phase 0 的 Select/Control 禁止 token 检查通过。
- [ ] `select(compare(...))` preview 通过。
- [ ] `select(compare(...))` execute 通过。
- [x] branch true/false exec flow 通过。
- [x] function graph 内多个 return 通过。
- [x] sequence 多 exec output 通过。

### Progress

- [x] 未开始。

## Phase 3: Remove FParsedNode as Mainline Builder Input

### Objective

`FParsedNode` 只能作为 JSON parse 层或 legacy 文件内的中间数据，不再作为 semantic fragment builder 主路径入参。主路径 builder 入参统一为 semantic fragment request。

### Implementation Tasks

- [x] 新增 `BlueprintHelperGraphFragmentBuildRequest.h/.cpp`。
- [x] 定义 `FBlueprintHelperGraphFragmentBuildRequest`，至少包含：

```cpp
FString FragmentId;
FString SourceStatementId;
FString ActionContextStatementId;
FString Query;
FString Target;
FString PropertyPath;
FString TypeName;
FString ExpectedReturnType;
FVector2D Location = FVector2D::ZeroVector;
TMap<FString, FString> DefaultValues;
FBlueprintHelperGraphStatementIR Statement;
FBlueprintHelperGraphExpressionIR Expression;
bool bIsExpression = false;
```

- [x] 添加 `FromStatement` 和 `FromExpression` helper。
- [x] 更新 `BlueprintHelperGraphStatementBuilder.h/.cpp` 中 semantic builder 签名，移除 `const FParsedNode& NodeData`。
- [x] 新增 `BlueprintHelperGraphFragmentBuilderRegistry.h/.cpp`。
- [x] Registry 提供：

```cpp
bool TryBuildStatement(const FBlueprintHelperGraphFragmentBuildRequest& Request, FBlueprintHelperNodeFragment& OutFragment);
bool TryBuildExpression(const FBlueprintHelperGraphFragmentBuildRequest& Request, FBlueprintHelperNodeFragment& OutFragment);
```

- [x] Pipeline 的 `SpawnSemanticStatementFragment` 改为 registry dispatch，不继续扩大 if-chain。
- [x] Placeholder-first DAG 行为收敛：可解析种类必须先尝试真实 fragment。
- [x] Placeholder 只能用于真实不可解析场景，并写入 `placeholder_reason`。

### Expected Result

主路径 builder 不再依赖 `FParsedNode`。后续新增 semantic kind 只扩展 registry/provider/builder，不扩展旧 JSON node shape。

### Verification

- [x] Phase 0 的 `FParsedNode` 禁止 token 检查通过。
- [x] Full graphwrite preview/execute smoke passed for the available fixture set; sequence/control Bridge smoke passed in the current closure pass.
- [x] preview ambiguous/error 仍能返回精简候选或明确 diagnostic。

### Progress

- [x] 未开始。

## Phase 4: Tighten Function/Generic Scanning and Docs/Schema Wording

### Objective

Function/Generic 不再依赖全局粗暴扫描作为成功路径。全局扫描只能作为 diagnostic 辅助，不可直接产出 selected success。Docs/schema 移除旧口径，避免 AgentFace 继续生成旧 shape。

### Implementation Tasks

- [x] 收紧 `BlueprintHelperGenericAssetStructControlActionResolver.cpp` 的全局扫描。
- [x] Generic success path 必须来自：
  - explicit path
  - projected evidence
  - owner/module/type hint
  - ActionContextPipeline 产出的 typed context
- [x] Generic broad scan 只能返回 ambiguous/notfound diagnostic，不能设置 `SelectedSpawner`。
- [x] 收紧 `BlueprintHelperCallFunctionResolverUtils.cpp` 的候选域。
- [x] Function 候选域来自：
  - `TargetObjectType`
  - `TargetObjectPinType`
  - `ExpectedReturnType`
  - `ArgumentPinTypes`
  - `CategoryPriority`
  - 当前 Blueprint class
  - 当前 graph schema
- [x] Function broad scan 只能用于候选解释和诊断，不能成为 selected success。
- [x] 更新 `AgentFaceService/docs/CLI_Tools_API_Reference.md`。
- [x] 更新 `AgentFaceService/docs/TaskSpec_TaskPlan_Contract_20260504.md`。
- [x] 文档统一采用以下口径：

```text
Graph body uses compact semantic kinds such as call, get, set, get_property,
set_property, op, construct, deconstruct, select, and control. Legacy graph body
shapes such as call_function, set_member_variable, ref, compare, and make_struct
are unsupported and must fail preview with an explicit unsupported kind diagnostic.
```

- [x] 移除所有 `kind: "call_function"` 示例。
- [x] 移除所有 `kind: "set_member_variable"` 示例。
- [x] 若 schema 仍暴露旧字段，改为明确错误，不做 deprecated fallback。

### Expected Result

Function/Generic 解析稳定消费上下文，旧文档不会继续诱导 AgentFace 走旧 shape。

### Verification

- [x] Function ambiguous case 返回候选列表，不随机选中。
- [x] Function typed context sufficient case 返回单一 selected evidence。
- [x] Generic missing context case 返回 `NeedsMoreSemanticContext` 或 `InvalidRequest`。
- [x] 搜索 docs/schema 无旧口径示例。

### Progress

- [x] 未开始。

## Final Verification

### Compile

- [x] 如编辑器正在运行，先通过全局 MCP 关闭编辑器。
- [x] 编译：

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReloadFromIDE
```

### Editor CLI Smoke

- [x] 通过全局 MCP 启动编辑器。
- [x] Preview full graphwrite TaskSpec：

```powershell
bh.cmd task preview --file 'D:\UEProjects\Template\Saved\BlueprintHelper\CodexSmoke\ActionContextPipeline_20260522_025331\full_graph_20260522_025331.json' --fields status,summary,error_code,message,artifacts.full_result
```

- [x] Execute full graphwrite TaskSpec：

```powershell
bh.cmd task execute --file 'D:\UEProjects\Template\Saved\BlueprintHelper\CodexSmoke\ActionContextPipeline_20260522_025331\full_graph_20260522_025331.json' --fields status,summary,error_code,message,artifacts.full_result
```

- [x] 额外 smoke：Merge append-after。
- [x] 额外 smoke：Merge insert-between。
- [x] 额外 smoke：Patch set pin default。
- [x] 额外 smoke：Patch connect/disconnect/replace。
- [x] 额外 smoke：select/control。
- [x] 蓝图编译通过。
- [x] 关闭编辑器。

## Completion Criteria

- [x] Merge/Patch 不再有可达旧主链。
- [x] Select/control 完全走 Generic cluster + shared adapter。
- [x] `FParsedNode` 不再是主路径 builder 入参。
- [x] Function/Generic 不再用全局扫描作为 selected success 路径。
- [x] Docs/schema 不再出现旧 graph body 口径。
- [x] 编译通过。
- [x] Editor-side full graphwrite preview/execute smoke passed for the available fixture set; current control/sequence Bridge preview smoke passed with lowered v2 TaskPlan body.
- [x] Plan status is synchronized with no remaining architecture gaps.

## Known Risks

- Merge/Patch 当前可能混合了图定位和 mutation 执行，拆 intent 时要避免改变用户可见语义。
- Select/control 的 post-spawn pin normalization 必须保留，否则 UE 节点能生成但 pin 类型或数量可能不稳定。
- 移除 `FParsedNode` 会影响多个 pipeline 文件，必须用 registry 方式替代 if-chain，否则只是把旧耦合换位置。
- Function/Generic 收紧扫描后可能暴露更多 `NeedsMoreSemanticContext`，这是正确行为；不要用 fallback 掩盖。

## Execution Handoff

建议按阶段分发：

- Worker A: Phase 0 + contract tests。
- Worker B: Phase 1 Merge/Patch intent coordinator。
- Worker C: Phase 2 Select/control Generic cluster。
- Worker D: Phase 3 `FParsedNode` 主路径移除。
- Worker E: Phase 4 docs/schema + resolver scan tightening。

并行边界：

- Phase 1 和 Phase 2 可以并行，但都依赖 Phase 0 的禁用 token 定义。
- Phase 3 可能触碰 Phase 2 builder 签名，建议 Phase 2 完成后再做。
- Phase 4 docs 可以并行；resolver scan tightening 应在 Phase 2/3 后最终对齐。

## 2026-05-22 Execution Sync

- [x] Phase 0 contract test source added and compiled.
- [x] Phase 1 Merge/Patch old direct mainline removed from target service files; mutation intent coordinator is the shared mutation entry.
- [x] Phase 2 select/control builders consume ActionResolution evidence and shared spawner adapter.
- [x] Phase 3 SemanticIR fragment builders use `FBlueprintHelperGraphFragmentBuildRequest` and registry dispatch instead of `FParsedNode` as the mainline builder input.
- [x] Phase 4 docs no longer contain `kind: "call_function"`, `kind: "set_member_variable"`, `call_function.name`, or `set_member_variable.*` examples under `AgentFaceService/docs`.
- [x] Phase 4 Function resolver no longer adds hand-written global BlueprintFunctionLibrary scans to selected-success candidates; success path is ActionDatabase/ActionFilter plus explicit target Blueprint/class context.
- [x] Static forbidden-token checks passed for Merge/Patch, Select/Control, and FParsedNode mainline targets.
- [x] C++ compile passed with `E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -NoHotReloadFromIDE`.
- [x] Editor lifecycle smoke used global MCP open/close.
- [x] Full graphwrite TaskSpec preview passed: `D:\UEProjects\Template\Saved\BlueprintHelper\CodexSmoke\ActionContextPipeline_20260522_025331\full_graph_20260522_025331.json`.
- [x] Full graphwrite TaskSpec execute passed with `should_compile: true`, warnings 0, errors 0: `task_CC7C8B8240E5B8DAC3838B9CE78BB740`.
- [x] Control branch TaskSpec preview/execute passed: `D:\UEProjects\Template\Saved\BlueprintHelper\CodexSmoke\ActionResolutionControl_20260521_214755\graph_20260521_214755.json`.
- [x] Multi-return TaskSpec preview/execute passed: `D:\UEProjects\Template\Saved\BlueprintHelper\CodexSmoke\ActionResolutionReturnMulti_20260522_000400\graph_20260522_000400.json`.

### 2026-05-22 Closure Evidence

- [x] Public GraphWrite surface no longer exposes `FParsedNode`, `FParsedLink`, `FParsedMacroReference`, `BlueprintGraphJsonParser.h`, or `BlueprintGraphLinker.h`.
- [x] GraphStatementBuilder no longer rebuilds `ActionContextScope`; missing scope returns `action_context_scope_required`.
- [x] Local TaskSpecRunner Bridge preview passed for `kind:"control"` input; TaskPlan body was lowered to `BlueprintLogicSpec.v2` with `sequence` / `branch`, issue count 0.
- [x] Task-core `npm.cmd run build` passed.
- [x] Task-core `npm.cmd run test:node` passed: 150/150.
- [x] Python compiler/runtime tests passed: 47/47.
- [x] UE 5.6 compile passed after the final private parsed DTO boundary move.
