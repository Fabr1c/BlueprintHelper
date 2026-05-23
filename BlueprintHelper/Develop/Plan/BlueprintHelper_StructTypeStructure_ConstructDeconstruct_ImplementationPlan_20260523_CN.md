# BlueprintHelper Struct / TypeStructure Construct-Deconstruct Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 收敛 `construct` / `deconstruct` 到 `Struct` 或 `TypeStructure` 语义族，并以 `type_operation=construct|deconstruct` 表达具体操作；本计划不合并 broad `create`，直到 struct evidence 规则稳定后再单独处理 broad create。

**Architecture:** `AgentFace TaskSpec -> Semantic IR -> ActionContextPipeline -> ActionResolutionCore -> GenericAssetStructControlActionCluster -> Struct/TypeStructure resolver -> UE NodeSpawner evidence -> shared adapter/composer lifecycle -> UE Graph Mutator`。`construct/deconstruct` 不再作为一级 ActionResolution 请求类型，也不走旧 `make_struct/break_struct` 搜索兜底；它们只作为语义约束和类型操作进入 Generic cluster。

**Tech Stack:** UE 5.6, C++, BlueprintGraph, `ActionContextPipeline`, `ActionResolutionCore`, `GenericAssetStructControlActionCluster`, `UBlueprintNodeSpawner`, `UBlueprintFieldNodeSpawner`, `UBlueprintFunctionNodeSpawner`, BlueprintHelper CLI, UE Automation Tests.

---

## 1. Scope

### In scope

- [x] 将 `construct` / `deconstruct` 规范为 `SemanticFamily=Struct|TypeStructure`。
- [x] 使用 `TypeOperation=Construct|Deconstruct` 作为 resolver 和 evidence 的唯一操作语义。
- [x] 让 `construct/deconstruct` 继续通过 `SpawnerClusterKind=GenericAssetStructControlAction` 分发。
- [x] 在 ActionContextPipeline 中收集和投影 struct/type evidence。
- [x] 提供清晰的 UE spawner evidence，包括 `struct_path`、`type_structure_id`、`type_operation`、`spawner_class`、`node_class`、`stable_id`、`match_reason`。
- [x] 抽离 Struct/TypeStructure resolver，避免 `GenericAssetStructControlActionResolver` 继续膨胀。
- [x] 将成功路径收敛到 UE action / NodeSpawner evidence；只有 UE evidence 无法表达的结构化场景才允许进入显式、通用的 dedicated FragmentBuilder boundary。

### Out of scope

- [x] 不处理 broad `create`、`asset_action`、`spawn_actor`、`create_widget`、`construct_object`。
- [x] 不处理 `convert`、`schedule`、`latent_or_async`。
- [x] 不恢复旧 AgentFace alias，例如 `make_struct` / `break_struct`。
- [x] 不引入 NodeHandler fallback、ParsedNode fallback、旧 Graph JSON fallback。
- [x] 不允许通过全局宽扫描直接选中成功；宽扫描只能作为显式诊断或候选解释，不能成为默认成功路径。

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

- [x] 如果 `construct/deconstruct` 被解析成 `FunctionActionCluster`，必须失败测试。
- [x] 如果 `construct/deconstruct` 被解析成 `create` 或 `create_operation`，必须失败测试。
- [x] 如果 resolver 只能依靠旧 `make_struct` / `break_struct` 字符串 token 才能成功，必须失败测试。
- [x] 如果缺少 struct/type evidence，返回 `NeedsMoreSemanticContext`，不能 silently fallback。

---

## 3. Files to modify

### Action context and DTO

- [x] `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h`
- [x] `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
- [x] `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.cpp`
- [x] `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.cpp`

### Action resolution

- [x] `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h`
- [x] `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.cpp`
- [x] `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionResolver.h`
- [x] `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionResolver.cpp`

### New resolver extraction

- [x] `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperStructTypeStructureActionResolver.h`
- [x] `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperStructTypeStructureActionResolver.cpp`

### Fragment and composer lifecycle

- [x] `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
- [x] `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphSemanticIRUtils.cpp`
- [x] `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGraphNodeSpawnAdapter.cpp`

### Tests

- [x] `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericAssetStructControlActionClusterTests.cpp`
- [x] `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFourClusterContextConsumptionTests.cpp`

### Docs

- [x] `BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
- [x] `BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`
- [x] `AgentFaceService/docs/TaskSpec_UE_Editor_Capability_Matrix_20260521_CN.md`

---

## 4. Implementation steps

### Step 1: Add taxonomy tests first

- [x] Add a test case for `construct FVector`.
- [x] Add a test case for `deconstruct FVector`.
- [x] Assert both requests resolve to `SpawnerClusterKind=GenericAssetStructControlAction`.
- [x] Assert `SemanticFamily=Struct` or `TypeStructure`.
- [x] Assert `TypeOperation=Construct` or `Deconstruct`.
- [x] Assert no selected evidence contains `create_operation`.
- [x] Assert no selected evidence requires legacy `make_struct` / `break_struct` AgentFace tokens.

Expected failure before implementation:

```text
construct/deconstruct taxonomy does not expose canonical SemanticFamily + TypeOperation evidence.
```

### Step 2: Extend semantic DTO and string conversion

- [x] Add `EBlueprintHelperActionSemanticFamily`.
- [x] Add `EBlueprintHelperTypeOperation`.
- [x] Add string conversion helpers for logs, debug bundle, CLI preview, and test assertions.
- [x] Add `SemanticFamily` and `TypeOperation` to `FBlueprintHelperActionSemanticConstraints`.
- [x] Add struct evidence fields to the selected evidence DTO.

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

- [x] Map compact `construct` to `SemanticFamily=Struct|TypeStructure` and `TypeOperation=Construct`.
- [x] Map compact `deconstruct` to `SemanticFamily=Struct|TypeStructure` and `TypeOperation=Deconstruct`.
- [x] Preserve compact kind only as source syntax metadata when needed for diagnostics.
- [x] Ensure `ActionResolutionCore` still dispatches only by `ClusterKind`.
- [x] Remove any code path where `Construct` or `Deconstruct` is treated as a first-level ActionResolution request type.

Acceptance check:

```text
ActionResolutionCore does not switch on construct/deconstruct as top-level request kinds.
```

### Step 4: Build struct/type demand in ActionContextPipeline

- [x] Demand collector identifies `construct/deconstruct` statements and requests struct/type evidence.
- [x] Inference service fills `RequestedStructPath` when TaskSpec gives explicit type.
- [ ] Inference service derives type from linked typed pins when input/output links are already available.
- [x] Bundle projector emits one canonical request per construct/deconstruct action.
- [x] Missing type returns a structured `NeedsMoreSemanticContext` response with concise candidate hints.

Missing context response shape:

```json
{
  "status": "needs_more_semantic_context",
  "missing": ["struct_type"],
  "candidate_types": []
}
```

### Step 5: Extract Struct/TypeStructure resolver

- [x] Create `FBlueprintHelperStructTypeStructureActionResolver`.
- [x] Move construct/deconstruct-specific logic out of `FBlueprintHelperGenericAssetStructControlActionResolver`.
- [x] Let `GenericAssetStructControlActionCluster` delegate only when `SemanticFamily=Struct|TypeStructure`.
- [x] Keep `select/control` in their existing Generic cluster path.
- [x] Do not merge with broad `create`.

Resolver responsibility:

```text
Input: projected context + SemanticFamily + TypeOperation + type evidence.
Output: selected UE spawner evidence, ambiguous candidate list, or NeedsMoreSemanticContext.
```

### Step 6: Resolver selection order

- [x] First, try UE ActionDatabase / ActionFilter evidence where it can express native make/break or function-backed struct operations.
- [x] Second, try `UBlueprintFieldNodeSpawner` style MakeStruct / BreakStruct evidence for known `UScriptStruct`.
- [x] Third, use a dedicated FragmentBuilder boundary only for generic struct operations that UE action evidence cannot represent.
- [x] Mark dedicated boundary explicitly with `bRequiresDedicatedFragmentBuilder=true`.
- [x] Reject success if selected evidence came from unrelated broad `create`.

Selection rules:

```text
Exact struct path + matching type operation wins.
Exact type name + unique resolved struct wins.
Multiple matching structs returns ambiguous candidates.
No struct evidence returns NeedsMoreSemanticContext.
Wide global scan never auto-wins.
```

### Step 7: Shared adapter and lifecycle consumption

- [x] Make `BlueprintHelperGraphNodeSpawnAdapter` consume selected Struct/TypeStructure evidence.
- [x] Route post-spawn defaults through shared adapter lifecycle.
- [x] Route pin normalization through shared adapter lifecycle.
- [x] Route post-link reconstruct through shared composer lifecycle.
- [x] Ensure `GraphStatementBuilder` no longer branches on `make_struct` / `break_struct` style tokens for success.

Acceptance check:

```text
construct/deconstruct nodes are created from selected evidence, not from local string matching in builder code.
```

### Step 8: CLI preview response

- [x] Preview success returns minimal success when evidence is unique.
- [ ] Preview ambiguity returns concise candidate list.
- [x] Preview missing type returns `NeedsMoreSemanticContext`.
- [x] Preview must include evidence path in `full_result`, not in the compact default response.

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

- [x] Remove active success paths that search by `make_struct`.
- [x] Remove active success paths that search by `break_struct`.
- [x] Remove active success paths that interpret `construct` as broad `create`.
- [x] Keep diagnostic strings only if they are clearly marked as unsupported or deprecated and cannot be selected as success.
- [x] Update gap doc if any legacy code cannot be removed in this pass.

Static scan commands:

```powershell
rg -n "make_struct|break_struct|create_operation|Construct|Deconstruct" BlueprintHelper/Source/BlueprintHelper
rg -n "TObjectIterator<UScriptStruct|TObjectIterator<UFunction>" BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite
```

### Step 10: Runtime smoke specs

- [x] Add or update one TaskSpec for `construct FVector`.
- [x] Add or update one TaskSpec for `deconstruct FVector`.
- [x] Add one missing-type preview spec that must return `NeedsMoreSemanticContext`.
- [x] Run preview before execute.
- [x] Execute only after preview succeeds.
- [x] Compile the generated Blueprint after execute.

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

- [x] Update `BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md` with the canonical `Struct/TypeStructure + type_operation` rule.
- [x] Update `BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md` to close the construct/deconstruct taxonomy gap or list exact remaining gap.
- [x] Update AgentFace capability matrix so `construct/deconstruct` are not described as broad `create`.
- [x] Mark this implementation plan checkboxes only after the corresponding code or doc change is actually complete.

---

## 5. Acceptance criteria

- [x] `construct/deconstruct` are not first-level ActionResolution request kinds.
- [x] `construct/deconstruct` resolve through `GenericAssetStructControlActionCluster`.
- [x] Canonical evidence exposes `SemanticFamily=Struct|TypeStructure`.
- [x] Canonical evidence exposes `TypeOperation=Construct|Deconstruct`.
- [x] Broad `create` is untouched and remains out of scope.
- [x] Missing struct/type context returns `NeedsMoreSemanticContext`.
- [ ] Ambiguous struct/type context returns candidates.
- [x] Successful node creation consumes UE spawner evidence or an explicitly marked dedicated struct builder boundary.
- [x] No old NodeHandler, ParsedNode, direct string token, or broad create fallback is used for successful construct/deconstruct creation.
- [x] Runtime smoke preview passes.
- [x] Runtime smoke execute passes.
- [x] Generated Blueprint compiles.
- [x] Design and gap docs are synchronized.

---

## 6. Blocker policy

If implementation discovers that UE 5.6 cannot expose MakeStruct/BreakStruct through stable NodeSpawner evidence for a required struct operation, do not route through old fallback. Record the blocker in `BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md` and implement a dedicated, generic `Struct/TypeStructure` FragmentBuilder boundary with explicit evidence.

## 2026-05-24 执行记录

- [x] UBT 编译通过：`Build.bat TemplateEditor Win64 Development D:\UEProjects\Template\Template.uproject -WaitMutex`。
- [x] 自动化通过：`BlueprintHelper.GraphWrite.ActionResolution.Generic.P5.StructMakeBreakEvidence`。
- [x] 自动化通过：`BlueprintHelper.GraphWrite.ActionContext.SingleDemand.ConstructMapsToStructTypeOperation`。
- [x] 自动化通过：`BlueprintHelper.GraphWrite.ActionContext.DTO.SourceContract`。
- [x] 自动化通过：`BlueprintHelper.GraphWrite.LegacyMainline.ActiveGraphWriteSourceLegacyTokenGate`。
- [x] CLI smoke 通过：创建 `/Game/BlueprintHelperCliSmoke/StructTypeStructure_20260524/BP_STS_ConstructDeconstruct_20260524_002`，task_run_id=`task_26284C3F4488611AA1F396BB513D3D12`。
- [x] CLI smoke 通过：`construct Vector` preview/execute/compile，task_run_id=`task_56400BE64B1DFBD69A2D4084F25CDA3C`。
- [x] CLI smoke 通过：`deconstruct Vector` preview/execute/compile，task_run_id=`task_6FD2084745099E2E9743B9BC2C8031D5`。
- [x] CLI missing-type preview 返回 `needs_more_semantic_context`，preview_id=`preview_1779556782697_0001`。
- [x] 源码 token gate：`rg -n "make_struct|break_struct|create_operation" BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite BlueprintHelper\Source\BlueprintHelper\Public\Systems\ToolClusters\GraphWrite` 无命中。

### 距离完整期望的差距

- [ ] `Inference service derives type from linked typed pins when input/output links are already available.` 本次未单独构造 linked typed pin 推断用例；当前验证覆盖显式 `type=Vector` 和 missing-type 阻断。
- [ ] `Preview ambiguity returns concise candidate list.` 本次未构造多候选歧义用例；当前验证覆盖唯一成功和缺上下文阻断。