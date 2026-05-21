# BlueprintHelper DataFlow Core First Batch Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the first batch Graph body data-flow AgentFace semantics and remove the old NodeHandler creation path so GraphWrite only creates UE nodes through SemanticIR -> FragmentDAG -> NodeFragment builder / mutator.

**Architecture:** AgentFace emits compact semantic `BlueprintLogicSpec` fields; TS/Python compilers preserve canonical data-flow kinds and do not keep old Graph body aliases. UE GraphWrite parses the same semantic model into SemanticIR, resolves types and symbols, builds a FragmentDAG, and mutates UE graphs only through focused fragment builders and the graph mutator. Legacy `NodeHandlers`, direct parsed-node spawning fallback, Graph body `call_function/name`, Graph body `set_member_variable/name`, `compare`, `make_struct`, and `ref` are removed from the public/canonical path. Old node fallback is not allowed and is not deprecated compatibility; unsupported old shapes must error instead of being normalized or rerouted.

**Tech Stack:** TypeScript task-core compiler, Python canonical compiler, Unreal Engine 5.6 C++ plugin, Blueprint Graph K2 APIs, BlueprintHelper TaskRuntime, Automation tests.

---

## 褰撳墠鎵ц鐘舵€侊紙2026-05-21锛?
- [x] `FieldVariableActionCluster` 已实现成员变量 `UBlueprintVariableNodeSpawner` get/set 候选解析，并接入 NodeFragment emission；`get_property/set_property` 仍未完成。
璺濈鏈熸湜宸窛锛歠irst-batch 褰撳墠涓嶆槸瀹屾暣鍙啓鑳藉姏锛屽彧瀹屾垚浜嗛鏋跺己鍒惰矾鐢卞拰鏃?fallback 鍒囨柇銆備笅涓€闃舵蹇呴』琛ョ湡瀹?provider 涓?fragment adapter锛屼笉鑳芥妸 `UnsupportedClusterMigration` 褰撳畬鎴愮姸鎬併€?

## 2026-05-21 Runtime Smoke 同步

- [x] `FieldVariableActionCluster` 已实现成员变量 `get/set` 的 `UBlueprintVariableNodeSpawner` 候选解析。
- [x] `FieldVariableAction` `get/set` 已接入 NodeFragment emission，并通过真实 Blueprint create / variable / graph execute / logic_json read-back smoke。
- [x] 修复成员变量 `VarContext` 误传导致的 local variable scope 问题；成员变量 spawner 对齐 UE ActionDatabase，不传目标图作为 local context。

距离期望差距：first-batch 仍未完成 `get_property/set_property/op/construct/deconstruct/select` 的完整 provider + adapter 覆盖；本次只关闭 FieldVariable `get/set` 真实运行面。

## Source Requirements

Primary requirements are recorded in:

- `AgentFaceService/docs/TaskSpec_UE_Editor_Capability_Matrix_20260521_CN.md`
- `BlueprintHelper/Develop/Plan/BlueprintHelper_DataFlowCore_AgentFaceFields_20260520_CN.md`

Hard boundaries:

- The first batch belongs only to `BlueprintLogicSpec` / Graph body writes.
- `set_property` does not replace `edit_object_properties`, component template edits, WidgetTree design-time edits, class defaults, or DataAsset edits.
- `construct/deconstruct` use two-stage resolver rules when fields are missing.
- AgentFace does not expose UE node names such as `MakeVector`, `BreakStruct`, `KismetMathLibrary.Greater`, or `UK2Node_*`.
- GraphWrite does not create signatures, assets, components, WidgetTree nodes, DataTable rows, or class settings.
- Old NodeHandler fallback is not allowed and must be removed rather than kept as deprecated compatibility or a hidden fallback path.

## Spawner-Oriented AgentFace Intent vs UE Action Layer

Current architecture baseline:

```text
AgentFace compact intent
-> Semantic Resolver / typed resolver
-> SpawnerClusterResolver
-> BlueprintActionResolutionCore
-> selected UBlueprintNodeSpawner or derived spawner
-> cluster-specific NodeFragment adapter
-> FragmentDAG
-> Composer / Linker
-> UE Mutator
```

AgentFace kinds such as `call`, `get`, `set`, `op`, `construct`, `deconstruct`, `select`, `event`, `bind`, `create`, and `schedule` are compact semantic intents. They do not define lower-level implementation clusters. Lower-level clusters are defined by UE NodeSpawner families:

```text
FunctionActionCluster
FieldVariableActionCluster
EventDelegateActionCluster
GenericAssetStructControlActionCluster
```

Cluster ownership:

| Cluster | UE spawner family | AgentFace intents routed here |
|---|---|---|
| `FunctionActionCluster` | `UBlueprintFunctionNodeSpawner`, type-promotion operator spawners | `call`, `op`, function-shaped `convert`, function-shaped `schedule` |
- [x] `FieldVariableActionCluster` 已实现成员变量 `UBlueprintVariableNodeSpawner` get/set 候选解析，并接入 NodeFragment emission；`get_property/set_property` 仍未完成。
| `EventDelegateActionCluster` | `UBlueprintEventNodeSpawner`, `UBlueprintBoundEventNodeSpawner`, `UAnimNotifyEventNodeSpawner`, `UBlueprintDelegateNodeSpawner`, `UBlueprintBoundNodeSpawner` | `event`, `component_bound_event`, `bind`, `unbind`, `assign`, `delegate_call` |
| `GenericAssetStructControlActionCluster` | generic `UBlueprintNodeSpawner`, `UBlueprintAssetNodeSpawner`, struct / enum / generic registrar delegates | `construct`, `deconstruct`, `select`, `control`, `create`, `asset_action` |

Hard internal reuse rule:

```text
if UE editor context menu can represent the node/action:
  semantic resolver builds constraints
  SpawnerClusterResolver selects a spawner-oriented cluster
  BlueprintActionResolutionCore builds candidates from ActionDatabase / BlueprintActionFilter
  selected UBlueprintNodeSpawner creates the node
else:
  a dedicated FragmentBuilder is allowed only with documented ActionDatabase-unrepresentable reason
```

Forbidden API simplification:

```text
get/set/op/construct/deconstruct/select/event/bind/create/schedule
-> expose UE function names, node class names, or spawner internals to AgentFace
```

This keeps AgentFace fields compact while making lower-level GraphWrite responsibilities match UE NodeSpawner families instead of overlapping natural-language semantic groups.
## Spawner-Oriented Cluster Baseline

鏈鍒掍腑鐨?`get` / `set` / `get_property` / `set_property` / `op` / `construct` / `deconstruct` / `select` 鏄?AgentFace 璇箟 intent锛屼笉鍐嶅畾涔夊簳灞傚伐鍏风皣杈圭晫銆?
搴曞眰鑳藉姏蹇呴』鎸?UE `NodeSpawner` 瀹舵棌缁勭粐锛?
```text
AgentFace semantic intent
-> SpawnerClusterResolver
-> BlueprintActionResolutionCore
-> UBlueprintNodeSpawner candidate
-> NodeFragment adapter
-> FragmentDAG
-> Composer / Linker / Mutator
```

`call` 涓嶆槸缁熶竴 action resolution 鐨勬墍鏈夎€咃紱`call` 鍙槸 `FunctionActionCluster` 鐨勪竴涓?consumer銆俙get` / `set` / `op` / `construct` 绛?intent 涓嶈兘閫氳繃 `call` 鐨勫眬閮ㄥ疄鐜板厹搴曪紝鑰屽簲澶嶇敤鍚屼竴濂?`BlueprintActionResolutionCore`銆?
棣栨壒 DataFlowCore 鏄犲皠濡備笅锛?
| AgentFace kind | Spawner-Oriented Cluster | 璇存槑 |
|---|---|---|
| `call` | `FunctionActionCluster` | 鏅€?callable/action锛屼綔涓哄叡浜?resolver 鐨?consumer |
| `op` | `FunctionActionCluster` | operator 閫氳繃 typed operands 绾︽潫 function/type-promotion spawner |
| `get` | `FieldVariableActionCluster` | 鍙橀噺銆佸瓧娈点€佺粍浠跺紩鐢ㄨ鍙?|
| `set` | `FieldVariableActionCluster` | 鍙橀噺銆佸瓧娈靛啓鍏?|
| `get_property` | `FieldVariableActionCluster` | 绠€鍗?property path锛涘鏉?path 鍙粍鍚?struct/generic fragment |
| `set_property` | `FieldVariableActionCluster` | 绠€鍗?property write锛涘鏉?path 鍙粍鍚?struct/generic fragment |
| `construct` | `GenericAssetStructControlActionCluster` | Make Struct / container / generic construct action |
| `deconstruct` | `GenericAssetStructControlActionCluster` | Break Struct / container / generic deconstruct action |
| `select` | `GenericAssetStructControlActionCluster` | Select 绫?generic data-flow action |

`event`銆乣component_bound_event`銆乣bind`銆乣create`銆乣convert`銆乣schedule`銆乣control` 涓嶅睘浜庢湰鎵瑰畬鏁村疄鐜拌寖鍥达紝浣嗗畠浠悓鏍峰繀椤昏繘鍏?Spawner-Oriented Cluster锛岃€屼笉鏄柊澧炶嚜鐒惰涔夊伐鍏风皣鎴栨棫 handler fallback銆?
绂佹璺緞锛?
1. 涓嶅厑璁告仮澶?`NodeHandler` / parsed-node fallback銆?2. 涓嶅厑璁镐互 `FindFunctionByName()` 浣滀负 action resolution 鍏滃簳銆?3. 涓嶅厑璁稿洜涓烘煇涓?`kind` 宸茬粡瀛樺湪灞€閮ㄥ疄鐜帮紝灏辩粫寮€ `SpawnerClusterResolver`銆?4. 涓嶅厑璁镐繚鐣欐棫 AgentFace aliases 浣滀负闅愯棌鍏煎璺緞锛涙棫瀛楁搴旂洿鎺ユ姤閿欐垨鍦?schema 灞傜Щ闄ゃ€?
## P0 Architecture Correction Before First-Batch Implementation

绗竴鎵规暟鎹祦瀹炵幇鍓嶅繀椤诲厛鍋?P0 鏋舵瀯绾犲亸銆傚師鍥犳槸褰撳墠浠ｇ爜浠嶅瓨鍦ㄦ棫 GraphWrite parsed-node / NodeHandler 鍏滃簳璺緞銆傚鏋滀笉鍏堟柇寮€锛屽悗缁柊澧?`get/set_property/op/construct/deconstruct/select` 浼氱户缁嚭鐜扳€滆涔夊瓧娈靛凡缁忚璁★紝浣?UE 鑺傜偣鍒涘缓浠嶇粫鍥炴棫 NodeHandler鈥濈殑姹℃煋銆?
P0 鐩爣锛?
1. 瀹屽叏鏂紑鎵€鏈夋棫瀹炵幇鍏滃簳銆?2. 鐩存帴绉婚櫎鏃?handler / old node creation path锛屼笉淇濈暀鍙皟鐢?deprecated fallback銆?3. 灏嗘棫瀹炵幇鏂紑鍚庣己澶辩殑閾捐矾琛ラ綈鍒版枃妗ｅ畾涔夌殑澶氱皣鏋舵瀯锛欰gentFace / BlueprintLogicSpec / SemanticIR / Resolver / Pattern Registry / NodeFragment / Composer-Linker / Mutator銆?4. 閫氳繃 NodeFragment 閲嶈矾鐢卞埌鏂板疄鐜扮皣锛岃€屼笉鏄妸鏃?NodeHandler 鍖呬竴灞傜户缁皟鐢ㄣ€?5. 瑙ｆ瀽灞傚繀椤绘敞鍏ュ埌鏂扮绾匡細浠讳綍鏂板鑳藉姏閮藉厛杩涘叆 SemanticIR parser / resolver锛屽啀杩涘叆 FragmentDAG銆?
鏃у疄鐜板鐞嗗師鍒欙細

- `NodeHandlers` / `OperationHandlers` 鍦?P0 涓洿鎺ュ垹闄ゆ簮鐮佹枃浠跺拰 build 寮曠敤銆?- 涓嶅厑璁镐繚鐣?deprecated handler銆乭idden fallback銆亀rapper fallback 鎴栨棫 registry 鏌ヨ銆?- 濡傛灉鍙戠幇鏌愪釜褰撳墠鑳藉姏鍙瓨鍦ㄤ簬鏃?handler 涓紝涓嶈兘鎭㈠ fallback锛涘繀椤绘寜鏂版灦鏋勮ˉ涓€涓?Pattern / FragmentBuilder / Mutator 璺緞銆?
鏂板鑳藉姏鏍囧噯鎺ュ叆姝ラ锛?
```text
1. AgentFace schema / docs 瀹氫箟 canonical semantic shape
2. TS/Python compiler 鍙繚鐣?canonical shape锛屼笉鍋氭棫瀛楁 normalization
3. SemanticIR parser 瑙ｆ瀽 kind 鍜屽瓧娈?4. Semantic Resolver 瑙ｆ瀽 scope / symbol / target / type / candidates
5. Pattern Registry 鏍规嵁 semantic kind + typed context 閫夋嫨 builder
6. NodeFragment Builder 鐢熸垚 fragment
7. FragmentDAG Builder 寤?data / exec edge
8. Graph Composer / Linker 娑堣垂 edge 骞惰繛 pin
9. UE Graph Mutator 鍒涘缓 / 淇敼 UK2Node
10. Review evidence / DebugBundle 娑堣垂鍚屼竴浠?semantic + fragment evidence
11. ReadContext / LogicFlow 杈撳嚭鍚屼竴濂?canonical semantic 淇℃伅
```

涓嶅厑璁革細

- 鏂板 AgentFace 瀛楁鍚庣洿鎺ュ湪 compiler 涓敓鎴?UE 鑺傜偣鍚嶃€?- 鏂板鑳藉姏鏃舵妸鏃?NodeHandler 鍖呰鎴?Pattern銆?- 浠?`if kind == X then NewObject<UK2Node_X>` 鐨勬柟寮忕粫杩?Pattern Registry / NodeFragment銆?- 鍦ㄨВ鏋愬眰鎺ュ彈鏃у瓧娈典綔涓?alias銆?- 涓洪€氳繃娴嬭瘯淇濈暀 hidden fallback銆?
## Canonical First-Batch Surface

Graph body data-flow expression kinds:

```text
get
get_property
op
construct
deconstruct
select
call
literal
```

Graph body statement kinds included in this first implementation:

```text
set
set_property
call
let
```

`call` remains available because expressions need nested calls and statement calls. `control` is intentionally not implemented in this first batch; `branch` and `return` must not be expanded during this plan except where existing smoke tests need to be rewritten or marked outside this batch.

Old-shape rejection reference, not compiler migration:

| Rejected old shape | Required canonical authoring shape |
|---|---|
| `kind="call_function"` + `name` | `kind="call"` + `target` |
| Graph body `kind="set_member_variable"` + `name` | `kind="set"` + `target` |
| `kind="ref"` | `kind="get"` |
| `kind="compare"` + `left/right/operator` | `kind="op"` + `operator/args` |
| `kind="make_struct"` + `type/args` | `kind="construct"` + `type/fields` |

These rows are authoring guidance only. The compiler, SemanticIR parser, and UE path must not normalize, alias, deprecate, or otherwise tolerate these old shapes; they should fail through unsupported-kind diagnostics.

Important distinction:

- `set_member_variable` in this table only means the old Graph body statement shape.
- Current `blueprint_variable` tool-cluster operations remain valid, including `set_member_variable_properties`, `set_member_default`, and local variable operations.
- Do not remove current Blueprint Variable capability while removing the old Graph body alias.

## File Map

### P0 old-path cutoff and extension-point hardening

- Modify `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.cpp`
  - Remove parsed-node / NodeHandler fallback before first-batch implementation.
  - Ensure unsupported semantic kinds stop with diagnostics.
- Delete `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/NodeHandlers/*.h`
  - Remove old node-specific handler declarations.
- Delete `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/NodeHandlers/*.cpp`
  - Remove old node-specific handler implementations and registry.
- Delete `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/OperationHandlers/*.h`
  - Remove old graph operation handler declarations.
- Delete `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/OperationHandlers/*.cpp`
  - Remove old graph operation handler implementations.
- Modify `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphPatternRegistry.h`
  - Define the new ability extension seam.
- Modify `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphPatternRegistry.cpp`
  - Register only new semantic pattern builders.
- Modify `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h`
  - Make NodeFragment builder entrypoints explicit.
- Modify `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
  - Route all first-batch semantic kinds through builder entrypoints.
- Modify `BlueprintHelper/Develop/Plan/BlueprintHelper_DataFlowCore_AgentFaceFields_20260520_CN.md`
  - Add the standard new-capability extension checklist.

### AgentFace TypeScript compiler

- Modify `AgentFaceService/task-core/src/task/schema/task-contract.ts`
  - Update first-slice contract to canonical data-flow kinds.
  - Remove `legacy_statement_kinds`.
- Modify `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
  - Remove old statement aliases and their normalization branches.
  - Add canonical expression validation and lowering for `op`, `construct`, `deconstruct`, `set_property`.
  - Let noncanonical / unknown Graph body kinds fail through the normal unsupported-kind path; do not add deprecation compatibility branches.
- Modify `AgentFaceService/task-core/src/task/fixtures/task-protocol.fixtures.ts`
  - Replace fixtures using `call_function`, `set_member_variable`, `compare`, `make_struct`, and `ref`.
- Modify `AgentFaceService/task-core/src/task/service/task-spec-runner.test.ts`
  - Add compile-level coverage for accepted canonical shapes and rejected old shapes.

### AgentFace Python compiler

- Modify `AgentFaceService/task-core/python/blueprinthelper_task/compiler/graph_write_append.py`
  - Keep Python parity with TypeScript canonical GraphWrite compiler.
  - Replace synthesized `call_function` from interface integration with `call`.
  - Reject old data-flow shapes consistently.
- Modify `AgentFaceService/task-core/python/blueprinthelper_task/compiler/p1_capabilities.py`
  - Do not route Graph body data-flow shapes through asset/component/widget/data-table paths.
- Modify `AgentFaceService/task-core/python/blueprinthelper_task/compiler/p2_capabilities.py`
  - Keep Signature responsibilities separate from Graph body data-flow writes.

### UE SemanticIR and FragmentDAG

- Modify `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`
  - Add explicit semantic fields for `set_property`, `op`, `construct`, `deconstruct`, and canonical `get`.
- Modify `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp`
  - Parse and validate canonical fields.
  - Emit resolver diagnostics for missing construct/deconstruct fields.
- Modify `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphSemanticIRUtils.h`
  - Add focused helpers for expression kind parsing and field extraction.
- Modify `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphSemanticIRUtils.cpp`
  - Implement kind parsing without old aliases.
- Modify `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDag.h`
  - Ensure data edges support expression-to-expression and expression-to-statement consumer links.
- Modify `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDagBuilder.cpp`
  - Build fragment DAGs for the canonical first-batch operations.

### UE semantic resolvers and fragment builders

- Modify `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h`
  - Add builder entrypoints for canonical data-flow fragments.
- Modify `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
  - Implement fragment building for `get`, `set`, `get_property`, `set_property`, `op`, `construct`, `deconstruct`, and `select`.
- Modify `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperStructConstructionResolver.h`
  - Generalize field discovery for construct/deconstruct.
- Modify `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperStructConstructionResolver.cpp`
  - Return available fields and safe default values for construct.
  - Return available fields for deconstruct.
- Modify `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h`
  - Expose typed operator resolution as a resolver service used by `op`.
- Modify `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.cpp`
  - Resolve `op.operator` by typed argument constraints, not by hard-coded node names.
- Modify `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/FunctionResolution/Utils/BlueprintHelperCallFunctionResolverUtils.h`
  - Add reusable operator alias helpers for `>`, `<`, `>=`, `<=`, `==`, `!=`, `+`, `-`, `*`, `/`, `and`, `or`, `not`.
- Modify `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/FunctionResolution/Utils/BlueprintHelperCallFunctionResolverUtils.cpp`
  - Implement alias-to-candidate filtering without special-casing a single UE function.

### UE GraphWrite pipeline and old NodeHandler removal

- Modify `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.cpp`
  - Remove fallback from unsupported semantic statement to `FBlueprintNodeHandlerRegistry`.
  - Remove entry-node creation through NodeHandler.
  - Return unsupported semantic diagnostics instead of spawning parsed legacy nodes.
- Modify `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h`
  - Remove public helpers that only serve old parsed-node spawning.
- Modify `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.cpp`
  - Remove implementations for old parsed-node spawning helpers after call sites are gone.
- Delete `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/NodeHandlers/*.h`
  - Remove old node-specific handler declarations.
- Delete `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/NodeHandlers/*.cpp`
  - Remove old node-specific handler implementations and registry.
- Delete `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/OperationHandlers/*.h`
  - Remove legacy graph operation handlers that create function/macro/dispatcher outside Signature.
- Delete `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/OperationHandlers/*.cpp`
  - Remove legacy graph operation handler implementations after build references are gone.

### Tests and docs

- Modify `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphSemanticIRRuntimeFactTests.cpp`
  - Add UE-side SemanticIR tests for accepted first-batch shapes and rejected old aliases.
- Modify `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperCallFunctionResolverTests.cpp`
  - Add typed operator resolution coverage for canonical `op`.
- Add `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperDataFlowCoreSemanticTests.cpp`
  - Test construct/deconstruct field discovery and set_property/get_property semantics.
- Modify `BlueprintHelper/Source/BlueprintHelper/Private/Tests/UI/BlueprintHelperTaskSpecWorkbenchServicesTests.cpp`
  - Replace old `call_function` examples with canonical `call`.
- Modify `AgentFaceService/docs/TaskSpec_UE_Editor_Capability_Matrix_20260521_CN.md`
  - Mark current first-batch implementation status once tasks complete.
- Modify `BlueprintHelper/Develop/Plan/BlueprintHelper_DataFlowCore_AgentFaceFields_20260520_CN.md`
  - Add implementation status and any verified gaps.
- Modify `BlueprintHelper/Develop/Plan/BlueprintHelper_RemoveLegacyCallFunctionName_ImplementationPlan_20260520_CN.md`
  - Mark superseded by this broader first-batch plan if all old call_function cleanup tasks are absorbed here.

## Slice C Documentation Sync Status (2026-05-21)

This Slice C pass updates documentation after code slices A/B without reading or verifying the current implementation. Only documentation facts completed by this pass are marked complete; implementation and validation items remain pending until backed by code/test/editor evidence.

Canonical Graph body statement/expression path:

```text
AgentFace schema/docs
-> TS/Python compiler
-> SemanticIR parser
-> Resolver
-> Pattern Registry
-> NodeFragment Builder
-> FragmentDAG
-> Composer/Linker
-> UE Mutator
-> Review/Debug
-> ReadContext/LogicFlow
```

- [x] Documentation records that old NodeHandler / parsed-node fallback is not allowed and is not deprecated compatibility.
- [x] Documentation records that old Graph body shapes `call_function`, Graph body `set_member_variable`, `ref`, `compare`, and `make_struct` must error as unsupported kinds rather than being normalized.
- [x] Documentation records the canonical multi-stage path from AgentFace schema/docs through ReadContext/LogicFlow.
- [ ] Code slice A/B implementation status was not verified during this documentation-only pass.
- [ ] TS/Python compiler tests, UE tests, editor preview/execute smoke, compile, and LogicFlow/readback evidence are pending gaps in this document until actual results are recorded.
- [ ] Task checkboxes below intentionally remain unchecked unless supported by known implementation/test evidence.
## Task 1: TypeScript Contract Canonicalization

**Files:**
- Modify: `AgentFaceService/task-core/src/task/schema/task-contract.ts`
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- Modify: `AgentFaceService/task-core/src/task/fixtures/task-protocol.fixtures.ts`
- Modify: `AgentFaceService/task-core/src/task/service/task-spec-runner.test.ts`

- [ ] **Step 1: Replace contract kind lists**

In `TASK_PROTOCOL_CONTRACT_V1.supported_first_slice`, set:

```ts
statement_kinds: ['call', 'set', 'set_property', 'let'],
expression_kinds: ['literal', 'get', 'get_property', 'call', 'op', 'construct', 'deconstruct', 'select'],
legacy_statement_kinds: [],
```

Remove any text that says the compiler normalizes `call_function` or `set_member_variable`.

- [ ] **Step 2: Remove old-shape handling instead of adding deprecation logic**

Do not add a special deprecated-kind detector or compatibility fallback.

The compiler should keep only canonical allowlists. Any noncanonical kind should naturally fail through the existing unsupported-kind path.

```ts
const supportedStatementKinds = new Set(['call', 'set', 'set_property', 'let']);
const supportedExpressionKinds = new Set(['literal', 'get', 'get_property', 'call', 'op', 'construct', 'deconstruct', 'select']);
```

If the input still contains `call_function`, Graph body `set_member_variable`, `ref`, `compare`, or `make_struct`, it should be treated exactly like any unknown unsupported kind. Do not write replacement / deprecation normalization.

- [ ] **Step 3: Remove old statement normalization**

Delete branches that rewrite:

```ts
call_function -> call
set_member_variable -> set
name -> target
```

The compiler should now require:

```json
{ "kind": "call", "target": "PrintString" }
{ "kind": "set", "target": "bDoorOpen", "value": true }
```

- [ ] **Step 4: Replace expression lowering names**

In expression compilation:

```ts
ref -> get
compare -> op
make_struct -> construct
```

Remove the old branches and add canonical branches:

```ts
if (kind === 'get') { ... }
if (kind === 'op') { ... }
if (kind === 'construct') { ... }
if (kind === 'deconstruct') { ... }
if (kind === 'select') { ... }
```

`op` must accept:

```json
{ "kind": "op", "operator": ">", "args": [1, 0] }
```

`construct` must accept:

```json
{ "kind": "construct", "type": "Vector", "fields": { "X": 1, "Y": 2, "Z": 3 } }
```

`deconstruct` with missing `fields` should compile as a resolver-query semantic payload, not as a UE node payload.

- [ ] **Step 5: Update fixtures**

Replace each fixture instance:

```ts
{ kind: 'call_function', name: 'PrintString' }
```

with:

```ts
{ kind: 'call', target: 'PrintString' }
```

Replace:

```ts
{ kind: 'set_member_variable', name: 'bReady' }
```

with:

```ts
{ kind: 'set', target: 'bReady' }
```

Replace compare/make_struct/ref examples using the canonical table in this plan.

- [ ] **Step 6: Add compiler tests**

Add tests in `task-spec-runner.test.ts` for:

```ts
expectCompileAccepts({ kind: 'op', operator: '>', args: [1, 0] });
expectCompileRejects({ kind: 'compare', operator: '>', left: 1, right: 0 }, 'unsupported_graph_write_expression_kind');
expectCompileRejects({ kind: 'call_function', name: 'PrintString' }, 'unsupported_graph_write_statement_kind');
```

The rejection is not a compatibility/deprecation feature; it is the normal unsupported-kind failure after the old implementation is deleted.

- [ ] **Step 7: Run TypeScript tests**

Run from `AgentFaceService/task-core`:

```powershell
npm test -- --runInBand
```

Expected result:

```text
PASS
```

If the repository test runner uses a different script, run the package鈥檚 listed test script from `package.json` and record the exact command in `BlueprintHelper_DataFlowCore_AgentFaceFields_20260520_CN.md`.

## Task 2: Python Compiler Parity

**Files:**
- Modify: `AgentFaceService/task-core/python/blueprinthelper_task/compiler/graph_write_append.py`
- Modify: `AgentFaceService/task-core/python/blueprinthelper_task/compiler/p1_capabilities.py`
- Modify: `AgentFaceService/task-core/python/blueprinthelper_task/compiler/p2_capabilities.py`

- [ ] **Step 1: Replace synthesized interface call**

In `graph_write_append.py`, replace:

```py
{"kind": "call_function", "name": implementation["call"]}
```

with:

```py
{"kind": "call", "target": implementation["call"]}
```

- [ ] **Step 2: Keep only canonical graph body validation**

Add validation sets:

```py
SUPPORTED_STATEMENT_KINDS = {"call", "set", "set_property", "let"}
SUPPORTED_EXPRESSION_KINDS = {"literal", "get", "get_property", "call", "op", "construct", "deconstruct", "select"}
```

Do not add forbidden/deprecated sets. Unknown kinds should fail as unsupported kinds through the same normal path as TypeScript.

- [ ] **Step 3: Keep non-Graph clusters isolated**

Confirm `p1_capabilities.py` and `p2_capabilities.py` do not consume `create`, `set_property`, `bind`, or `schedule` as asset/component/widget/signature operations.

If a graph body semantic appears in those compilers, reject it with:

```text
unsupported_taskspec_semantic_for_cluster
```

- [ ] **Step 4: Run Python compiler checks**

Run from `AgentFaceService/task-core`:

```powershell
python -m pytest
```

Expected result:

```text
passed
```

If no pytest suite exists, run the repository鈥檚 Python compiler smoke script and record the exact command in the data-flow fields document.

## Task 3: SemanticIR Model for Canonical Data Flow

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphSemanticIRUtils.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphSemanticIRUtils.cpp`

- [ ] **Step 1: Add canonical enum values**

Add expression kinds:

```cpp
Get,
GetProperty,
Op,
Construct,
Deconstruct,
Select,
Call,
Literal
```

Add statement kinds:

```cpp
Call,
Set,
SetProperty,
Let
```

Do not add `Compare`, `MakeStruct`, `Ref`, `CallFunction`, or `SetMemberVariable`.

- [ ] **Step 2: Add canonical fields**

Represent the first batch with fields equivalent to:

```cpp
FString Target;
FString TargetKind;
FString Property;
FString Type;
FString Operator;
TMap<FString, FBlueprintHelperGraphSemanticExpression> Fields;
TArray<FBlueprintHelperGraphSemanticExpression> Args;
TSharedPtr<FBlueprintHelperGraphSemanticExpression> Value;
TSharedPtr<FBlueprintHelperGraphSemanticExpression> Condition;
TSharedPtr<FBlueprintHelperGraphSemanticExpression> ThenValue;
TSharedPtr<FBlueprintHelperGraphSemanticExpression> ElseValue;
```

Use existing data-struct style if the file already has equivalent containers; do not introduce a parallel model.

- [ ] **Step 3: Parse canonical JSON**

Implement parsing rules:

```text
get.target required
set.target required
get_property.target required
get_property.property required
set_property.target required
set_property.property required
set_property.value required
op.operator required
op.args non-empty
construct.type optional
construct.fields optional
deconstruct.target required
deconstruct.fields optional
select.condition/then/else required
```

- [ ] **Step 4: Emit missing-field resolver diagnostics**

For `construct` with no `fields`, emit diagnostic:

```text
needs_construct_fields
```

If type cannot be inferred:

```text
needs_construct_type
```

For `deconstruct` with no `fields`, emit diagnostic:

```text
needs_deconstruct_fields
```

These diagnostics must be preview blockers and must not mutate assets.

- [ ] **Step 5: Remove alias parser branches in UE parser**

Delete parser branches for old aliases. If a removed kind reaches the UE parser, it should fail through the normal unsupported semantic kind diagnostic:

```text
unsupported_graph_write_semantic_kind
```

The diagnostic message should report the unsupported kind. It should not preserve a compatibility or deprecation mapping.

## Task 4: FragmentDAG and Builder Support

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDag.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDagBuilder.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`

- [ ] **Step 1: Add fragment outputs for every expression**

Each expression fragment must expose:

```text
FragmentId
PrimaryNode
DataOutputs
ReviewTargets
SourceStatementId
```

Literal expressions may be represented as default-value sources if no UE node is required.

- [ ] **Step 2: Add `get` fragment**

Resolve symbols by priority:

```text
let/local
parameter
member variable
component
```

Use `target_kind` only as a resolver hint.

- [ ] **Step 3: Add `set` fragment**

Build variable set or local variable set from semantic target resolution.

Reject unresolved targets with:

```text
semantic_target_unresolved
```

- [ ] **Step 4: Add `get_property` fragment**

Support property paths such as:

```text
RelativeRotation.Yaw
```

Resolver must choose property access, split pin, or generated getter node based on typed target. Do not use an asset-level ObjectProperty service.

- [ ] **Step 5: Add `set_property` fragment**

Support object and struct property assignment in Graph body only.

Reject attempts where the target resolves to:

```text
asset_default
component_template
widget_tree_design_time
class_default
data_asset_property
```

Use error code:

```text
set_property_scope_not_graph_body
```

- [ ] **Step 6: Add `op` fragment**

Resolve operator using typed argument constraints. The builder should call the function resolver and produce candidate diagnostics when ambiguous.

Supported aliases in this batch:

```text
>, <, >=, <=, ==, !=, +, -, *, /, and, or, not
```

- [ ] **Step 7: Add `construct` fragment**

If fields are present, build a typed construct fragment using the struct construction resolver.

If fields are missing, return the preview diagnostic created by Task 3 and do not spawn a node.

- [ ] **Step 8: Add `deconstruct` fragment**

If fields are present, build one deconstruct fragment with named output ports for requested fields.

If fields are missing, return the preview diagnostic created by Task 3 and do not spawn a node.

- [ ] **Step 9: Add `select` fragment**

Build a select fragment where:

```text
condition -> selection pin
then -> true option
else -> false option
```

Use typed output inference from the consuming pin if available.

## Task 5: Resolver Improvements

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperStructConstructionResolver.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperStructConstructionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/FunctionResolution/Utils/BlueprintHelperCallFunctionResolverUtils.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/FunctionResolution/Utils/BlueprintHelperCallFunctionResolverUtils.cpp`

- [ ] **Step 1: Implement construct field discovery**

Expose a result containing:

```cpp
FString TargetType;
TArray<FBlueprintHelperResolvedStructField> AvailableFields;
bool bTypeInferredFromConsumer;
```

Each field must include:

```cpp
Name
Type
DefaultValue
bHasSafeDefault
```

- [ ] **Step 2: Implement deconstruct field discovery**

Expose the same field list without requiring safe defaults.

- [ ] **Step 3: Implement typed operator candidates**

Add a resolver method equivalent to:

```cpp
ResolveOperator(Operator, ArgumentPinTypes, ConsumerPinType)
```

It should return:

```cpp
CandidateFunctions
ResolvedFunction
AmbiguityDiagnostics
```

- [ ] **Step 4: Keep operator aliases data-driven inside resolver utilities**

Store aliases in one helper table:

```cpp
{ TEXT(">"), TEXT("greater") }
{ TEXT("=="), TEXT("equal") }
{ TEXT("and"), TEXT("boolean_and") }
```

Do not hard-code `Break Vector`, `Make Vector`, or a single `KismetMathLibrary` function in GraphStatementBuilder.

## Task 6: Remove Old NodeHandler and OperationHandler Paths

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.cpp`
- Delete: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/NodeHandlers/*.h`
- Delete: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/NodeHandlers/*.cpp`
- Delete: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/OperationHandlers/*.h`
- Delete: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/OperationHandlers/*.cpp`

- [ ] **Step 1: Remove registry fallback in pipeline**

Remove code equivalent to:

```cpp
IBlueprintNodeHandler* Handler = FBlueprintNodeHandlerRegistry::Get().FindHandler(NodeData.NodeType);
UK2Node* SpawnedNode = Handler ? Handler->Spawn(TargetGraph, NodeData, OutError) : nullptr;
```

Replace with:

```text
unsupported_semantic_fragment_builder
```

and include the rejected semantic kind in diagnostics.

- [ ] **Step 2: Remove entry creation through NodeHandler**

Entry nodes must be produced by Signature dependency steps or existing GraphWrite entry resolution, not by legacy node handlers.

If an append path requires an entry, it must use SemanticIR entry facts and Signature output, not `CustomEventNodeHandler`.

- [ ] **Step 3: Delete NodeHandler includes**

Search and remove includes under:

```text
Systems/ToolClusters/GraphWrite/NodeHandlers/
```

The build must fail if any old handler include remains.

- [ ] **Step 4: Delete OperationHandler includes**

Search and remove includes under:

```text
Systems/ToolClusters/GraphWrite/OperationHandlers/
```

Function, macro, event dispatcher lifecycle belongs to Signature or other dedicated clusters.

- [ ] **Step 5: Remove facade methods only used by old handlers**

Remove declarations and definitions such as:

```cpp
SpawnVariableGetNode
SpawnVariableSetNode
SpawnMacroNode
```

only after all call sites have moved to GraphStatementBuilder / GraphNodeFactory.

- [ ] **Step 6: Run a no-reference search**

Run:

```powershell
rg -n "NodeHandler|FBlueprintNodeHandlerRegistry|OperationHandler|call_function|kind\\s*[:=]\\s*['\\\"]set_member_variable['\\\"]|make_struct|kind\\s*[:=]\\s*['\\\"]ref['\\\"]|kind\\s*[:=]\\s*['\\\"]compare['\\\"]" BlueprintHelper/Source AgentFaceService/task-core AgentFaceService/docs BlueprintHelper/Develop/Plan
```

Expected result:

```text
Only historical plan documents mention these strings.
No source, schema, compiler, fixture, or active guide reference remains.
Current Blueprint Variable operations such as set_member_variable_properties and set_member_default may remain.
```

## Task 7: Tests

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphSemanticIRRuntimeFactTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperCallFunctionResolverTests.cpp`
- Add: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperDataFlowCoreSemanticTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/UI/BlueprintHelperTaskSpecWorkbenchServicesTests.cpp`

- [ ] **Step 1: Add parser acceptance tests**

Cover:

```json
{ "kind": "get", "target": "DoorPanel" }
{ "kind": "set", "target": "bDoorOpen", "value": true }
{ "kind": "get_property", "target": { "kind": "get", "target": "DoorPanel" }, "property": "RelativeRotation.Yaw" }
{ "kind": "set_property", "target": { "kind": "get", "target": "DoorPanel" }, "property": "RelativeRotation", "value": { "kind": "construct", "type": "Rotator", "fields": { "Yaw": 90 } } }
{ "kind": "op", "operator": ">", "args": [1, 0] }
{ "kind": "select", "condition": true, "then": 1, "else": 0 }
```

- [ ] **Step 2: Add unsupported noncanonical-kind tests**

Cover:

```json
{ "kind": "call_function", "name": "PrintString" }
{ "kind": "set_member_variable", "name": "bDoorOpen" }
{ "kind": "compare", "operator": ">", "left": 1, "right": 0 }
{ "kind": "make_struct", "type": "Vector", "args": { "X": 1 } }
{ "kind": "ref", "name": "TempValue" }
```

These tests should assert generic unsupported-kind errors. They must not assert a deprecation warning, compatibility rewrite, or replacement suggestion.

- [ ] **Step 3: Add construct/deconstruct query tests**

Construct missing fields:

```json
{ "kind": "construct", "type": "Rotator" }
```

Expected diagnostic:

```text
needs_construct_fields
```

Deconstruct missing fields:

```json
{ "kind": "deconstruct", "target": { "kind": "get", "target": "HitResult" } }
```

Expected diagnostic:

```text
needs_deconstruct_fields
```

- [ ] **Step 4: Add resolver tests for operator ambiguity**

When `op` cannot resolve uniquely, expect:

```text
candidate_functions
```

with structured function candidates.

- [ ] **Step 5: Add integration smoke TaskSpec fixtures**

Create one smoke TaskSpec for append-owned graph that uses:

```text
get -> op -> select -> construct -> set_property
```

Create another preview-only TaskSpec for:

```text
construct missing fields
deconstruct missing fields
```

## Task 8: Documentation Sync

**Files:**
- Modify: `AgentFaceService/docs/TaskSpec_UE_Editor_Capability_Matrix_20260521_CN.md`
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_DataFlowCore_AgentFaceFields_20260520_CN.md`
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_RemoveLegacyCallFunctionName_ImplementationPlan_20260520_CN.md`

- [ ] **Step 1: Update capability matrix implementation status**

Mark first-batch Graph body data-flow as implemented only after TS, Python, UE tests, and editor-side smoke pass.

If any feature remains partial, record the exact gap:

```text
璺濈鏈熸湜宸窛锛?..
```

- [ ] **Step 2: Update data-flow field document**

Add a section:

```markdown
## 瀹炵幇鐘舵€?```

Use:

```text
[x] 宸插畬鎴?[o] 閮ㄥ垎瀹屾垚
[ ] 鏈畬鎴?```

Do not mark construct/deconstruct two-stage query complete until preview returns candidate field lists without writing assets.

- [ ] **Step 3: Supersede legacy callfunction cleanup plan**

In `BlueprintHelper_RemoveLegacyCallFunctionName_ImplementationPlan_20260520_CN.md`, add:

```markdown
## 鐘舵€?
鏈鍒掑凡骞跺叆 `BlueprintHelper_DataFlowCore_FirstBatch_ImplementationPlan_20260521_CN.md`銆?```

Only do this after the broader cleanup truly covers the old `call_function/name` fields.

## Task 9: Validation and Closed-Loop Smoke

**Files:**
- No source files unless validation finds defects.
- Update docs listed in Task 8 with real results.

- [ ] **Step 1: Compile plugin**

Run the project鈥檚 standard UE build command for BlueprintHelper.

Expected result:

```text
0 errors
```

- [ ] **Step 2: Run TypeScript compiler tests**

Run from `AgentFaceService/task-core`:

```powershell
npm test -- --runInBand
```

Expected result:

```text
PASS
```

- [ ] **Step 3: Run Python compiler tests**

Run from `AgentFaceService/task-core`:

```powershell
python -m pytest
```

Expected result:

```text
passed
```

- [ ] **Step 4: Start editor through global MCP**

Use the global BlueprintHelper MCP editor lifecycle tool when available.

Expected result:

```text
Editor starts and Bridge accepts CLI requests.
```

- [ ] **Step 5: Preview query-only construct/deconstruct TaskSpecs**

Run preview for missing-field `construct` and `deconstruct`.

Expected result:

```text
preview blocked
status includes needs_construct_fields or needs_deconstruct_fields
available_fields is present
modified=false
```

- [ ] **Step 6: Execute first-batch append-owned graph smoke**

Run execute for a graph body using:

```text
get
op
select
construct
set_property
```

Expected result:

```text
execute succeeded
summary.modified=true
Blueprint compiles
LogicJson readback contains the written graph body
```

- [ ] **Step 7: Close editor and compile again**

Close editor through global MCP and compile once more.

Expected result:

```text
0 errors
```

## Task 10: Commit Message Preparation

**Files:**
- No file edits.

- [ ] **Step 1: Prepare manual commit message**

Use the user鈥檚 required format:

```text
鏂板鍐呭锛?1. 瀹炵幇绗竴鎵?Graph body 鏁版嵁娴佽涔夊瓧娈?2. 澧炲姞 construct/deconstruct 涓ら樁娈靛瓧娈靛彂鐜?
淇鍐呭锛?1. 绉婚櫎鏃?NodeHandler / OperationHandler 鍥為€€璺緞锛岄伩鍏嶆柊鏃?GraphWrite 鍒涘缓璺緞姹℃煋

鍙樻洿闇€姹傦細
1. 绉婚櫎 Graph body 鏃у瓧娈?call_function/set_member_variable/ref/compare/make_struct 鐨勫疄鐜拌矾寰?```

- [ ] **Step 2: Provide manual git commands only**

Do not run `git add`, `git commit`, or `git push`.

Output commands:

```powershell
git add <files touched by implementation>
git commit -m "<message>"
```

## Self-Review Checklist

- [x] Plan covers AgentFace TypeScript compiler.
- [x] Plan covers Python compiler parity.
- [x] Plan covers UE SemanticIR / FragmentDAG / GraphStatementBuilder.
- [x] Plan covers construct/deconstruct two-stage resolver behavior.
- [x] Plan covers old NodeHandler and OperationHandler removal.
- [x] Plan preserves Signature, AssetFactory, Component, WidgetTree, DataTable, ObjectProperty, and ClassSettings boundaries.
- [x] Plan includes tests and editor-side smoke.
- [x] Plan includes documentation sync with honest gap reporting.
- [x] Plan avoids keeping legacy fallback as a hidden compatibility path.

## 2026-05-21 Implementation Status Update

- [x] AgentFace `task-core` source and generated `build` output no longer expose `legacy_statement_kinds`, `call_function` Graph body statement support, or `make_struct` expression support in schema/compiler paths.
- [x] UE GraphWrite old `NodeHandler` registry source files were removed, and parsed-node mutation plans now report unsupported instead of routing through the removed registry.
- [x] UE SemanticIR expression kinds were narrowed to canonical data-flow expression kinds; `ref`, `compare`, and `make_struct` enum paths were removed from SemanticIR / FragmentDAG / GraphStatementBuilder / runtime naming.
- [x] UE Blueprint structure service no longer depends on the old GraphWrite `OperationHandler` registry for variable, graph, and dispatcher structure operations.
- [x] `npm.cmd run build` passed in `AgentFaceService/task-core`.
- [ ] UE compile is not complete. `Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex` was blocked before C++ compilation by stale package artifacts under `Plugins\BlueprintHelper\Saved\B53`, `Saved\B54`, `Saved\B55`, `Saved\B56`, `Saved\B57`, and `Saved\BuildPlugin_*`, which duplicate `BlueprintHelper.Build.cs` and trigger UBT `CS0101/CS0111` RulesError. Gap: clean or isolate those Saved build-package directories, then rerun compile.
- [ ] Editor-side preview/execute smoke is not complete because UE compile is blocked before C++ validation.
