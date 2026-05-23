# BlueprintHelper Struct / TypeStructure Construct-Deconstruct Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 收敛 `construct` / `deconstruct` 到 `Struct` 或 `TypeStructure` 语义族，并以 `type_operation=construct|deconstruct` 表达具体操作；本计划不合并 broad `create`，直到 struct evidence 规则稳定后再单独处理 broad create。

**Architecture:** `AgentFace TaskSpec -> Semantic IR -> ActionContextPipeline -> ActionResolutionCore -> GenericAssetStructControlActionCluster -> Struct/TypeStructure resolver -> UE NodeSpawner evidence -> shared adapter/composer lifecycle -> UE Graph Mutator`。`construct/deconstruct` 不再作为一级 ActionResolution 请求类型，也不走旧 `make_struct/break_struct` 搜索兜底；它们只作为语义约束和类型操作进入 Generic cluster。

**Tech Stack:** UE 5.6, C++, BlueprintGraph, `ActionContextPipeline`, `ActionResolutionCore`, `GenericAssetStructControlActionCluster`, `UBlueprintNodeSpawner`, `UBlueprintFieldNodeSpawner`, `UBlueprintFunctionNodeSpawner`, BlueprintHelper CLI, UE Automation Tests.

---

## 1. Scope

### In scope

- [ ] 将 `construct` / `deconstruct` 规范为 `SemanticFamily=Struct|TypeStructure`。
- [ ] 使用 `TypeOperation=Construct|Deconstruct` 作为 resolver 和 evidence 的唯一操作语义。
- [ ] 让 `construct/deconstruct` 继续通过 `SpawnerClusterKind=GenericAssetStructControlAction` 分发。
- [ ] 在 ActionContextPipeline 中收集和投影 struct/type evidence。
- [ ] 提供清晰的 UE spawner evidence，包括 `struct_path`、`type_structure_id`、`type_operation`、`spawner_class`、`node_class`、`stable_id`、`match_reason`。
- [ ] 抽离 Struct/TypeStructure resolver，避免 `GenericAssetStructControlActionResolver` 继续膨胀。
- [ ] 将成功路径收敛到 UE action / NodeSpawner evidence；只有 UE evidence 无法表达的结构化场景才允许进入显式、通用的 dedicated FragmentBuilder boundary。

### Out of scope

- [ ] 不处理 broad `create`、`asset_action`、`spawn_actor`、`create_widget`、`construct_object`。
- [ ] 不处理 `convert`、`schedule`、`latent_or_async`。
- [ ] 不恢复旧 AgentFace alias，例如 `make_struct` / `break_struct`。
- [ ] 不引入 NodeHandler fallback、ParsedNode fallback、旧 Graph JSON fallback。
- [ ] 不允许通过全局宽扫描直接选中成功；宽扫描只能作为显式诊断或候选解释，不能成为默认成功路径。

---

## 2. Canonical taxonomy

### Canonical SemanticConstraints shape

```cpp
struct FBlueprintHelperActionSemanticConstraints
{
	EBlueprintHelperSpawnerClusterKind ClusterKind = EBlueprintHelperSpawnerClusterKind::Unknown;
	EBlueprintHelperActionSemanticFamily SemanticFamily = EBlueprintHelperActionSemanticFamily::Unknown;
	EBlueprintHelperTypeOperation TypeOperation = EBlueprintHelperTypeOperation::None;
	FString RequestedTypeName;
	FSoftObjectPath RequestedStructPath;
	TArray<FBlueprintHelperTypedPinConstraint> InputPins;
	TArray<FBlueprintHelperTypedPinConstraint> OutputPins;
};
```

### Required mapping

| AgentFace compact kind | ClusterKind | SemanticFamily | TypeOperation | Notes |
| --- | --- | --- | --- | --- |
| `construct` | `GenericAssetStructControlAction` | `Struct` or `TypeStructure` | `Construct` | 构造 struct/type value，不等于 broad `create`。 |
| `deconstruct` | `GenericAssetStructControlAction` | `Struct` or `TypeStructure` | `Deconstruct` | 分解 struct/type value，不等于 broad `create`。 |

### Rejection rules

- [ ] 如果 `construct/deconstruct` 被解析成 `FunctionActionCluster`，必须失败测试。
- [ ] 如果 `construct/deconstruct` 被解析成 `create` 或 `create_operation`，必须失败测试。
- [ ] 如果 resolver 只能依靠旧 `make_struct` / `break_struct` 字符串 token 才能成功，必须失败测试。
- [ ] 如果缺少 struct/type evidence，返回 `NeedsMoreSemanticContext`，不能 silently fallback。

---

## 3. Files to modify

### Action context and DTO

- [ ] `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h`
- [ ] `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
- [ ] `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.cpp`
- [ ] `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.cpp`

### Action resolution

- [ ] `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h`
- [ ] `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.cpp`
- [ ] `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionResolver.h`
- [ ] `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionResolver.cpp`

### New resolver extraction

- [ ] `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperStructTypeStructureActionResolver.h`
- [ ] `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperStructTypeStructureActionResolver.cpp`

### Fragment and composer lifecycle

- [ ] `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
- [ ] `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphSemanticIRUtils.cpp`
- [ ] `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGraphNodeSpawnAdapter.cpp`

### Tests

- [ ] `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericAssetStructControlActionClusterTests.cpp`
- [ ] `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFourClusterContextConsumptionTests.cpp`

### Docs

- [ ] `BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
- [ ] `BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`
- [ ] `AgentFaceService/docs/TaskSpec_UE_Editor_Capability_Matrix_20260521_CN.md`

---

## 4. Implementation steps

### Step 1: Add taxonomy tests first

- [ ] Add a test case for `construct FVector`.
- [ ] Add a test case for `deconstruct FVector`.
- [ ] Assert both requests resolve to `SpawnerClusterKind=GenericAssetStructControlAction`.
- [ ] Assert `SemanticFamily=Struct` or `TypeStructure`.
- [ ] Assert `TypeOperation=Construct` or `Deconstruct`.
- [ ] Assert no selected evidence contains `create_operation`.
- [ ] Assert no selected evidence requires legacy `make_struct` / `break_struct` AgentFace tokens.

Expected failure before implementation:

```text
construct/deconstruct taxonomy does not expose canonical SemanticFamily + TypeOperation evidence.
```

### Step 2: Extend semantic DTO and string conversion

- [ ] Add `EBlueprintHelperActionSemanticFamily`.
- [ ] Add `EBlueprintHelperTypeOperation`.
- [ ] Add string conversion helpers for logs, debug bundle, CLI preview, and test assertions.
- [ ] Add `SemanticFamily` and `TypeOperation` to `FBlueprintHelperActionSemanticConstraints`.
- [ ] Add struct evidence fields to the selected evidence DTO.

Required evidence fields:

```cpp
FString StructPath;
FString TypeStructureId;
FString TypeOperation;
FString SpawnerClass;
FString NodeClass;
FString StableId;
FString MatchReason;
bool bRequiresDedicatedFragmentBuilder = false;
```

### Step 3: Canonical lowering from AgentFace kind

- [ ] Map compact `construct` to `SemanticFamily=Struct|TypeStructure` and `TypeOperation=Construct`.
- [ ] Map compact `deconstruct` to `SemanticFamily=Struct|TypeStructure` and `TypeOperation=Deconstruct`.
- [ ] Preserve compact kind only as source syntax metadata when needed for diagnostics.
- [ ] Ensure `ActionResolutionCore` still dispatches only by `ClusterKind`.
- [ ] Remove any code path where `Construct` or `Deconstruct` is treated as a first-level ActionResolution request type.

Acceptance check:

```text
ActionResolutionCore does not switch on construct/deconstruct as top-level request kinds.
```

### Step 4: Build struct/type demand in ActionContextPipeline

- [ ] Demand collector identifies `construct/deconstruct` statements and requests struct/type evidence.
- [ ] Inference service fills `RequestedStructPath` when TaskSpec gives explicit type.
- [ ] Inference service derives type from linked typed pins when input/output links are already available.
- [ ] Bundle projector emits one canonical request per construct/deconstruct action.
- [ ] Missing type returns a structured `NeedsMoreSemanticContext` response with concise candidate hints.

Missing context response shape:

```json
{
  "status": "needs_more_semantic_context",
  "missing": ["struct_type"],
  "candidate_types": []
}
```

### Step 5: Extract Struct/TypeStructure resolver

- [ ] Create `FBlueprintHelperStructTypeStructureActionResolver`.
- [ ] Move construct/deconstruct-specific logic out of `FBlueprintHelperGenericAssetStructControlActionResolver`.
- [ ] Let `GenericAssetStructControlActionCluster` delegate only when `SemanticFamily=Struct|TypeStructure`.
- [ ] Keep `select/control` in their existing Generic cluster path.
- [ ] Do not merge with broad `create`.

Resolver responsibility:

```text
Input: projected context + SemanticFamily + TypeOperation + type evidence.
Output: selected UE spawner evidence, ambiguous candidate list, or NeedsMoreSemanticContext.
```

### Step 6: Resolver selection order

- [ ] First, try UE ActionDatabase / ActionFilter evidence where it can express native make/break or function-backed struct operations.
- [ ] Second, try `UBlueprintFieldNodeSpawner` style MakeStruct / BreakStruct evidence for known `UScriptStruct`.
- [ ] Third, use a dedicated FragmentBuilder boundary only for generic struct operations that UE action evidence cannot represent.
- [ ] Mark dedicated boundary explicitly with `bRequiresDedicatedFragmentBuilder=true`.
- [ ] Reject success if selected evidence came from unrelated broad `create`.

Selection rules:

```text
Exact struct path + matching type operation wins.
Exact type name + unique resolved struct wins.
Multiple matching structs returns ambiguous candidates.
No struct evidence returns NeedsMoreSemanticContext.
Wide global scan never auto-wins.
```

### Step 7: Shared adapter and lifecycle consumption

- [ ] Make `BlueprintHelperGraphNodeSpawnAdapter` consume selected Struct/TypeStructure evidence.
- [ ] Route post-spawn defaults through shared adapter lifecycle.
- [ ] Route pin normalization through shared adapter lifecycle.
- [ ] Route post-link reconstruct through shared composer lifecycle.
- [ ] Ensure `GraphStatementBuilder` no longer branches on `make_struct` / `break_struct` style tokens for success.

Acceptance check:

```text
construct/deconstruct nodes are created from selected evidence, not from local string matching in builder code.
```

### Step 8: CLI preview response

- [ ] Preview success returns minimal success when evidence is unique.
- [ ] Preview ambiguity returns concise candidate list.
- [ ] Preview missing type returns `NeedsMoreSemanticContext`.
- [ ] Preview must include evidence path in `full_result`, not in the compact default response.

Candidate response example:

```json
{
  "construct": [
    {
      "display_name": "Make Vector",
      "struct_path": "/Script/CoreUObject.Vector",
      "type_operation": "construct",
      "confidence": "high"
    }
  ]
}
```

### Step 9: Remove active legacy surface

- [ ] Remove active success paths that search by `make_struct`.
- [ ] Remove active success paths that search by `break_struct`.
- [ ] Remove active success paths that interpret `construct` as broad `create`.
- [ ] Keep diagnostic strings only if they are clearly marked as unsupported or deprecated and cannot be selected as success.
- [ ] Update gap doc if any legacy code cannot be removed in this pass.

Static scan commands:

```powershell
rg -n "make_struct|break_struct|create_operation|Construct|Deconstruct" BlueprintHelper/Source/BlueprintHelper
rg -n "TObjectIterator<UScriptStruct|TObjectIterator<UFunction>" BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite
```

### Step 10: Runtime smoke specs

- [ ] Add or update one TaskSpec for `construct FVector`.
- [ ] Add or update one TaskSpec for `deconstruct FVector`.
- [ ] Add one missing-type preview spec that must return `NeedsMoreSemanticContext`.
- [ ] Run preview before execute.
- [ ] Execute only after preview succeeds.
- [ ] Compile the generated Blueprint after execute.

Required commands:

```powershell
bh task preview --file <construct_deconstruct_smoke.json> --select status,summary,artifacts.full_result
bh task execute --file <construct_deconstruct_smoke.json> --select status,summary,artifacts.full_result
```

Compile command:

```powershell
& "E:/UE_5.6/Engine/Build/BatchFiles/Build.bat" TemplateEditor Win64 Development "D:/UEProjects/Template/Template.uproject" -WaitMutex
```

### Step 11: Documentation sync

- [ ] Update `BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md` with the canonical `Struct/TypeStructure + type_operation` rule.
- [ ] Update `BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md` to close the construct/deconstruct taxonomy gap or list exact remaining gap.
- [ ] Update AgentFace capability matrix so `construct/deconstruct` are not described as broad `create`.
- [ ] Mark this implementation plan checkboxes only after the corresponding code or doc change is actually complete.

---

## 5. Acceptance criteria

- [ ] `construct/deconstruct` are not first-level ActionResolution request kinds.
- [ ] `construct/deconstruct` resolve through `GenericAssetStructControlActionCluster`.
- [ ] Canonical evidence exposes `SemanticFamily=Struct|TypeStructure`.
- [ ] Canonical evidence exposes `TypeOperation=Construct|Deconstruct`.
- [ ] Broad `create` is untouched and remains out of scope.
- [ ] Missing struct/type context returns `NeedsMoreSemanticContext`.
- [ ] Ambiguous struct/type context returns candidates.
- [ ] Successful node creation consumes UE spawner evidence or an explicitly marked dedicated struct builder boundary.
- [ ] No old NodeHandler, ParsedNode, direct string token, or broad create fallback is used for successful construct/deconstruct creation.
- [ ] Runtime smoke preview passes.
- [ ] Runtime smoke execute passes.
- [ ] Generated Blueprint compiles.
- [ ] Design and gap docs are synchronized.

---

## 6. Blocker policy

If implementation discovers that UE 5.6 cannot expose MakeStruct/BreakStruct through stable NodeSpawner evidence for a required struct operation, do not route through old fallback. Record the blocker in `BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md` and implement a dedicated, generic `Struct/TypeStructure` FragmentBuilder boundary with explicit evidence.
