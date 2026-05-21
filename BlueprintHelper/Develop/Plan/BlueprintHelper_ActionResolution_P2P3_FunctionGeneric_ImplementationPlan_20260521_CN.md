# BlueprintHelper ActionResolution P2/P3 Function + Generic Cluster Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Do not add legacy fallback, do not keep old AgentFace aliases, and do not commit automatically.

**Goal:** Complete P2/P3 after FieldVariable P1 by migrating `op` to UE-style promotable operator spawning, then defining and implementing `GenericAssetStructControlActionCluster` for `construct/deconstruct/select/control` with explicit provider boundaries.

**Architecture:** `ActionResolutionCore` continues to dispatch only by `SpawnerClusterKind`; `op/construct/deconstruct/select/control` are `SemanticConstraints` consumed inside the selected cluster. P2 must follow UE 5.6 operator behavior: create `UK2Node_PromotableOperator` through `FTypePromotion::GetOperatorSpawner(OpName)` when no dragged-pin-equivalent typed context exists, then let UE Schema pin linking and `NotifyPinConnectionListChanged` drive promotion. P3 first classifies which generic/struct/control semantics can be expressed by UE NodeSpawner/ActionDatabase and only uses dedicated FragmentBuilder paths for ActionDatabase-unrepresentable operations.

**Tech Stack:** UE 5.6, C++, BlueprintGraph, BlueprintActionDatabase, BlueprintActionFilter, UBlueprintNodeSpawner, UBlueprintFunctionNodeSpawner, `FTypePromotion`, `UK2Node_PromotableOperator`, Automation Tests, BlueprintHelper CLI.

---

## Current Baseline

- P0/P1 established `ClusterKind + SemanticConstraints` as the contract.
- `FunctionActionCluster` resolves `Call`; P2 must not model `Op` as a fake `Call` or as a concrete overload lookup problem.
- `GenericAssetStructControlActionCluster` owns `Construct/Deconstruct/Select/Control/Create/Convert/Schedule` but currently returns `generic_asset_struct_control_action_cluster_migration_pending`.
- `GraphStatementBuilder` already maps expression `op` to `FunctionActionCluster` and `construct/deconstruct/select/control` to `GenericAssetStructControlActionCluster`.
- No old `NodeHandler`, parsed-node fallback, direct node creation fallback, or old AgentFace alias may be reintroduced.
- UE 5.6 source confirms the native operator path:
  - `UBlueprintFunctionNodeSpawner::Create()` selects `UK2Node_PromotableOperator` for promotion-ready operator functions.
  - `FTypePromotion::GetOperatorSpawner(OpName)` returns the registered operator spawner.
  - UE tests spawn promotable operators first, then call `UEdGraphSchema_K2::TryCreateConnection()` and `UK2Node_PromotableOperator::NotifyPinConnectionListChanged()` to propagate pin types.
- Any current implementation that keeps adding concrete aliases such as `Greater_DoubleDouble` as the primary `op` strategy is a direction error and must be removed or demoted to a narrow explicit concrete-function fallback only when promotable operators cannot represent the requested semantic.

### Current Architecture Gap: UE NodeSpawner Convergence

以下差异是当前实现距离“所有可由 UE NodeSpawner 表达的节点统一走同一生成路径”的真实缺口。`Schema Menu Builder` 不纳入目标，因为 Agent 侧无法使用编辑器菜单 UI 上下文；BlueprintHelper 自身的 preview、candidate、缓存等 AgentFace 工作流也不作为缺口记录。

- `call` 解析已进入 `ActionResolutionCore -> FunctionActionCluster`，但节点创建仍经过 `FBlueprintHelperCallFunctionResolver::SpawnResolvedNode` 的专用路径。期望：所有可由 UE NodeSpawner 表达的 `call` 最终统一走 `ActionResolutionCore -> SpawnerCluster -> SelectedSpawner->Invoke()`。
- `get/set` 已进入 `FieldVariableActionCluster`，但仍需确认运行面没有保留变量节点直接创建 shortcut。期望：语义约束可以由 BlueprintHelper 构造，但 spawn 层只能消费 cluster 返回的 `SelectedSpawner` 并统一 `Invoke`。
- `op` 已使用 UE `FTypePromotion::GetOperatorSpawner()`，但 literal 类型提示、默认值应用、连接后的 promotion 通知仍是 `op` 附近的专用适配。期望：这些 post-spawn / post-link 生命周期处理沉到通用 spawner adapter / composer lifecycle 边界，避免后续每个节点类别各自实现。
- `construct/deconstruct` 已有底层 struct resolver 能创建 `MakeStruct/BreakStruct` 或 native make function，但还没有按当前架构明确区分“可由 NodeSpawner 表达”与“需要专用 FragmentBuilder”。期望：优先复用 UE spawner/action provider；UE ActionDatabase 无法表达时才进入专用 builder。
- `select/control` 仍处于专用 builder seam 阶段。期望：`select` 使用 UE 公开节点 API 完成 wildcard option pin 生命周期，`control` 使用专用控制流 FragmentBuilder 组合 exec DAG；两者都不能伪装成 `call`，也不能恢复旧 fallback。

---

## File Responsibility Map

### P2 FunctionActionCluster / op

- Boundary: `FunctionActionCluster` owns `op` only because UE registers promotable operators as `UBlueprintFunctionNodeSpawner` actions. It must return the selected UE spawner and stable id only.
- Boundary: `ActionResolution` must not create nodes, apply defaults, connect pins, or call promotion notifications.
- Boundary: `GraphStatementBuilder` / FragmentDAG creates the fragment from the resolved spawner.
- Boundary: `GraphComposer` / Linker connects data edges and invokes UE promotion lifecycle on changed `UK2Node_PromotableOperator` pins.
- Boundary: literal `value_type` and default values are not dragged-pin-equivalent type context. They may be recorded and applied after spawn, but they must not force concrete operator overload selection.
- Create: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Systems\ToolClusters\GraphWrite\ActionResolution\BlueprintHelperOperatorActionResolver.h`
- Create: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\ActionResolution\BlueprintHelperOperatorActionResolver.cpp`
- Modify: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\ActionResolution\BlueprintHelperFunctionActionCluster.cpp`
- Modify only if required by typed constraints: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Systems\ToolClusters\GraphWrite\ActionResolution\BlueprintHelperActionResolutionCore.h`
- Modify: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\GraphStatement\BlueprintHelperGraphStatementBuilder.cpp`
- Test: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Tests\GraphWrite\BlueprintHelperOperatorActionResolverTests.cpp`

### P3 GenericAssetStructControlActionCluster

- Create: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Systems\ToolClusters\GraphWrite\ActionResolution\BlueprintHelperGenericActionProviderBoundary.h`
- Create: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\ActionResolution\BlueprintHelperGenericActionProviderBoundary.cpp`
- Create: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Systems\ToolClusters\GraphWrite\ActionResolution\BlueprintHelperGenericAssetStructControlActionResolver.h`
- Create: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\ActionResolution\BlueprintHelperGenericAssetStructControlActionResolver.cpp`
- Modify: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\ActionResolution\BlueprintHelperGenericAssetStructControlActionCluster.cpp`
- Modify: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\GraphStatement\BlueprintHelperGraphStatementBuilder.cpp`
- Create if missing: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Systems\ToolClusters\GraphWrite\GraphStatement\BlueprintHelperSelectFragmentBuilder.h`
- Create if missing: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\GraphStatement\BlueprintHelperSelectFragmentBuilder.cpp`
- Create if missing: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Systems\ToolClusters\GraphWrite\GraphStatement\BlueprintHelperControlFragmentBuilder.h`
- Create if missing: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\GraphStatement\BlueprintHelperControlFragmentBuilder.cpp`
- Test: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Tests\GraphWrite\BlueprintHelperGenericAssetStructControlActionClusterTests.cpp`
- Modify after implementation: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\Plan\BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`

---

## P2: FunctionActionCluster Extends `op` with UE Promotable Operator Semantics

### Task 2.1: Add operator resolver contract based on UE type promotion

**Files:**
- Create: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Systems\ToolClusters\GraphWrite\ActionResolution\BlueprintHelperOperatorActionResolver.h`
- Create: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\ActionResolution\BlueprintHelperOperatorActionResolver.cpp`

- [ ] **Step 1: Create a resolver header with a narrow API**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class UBlueprintFunctionNodeSpawner;

class BLUEPRINTHELPER_API FBlueprintHelperOperatorActionResolver
{
public:
	static FBlueprintHelperActionResolutionResult Resolve(const FBlueprintHelperActionResolutionRequest& Request);

private:
	static bool TryMapOperatorTokenToPromotionName(const FString& Token, FName& OutOpName);
	static UBlueprintFunctionNodeSpawner* FindPromotableOperatorSpawner(FName OpName);
	static FBlueprintHelperActionResolutionResult MakePromotableOperatorResult(
		const FBlueprintHelperActionResolutionRequest& Request,
		FName OpName,
		UBlueprintFunctionNodeSpawner* Spawner);
	static FBlueprintHelperActionResolutionResult MakeInvalidRequestResult(const FString& Message);
	static FBlueprintHelperActionResolutionResult MakeNotFoundResult(const FBlueprintHelperActionResolutionRequest& Request, const FString& Message);
};
```

Expected: API exposes operator semantic resolution only. It does not expose `Call`-specific request conversion, alias expansion, or concrete overload lookup as the primary behavior.

- [ ] **Step 2: Implement semantic validation and token mapping**

```cpp
FBlueprintHelperActionResolutionResult FBlueprintHelperOperatorActionResolver::Resolve(const FBlueprintHelperActionResolutionRequest& Request)
{
	if (Request.Semantic.Kind != EBlueprintHelperActionSemanticKind::Op)
	{
		FBlueprintHelperActionResolutionResult Result;
		Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
		Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
		Result.ErrorCode = TEXT("operator_resolver_requires_op_semantic");
		Result.Message = TEXT("Operator resolver only accepts Semantic.Kind=Op.");
		return Result;
	}

	FName OpName = NAME_None;
	if (!TryMapOperatorTokenToPromotionName(Request.Semantic.Query, OpName))
	{
		return MakeInvalidRequestResult(FString::Printf(
			TEXT("Unsupported operator token '%s'. Supported tokens map to UE type promotion operator names such as Add, Subtract, Greater, Less, EqualEqual, and NotEqual."),
			*Request.Semantic.Query));
	}

	UBlueprintFunctionNodeSpawner* Spawner = FindPromotableOperatorSpawner(OpName);
	if (!Spawner)
	{
		return MakeNotFoundResult(Request, FString::Printf(
			TEXT("UE promotable operator spawner was not available for '%s'. Refresh BlueprintActionDatabase before treating this as unsupported."),
			*OpName.ToString()));
	}

	return MakePromotableOperatorResult(Request, OpName, Spawner);
}
```

Expected: invalid semantic kind fails explicitly; valid tokens resolve to UE promotion operator names instead of concrete function names.

### Task 2.2: Use `FTypePromotion::GetOperatorSpawner` as the primary operator provider

**Files:**
- Modify: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\ActionResolution\BlueprintHelperOperatorActionResolver.cpp`

- [ ] **Step 1: Include UE promotion headers**

```cpp
#include "BlueprintTypePromotion.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "K2Node_PromotableOperator.h"
```

Expected: P2 depends on UE 5.6's native type-promotion provider instead of duplicating operator overload selection.

- [ ] **Step 2: Map compact AgentFace operator tokens to UE promotion operator names**

```cpp
bool FBlueprintHelperOperatorActionResolver::TryMapOperatorTokenToPromotionName(const FString& Token, FName& OutOpName)
{
	const FString Normalized = Token.TrimStartAndEnd().ToLower();
	if (Normalized == TEXT("+") || Normalized == TEXT("add"))
	{
		OutOpName = TEXT("Add");
		return true;
	}
	if (Normalized == TEXT("-") || Normalized == TEXT("subtract"))
	{
		OutOpName = TEXT("Subtract");
		return true;
	}
	if (Normalized == TEXT("*") || Normalized == TEXT("multiply"))
	{
		OutOpName = TEXT("Multiply");
		return true;
	}
	if (Normalized == TEXT("/") || Normalized == TEXT("divide"))
	{
		OutOpName = TEXT("Divide");
		return true;
	}
	if (Normalized == TEXT(">") || Normalized == TEXT("greater"))
	{
		OutOpName = TEXT("Greater");
		return true;
	}
	if (Normalized == TEXT(">=") || Normalized == TEXT("greater_equal") || Normalized == TEXT("greater_or_equal"))
	{
		OutOpName = TEXT("GreaterEqual");
		return true;
	}
	if (Normalized == TEXT("<") || Normalized == TEXT("less"))
	{
		OutOpName = TEXT("Less");
		return true;
	}
	if (Normalized == TEXT("<=") || Normalized == TEXT("less_equal") || Normalized == TEXT("less_or_equal"))
	{
		OutOpName = TEXT("LessEqual");
		return true;
	}
	if (Normalized == TEXT("==") || Normalized == TEXT("equal") || Normalized == TEXT("equals"))
	{
		OutOpName = TEXT("EqualEqual");
		return true;
	}
	if (Normalized == TEXT("!=") || Normalized == TEXT("not_equal") || Normalized == TEXT("not_equals"))
	{
		OutOpName = TEXT("NotEqual");
		return true;
	}
	return false;
}
```

Expected: compact TaskSpec fields remain stable, but mapping stops before concrete typed function overload selection.

- [ ] **Step 3: Fetch the promotable operator spawner**

```cpp
UBlueprintFunctionNodeSpawner* FBlueprintHelperOperatorActionResolver::FindPromotableOperatorSpawner(FName OpName)
{
	if (OpName.IsNone())
	{
		return nullptr;
	}
	return FTypePromotion::GetOperatorSpawner(OpName);
}
```

Expected: the resolver asks UE for the same operator spawner used by the Blueprint editor, rather than using function-name aliases.

- [ ] **Step 4: Return a structured ActionResolution result that carries the spawner**

```cpp
FBlueprintHelperActionResolutionResult FBlueprintHelperOperatorActionResolver::MakePromotableOperatorResult(
	const FBlueprintHelperActionResolutionRequest& Request,
	FName OpName,
	UBlueprintFunctionNodeSpawner* Spawner)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
	Result.Semantic = Request.Semantic;
	Result.NodeSpawner = Spawner;
	Result.StableId = FString::Printf(TEXT("promotable_operator:%s"), *OpName.ToString());
	Result.DisplayName = OpName.ToString();
	Result.DebugSummary = FString::Printf(TEXT("Resolved UE promotable operator '%s'."), *OpName.ToString());
	return Result;
}
```

Expected: the downstream GraphStatement/Fragment path receives a real UE node spawner and can invoke it without direct `UK2Node` construction shortcuts.

### Task 2.3: Make GraphStatement/FragmentDAG treat `op` as a promotable node plus data edges

**Files:**
- Modify: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\GraphStatement\BlueprintHelperGraphStatementBuilder.cpp`
- Modify: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\GraphStatement\Utils\BlueprintHelperGraphComposerUtils.cpp`

- [ ] **Step 1: Stop using literal-only type hints to force concrete operator overloads**

```cpp
// Required behavior:
// - Literal defaults may populate input pin default values after the node exists.
// - Literal value_type may be kept as debug/preview evidence.
// - Literal value_type must not force FunctionActionCluster to choose Greater_IntInt, Greater_DoubleDouble, etc.
// - Real data edges from typed producer pins are the primary promotion context.
```

Expected: `op >` with two literal values no longer fails because the resolver guessed a wrong concrete overload such as Timespan or Double.

- [ ] **Step 2: Invoke the resolved `UBlueprintFunctionNodeSpawner` for `op` fragments**

```cpp
// Required behavior:
// UBlueprintFunctionNodeSpawner* Spawner = ActionResult.NodeSpawner;
// UK2Node* SpawnedNode = Cast<UK2Node>(Spawner->Invoke(TargetGraph, Bindings, SpawnLocation));
// The spawned node should be UK2Node_PromotableOperator for promotable operators.
```

Expected: node creation still flows through SemanticIR -> FragmentDAG -> NodeFragment builder/mutator, but the actual UE node is created by UE's spawner.

- [ ] **Step 3: Let linker/schema drive type promotion after data edges are connected**

```cpp
// Required behavior after a data edge is linked:
// const bool bConnected = Schema->TryCreateConnection(SourcePin, TargetPin);
// if (UK2Node_PromotableOperator* OpNode = Cast<UK2Node_PromotableOperator>(TargetPin->GetOwningNode()))
// {
//     OpNode->NotifyPinConnectionListChanged(TargetPin);
// }
```

Expected: when TaskSpec has a typed `get`, `get_property`, `construct`, or function result linked into `op`, UE performs the same promotion pass as the Blueprint editor.

- [ ] **Step 3.1: Notify promotable operator nodes from the shared graph composer utility**

```cpp
// Required behavior:
// Every successful schema data connection path in FBlueprintHelperGraphComposerUtils::TryCreateSchemaDataConnection()
// must call a shared helper that checks both pins and notifies any UK2Node_PromotableOperator owner.
```

Expected: typed promotion works through the common data edge path rather than an `op`-specific one-off branch.

- [ ] **Step 4: Keep concrete function resolution only as an explicit unsupported/promotable-unavailable diagnostic path**

```cpp
// Required behavior:
// If FTypePromotion::GetOperatorSpawner(OpName) is unavailable, return NotFound or NeedsAction.
// Do not silently fall back to old node handlers.
// Do not silently search arbitrary call functions unless a later design explicitly introduces a concrete_function_operator mode.
```

Expected: no legacy fallback and no hidden behavior drift from the UE editor model.

### Task 2.4: Wire `Op` into `FunctionActionCluster`

**Files:**
- Modify: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\ActionResolution\BlueprintHelperFunctionActionCluster.cpp`

- [ ] **Step 1: Include the operator resolver**

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOperatorActionResolver.h"
```

- [ ] **Step 2: Replace `Op` migration pending with resolver dispatch**

```cpp
if (Semantic.Kind == EBlueprintHelperActionSemanticKind::Op)
{
	return FBlueprintHelperOperatorActionResolver::Resolve(Request);
}
```

Expected: `operator_action_cluster_migration_pending` is removed from the active P2 implementation path.

### Task 2.5: Add operator tests and hygiene checks

**Files:**
- Create: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Tests\GraphWrite\BlueprintHelperOperatorActionResolverTests.cpp`

- [ ] **Step 1: Add a contract test for promotable operator dispatch**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperOperatorActionDispatchTest,
	"BlueprintHelper.GraphWrite.ActionResolution.FunctionAction.OperatorDispatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperOperatorActionDispatchTest::RunTest(const FString& Parameters)
{
	FBlueprintActionDatabase::Get().RefreshAll();

	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Op;
	Request.Semantic.Query = TEXT(">");

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestNotEqual(TEXT("Op is no longer UnsupportedClusterMigration"), Result.Status, EBlueprintHelperActionResolutionStatus::UnsupportedClusterMigration);
	TestEqual(TEXT("Op resolves as FunctionAction cluster"), Result.ClusterKind, EBlueprintHelperSpawnerClusterKind::FunctionAction);
	TestTrue(TEXT("Promotable operator result has a stable id"), Result.StableId.StartsWith(TEXT("promotable_operator:")));
	return true;
}
```

Expected: `op >` resolves to the UE promotable operator provider when type promotion is available.

- [ ] **Step 2: Add a GraphStatement smoke test for template operator creation**

```cpp
// Build a TaskSpec expression equivalent to:
// op ">" with no concrete typed input edge at the resolver phase.
// Expected preview: resolved operator fragment uses StableId "promotable_operator:Greater".
// Expected execute: spawned node class is UK2Node_PromotableOperator.
```

Expected: no direct `Greater_IntInt`, `Greater_FloatFloat`, `Greater_DoubleDouble`, or KismetMathLibrary overload is required for template spawn.

- [ ] **Step 3: Add a GraphStatement smoke test for typed edge promotion**

```cpp
// Build a TaskSpec where a typed variable/property/function output links into op input A/B.
// Expected execute: Schema connection succeeds.
// Expected execute: UK2Node_PromotableOperator::NotifyPinConnectionListChanged is called for changed operator pins.
// Expected compile: no Timespan/default wildcard promotion failure.
```

Expected: typed data edges, not alias expansion, drive operator specialization.

- [ ] **Step 4: Add source hygiene for removed migration marker and forbidden concrete alias strategy**

```cpp
const TArray<FString> ForbiddenTokens = {
	TEXT("operator_action_cluster_migration_pending"),
	TEXT("FunctionActionCluster owns semantic kind"),
	TEXT("Greater_DoubleDouble"),
	TEXT("Greater_IntInt"),
	TEXT("Greater_FloatFloat")
};
```

Expected: source hygiene blocks both the old P2 placeholder and the incorrect "solve operator by growing concrete alias list" strategy.

---

## P3: GenericAssetStructControlActionCluster

### Task 3.1: Define provider capability boundary before implementation

**Files:**
- Create: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Systems\ToolClusters\GraphWrite\ActionResolution\BlueprintHelperGenericActionProviderBoundary.h`
- Create: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\ActionResolution\BlueprintHelperGenericActionProviderBoundary.cpp`

- [ ] **Step 1: Create boundary enum and result type**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

enum class EBlueprintHelperGenericActionProviderMode : uint8
{
	NodeSpawnerCandidate,
	DedicatedFragmentBuilderRequired,
	NeedsMoreSemanticContext,
	Unsupported
};

struct FBlueprintHelperGenericActionProviderBoundary
{
	EBlueprintHelperGenericActionProviderMode Mode = EBlueprintHelperGenericActionProviderMode::Unsupported;
	FString Reason;
	FString RequiredBuilder;
};

class BLUEPRINTHELPER_API FBlueprintHelperGenericActionProviderBoundaryService
{
public:
	static FBlueprintHelperGenericActionProviderBoundary Classify(const FBlueprintHelperActionResolutionRequest& Request);
};
```

Expected: boundary decision is a reusable service, not a cluster-local pile of branches.

- [ ] **Step 2: Implement the first boundary matrix**

```cpp
FBlueprintHelperGenericActionProviderBoundary FBlueprintHelperGenericActionProviderBoundaryService::Classify(const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperGenericActionProviderBoundary Boundary;
	switch (Request.Semantic.Kind)
	{
	case EBlueprintHelperActionSemanticKind::Construct:
	case EBlueprintHelperActionSemanticKind::Deconstruct:
		Boundary.Mode = Request.Semantic.TypeName.IsEmpty()
			? EBlueprintHelperGenericActionProviderMode::NeedsMoreSemanticContext
			: EBlueprintHelperGenericActionProviderMode::NodeSpawnerCandidate;
		Boundary.RequiredBuilder = Request.Semantic.Kind == EBlueprintHelperActionSemanticKind::Construct
			? TEXT("ConstructFragmentBuilder")
			: TEXT("DeconstructFragmentBuilder");
		Boundary.Reason = Request.Semantic.TypeName.IsEmpty()
			? TEXT("construct/deconstruct requires TypeName before resolving struct or generic action candidates")
			: TEXT("construct/deconstruct can query struct or generic NodeSpawner candidates by TypeName");
		return Boundary;
	case EBlueprintHelperActionSemanticKind::Select:
		Boundary.Mode = EBlueprintHelperGenericActionProviderMode::DedicatedFragmentBuilderRequired;
		Boundary.Reason = TEXT("select requires wildcard option pin materialization and type propagation that ActionDatabase does not fully express");
		Boundary.RequiredBuilder = TEXT("SelectFragmentBuilder");
		return Boundary;
	case EBlueprintHelperActionSemanticKind::Control:
		Boundary.Mode = EBlueprintHelperGenericActionProviderMode::DedicatedFragmentBuilderRequired;
		Boundary.Reason = TEXT("control statements compose execution pins and nested statement DAGs, not a single menu action");
		Boundary.RequiredBuilder = TEXT("ControlFragmentBuilder");
		return Boundary;
	default:
		Boundary.Mode = EBlueprintHelperGenericActionProviderMode::Unsupported;
		Boundary.Reason = TEXT("semantic kind is not owned by GenericAssetStructControlActionCluster");
		return Boundary;
	}
}
```

Expected: provider boundary is explicit and testable before specialized builders are added.

### Task 3.2: Add generic resolver for NodeSpawner-expressible cases

**Files:**
- Create: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Systems\ToolClusters\GraphWrite\ActionResolution\BlueprintHelperGenericAssetStructControlActionResolver.h`
- Create: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\ActionResolution\BlueprintHelperGenericAssetStructControlActionResolver.cpp`

- [ ] **Step 1: Add resolver API**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class BLUEPRINTHELPER_API FBlueprintHelperGenericAssetStructControlActionResolver
{
public:
	static FBlueprintHelperActionResolutionResult ResolveNodeSpawnerCandidate(const FBlueprintHelperActionResolutionRequest& Request);

private:
	static FBlueprintHelperActionResolutionResult MakeNeedsContextResult(const FBlueprintHelperActionResolutionRequest& Request, const FString& Message);
	static FBlueprintHelperActionResolutionResult MakeNotFoundResult(const FBlueprintHelperActionResolutionRequest& Request, const FString& Message);
};
```

Expected: resolver only handles NodeSpawner-expressible generic cases; it does not construct select/control nodes manually.

- [ ] **Step 2: Resolve construct/deconstruct by semantic type**

```cpp
FBlueprintHelperActionResolutionResult FBlueprintHelperGenericAssetStructControlActionResolver::ResolveNodeSpawnerCandidate(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	if (Request.Semantic.TypeName.TrimStartAndEnd().IsEmpty())
	{
		return MakeNeedsContextResult(Request, TEXT("TypeName is required to resolve construct/deconstruct candidates."));
	}

	FBlueprintHelperActionResolutionResult Result;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
	Result.ErrorCode = TEXT("generic_action_not_found");
	Result.Message = FString::Printf(
		TEXT("No generic action candidate found for semantic '%s' and type '%s'."),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
		*Request.Semantic.TypeName);
	return Result;
}
```

Expected: execution replaces the database traversal body with real ActionDatabase search while keeping explicit states.

### Task 3.3: Wire generic cluster through boundary service

**Files:**
- Modify: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\ActionResolution\BlueprintHelperGenericAssetStructControlActionCluster.cpp`

- [ ] **Step 1: Include boundary and resolver**

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericActionProviderBoundary.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionResolver.h"
```

- [ ] **Step 2: Replace blanket migration pending with boundary-driven dispatch**

```cpp
const FBlueprintHelperGenericActionProviderBoundary Boundary = FBlueprintHelperGenericActionProviderBoundaryService::Classify(Request);
if (Boundary.Mode == EBlueprintHelperGenericActionProviderMode::NodeSpawnerCandidate)
{
	return FBlueprintHelperGenericAssetStructControlActionResolver::ResolveNodeSpawnerCandidate(Request);
}
```

Expected: P3 stops using one blanket migration-pending result and starts producing actionable provider-boundary diagnostics.

### Task 3.4: Add dedicated FragmentBuilder seams for select/control

**Files:**
- Modify: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\GraphStatement\BlueprintHelperGraphStatementBuilder.cpp`
- Create if missing: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Systems\ToolClusters\GraphWrite\GraphStatement\BlueprintHelperSelectFragmentBuilder.h`
- Create if missing: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\GraphStatement\BlueprintHelperSelectFragmentBuilder.cpp`
- Create if missing: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Systems\ToolClusters\GraphWrite\GraphStatement\BlueprintHelperControlFragmentBuilder.h`
- Create if missing: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite\GraphStatement\BlueprintHelperControlFragmentBuilder.cpp`

- [ ] **Step 1: Add select builder seam**

```cpp
class BLUEPRINTHELPER_API FBlueprintHelperSelectFragmentBuilder
{
public:
	static FBlueprintHelperGraphNodeFragment Build(
		const FBlueprintHelperGraphExpressionIR& Expression,
		const FBlueprintHelperActionResolutionRequest& Request,
		TArray<FBlueprintGeneratorDiagnostic>& OutDiagnostics);
};
```

Expected: select-specific wildcard pin handling is isolated in one builder, not embedded in `GenericAssetStructControlActionCluster`.

- [ ] **Step 2: Add control builder seam**

```cpp
class BLUEPRINTHELPER_API FBlueprintHelperControlFragmentBuilder
{
public:
	static FBlueprintHelperGraphNodeFragment BuildBranch(
		const FBlueprintHelperGraphStatementIR& Statement,
		const FBlueprintHelperActionResolutionRequest& Request,
		TArray<FBlueprintGeneratorDiagnostic>& OutDiagnostics);
};
```

Expected: control-flow DAG composition stays in GraphStatement/FragmentDAG layer, while ActionResolution only selects action/provider capability.

### Task 3.5: Add Generic cluster tests

**Files:**
- Create: `D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Tests\GraphWrite\BlueprintHelperGenericAssetStructControlActionClusterTests.cpp`

- [ ] **Step 1: Test provider boundary matrix**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericActionProviderBoundaryTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.ProviderBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericActionProviderBoundaryTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionResolutionRequest SelectRequest;
	SelectRequest.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	SelectRequest.Semantic.Kind = EBlueprintHelperActionSemanticKind::Select;
	const FBlueprintHelperGenericActionProviderBoundary SelectBoundary = FBlueprintHelperGenericActionProviderBoundaryService::Classify(SelectRequest);
	TestEqual(TEXT("Select requires dedicated builder"), SelectBoundary.Mode, EBlueprintHelperGenericActionProviderMode::DedicatedFragmentBuilderRequired);

	FBlueprintHelperActionResolutionRequest ConstructRequest;
	ConstructRequest.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	ConstructRequest.Semantic.Kind = EBlueprintHelperActionSemanticKind::Construct;
	ConstructRequest.Semantic.TypeName = TEXT("Vector");
	const FBlueprintHelperGenericActionProviderBoundary ConstructBoundary = FBlueprintHelperGenericActionProviderBoundaryService::Classify(ConstructRequest);
	TestEqual(TEXT("Construct with type can query NodeSpawner"), ConstructBoundary.Mode, EBlueprintHelperGenericActionProviderMode::NodeSpawnerCandidate);
	return true;
}
```

Expected: tests lock the provider boundary before builder implementation details change.

---

## Validation Plan

### Source hygiene

Run:

```powershell
rg "operator_action_cluster_migration_pending|generic_asset_struct_control_action_cluster_migration_pending|SelectCluster\(Intent|Request\.Intent|ActionRequest\.Intent|SpawnVariableGetNode|SpawnVariableSetNode|Greater_DoubleDouble|Greater_IntInt|Greater_FloatFloat" D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper
```

Expected: no matches.

### Build

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReload
```

Expected: build succeeds on UE 5.6.

### CLI runtime smoke

1. Start editor via global MCP lifecycle tool.
2. Run operator preview smoke with `op >` and `op ==` where no concrete typed data edge exists at resolver time.
3. Confirm preview resolves to `promotable_operator:Greater` / `promotable_operator:EqualEqual` and does not require a concrete `KismetMathLibrary` overload.
4. Run operator execute smoke where typed producer pins are linked into `op` inputs.
5. Confirm execute spawns `UK2Node_PromotableOperator`, links pins through `UEdGraphSchema_K2::TryCreateConnection`, and calls `NotifyPinConnectionListChanged` on changed operator pins.
6. Confirm Blueprint compile does not fail with wildcard promotion errors such as Timespan/default operator mismatch.
7. Run generic preview smoke for `construct`, `deconstruct`, `select`, and `control`.
8. Confirm generic preview returns one of `Resolved`, `Ambiguous`, `InvalidRequest`, or `Blocked` with `dedicated_fragment_builder_required`.
9. Confirm preview never returns old migration-pending markers.
10. Close editor via global MCP lifecycle tool.
11. Build once after code changes.

---

## Completion Criteria

- [x] `op` is handled by `FunctionActionCluster` through UE promotable operator semantics and no longer returns `UnsupportedClusterMigration`.
- [x] Operator resolution uses `FTypePromotion::GetOperatorSpawner(OpName)` as the primary provider.
- [x] Operator preview returns structured resolved/not_found diagnostics with `promotable_operator:<OpName>` stable ids.
- [x] Operator execute spawns `UK2Node_PromotableOperator` through UE's spawner instead of directly constructing `UK2Node` or resolving concrete KismetMathLibrary overloads.
- [x] Typed data edges drive operator promotion through Schema linking and `NotifyPinConnectionListChanged`.
- [ ] Generic cluster has a tested provider boundary for `construct/deconstruct/select/control`.
- [ ] `construct/deconstruct` attempt NodeSpawner/ActionDatabase resolution when enough type context exists, and only use dedicated builder when UE provider cannot express the semantic.
- [x] `select/control` have dedicated FragmentBuilder seams when ActionDatabase cannot express the semantic.
- [ ] `call` spawn layer is unified onto the same `SelectedSpawner->Invoke()` adapter used by other NodeSpawner-expressible actions.
- [ ] `get/set` spawn layer has no direct variable-node shortcut and only consumes `FieldVariableActionCluster` selected spawners.
- [ ] shared spawner adapter / composer lifecycle owns post-spawn defaults, pin normalization, and post-link promotion notifications instead of each semantic kind owning one-off handling.
- [x] No old fallback, old AgentFace alias, direct `UK2Node` shortcut, concrete operator alias-growth strategy, or blanket migration marker remains.
- [x] UE 5.6 build succeeds.
- [x] Progress/design documents honestly record any remaining runtime gap.

---

## Execution Update 2026-05-21

- [x] P2 boundary corrected: `ActionResolution` now resolves `op` to the UE promotable operator spawner only; node creation and pin/link promotion remain in GraphStatement/GraphComposer boundaries.
- [x] Removed the incorrect concrete operator alias-growth path (`Greater_IntInt`, `Greater_FloatFloat`, `Greater_DoubleDouble` and equivalent resolver flow).
- [x] `GraphStatementBuilder` now spawns resolved operator actions through the generic spawner path instead of rehydrating `FBlueprintHelperCallFunctionCandidate`.
- [x] `GraphComposerUtils` now notifies `UK2Node_PromotableOperator::NotifyPinConnectionListChanged` from the shared data-connection path after successful schema connections.
- [x] Literal-only operator expressions now use literal `value_type` to convert the promotable operator pin before applying default values. This handles TaskSpec values such as `float 2 > float 1` without reverting to concrete function aliases.
- [x] Source hygiene check passed:

```powershell
rg "operator_action_cluster_migration_pending|generic_asset_struct_control_action_cluster_migration_pending|SelectCluster\(Intent|Request\.Intent|ActionRequest\.Intent|SpawnVariableGetNode|SpawnVariableSetNode|Greater_DoubleDouble|Greater_IntInt|Greater_FloatFloat" BlueprintHelper\Source\BlueprintHelper
```

- [x] UE 5.6 compile passed:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReload
```

- [x] CLI preview smoke passed for `op >`:

```powershell
bh.cmd task preview --file D:\UEProjects\Template\Saved\BlueprintHelper\CodexSmoke\ActionResolutionP2P3\op_set_preview_20260521_191747.json --select status,summary,error_code,message,artifacts.full_result
```

- [x] CLI execute smoke passed for literal-only `op >`:

```powershell
bh.cmd task execute --file D:\UEProjects\Template\Saved\BlueprintHelper\CodexSmoke\ActionResolutionP2P3\op_set_execute_20260521_192125.json --select status,summary,error_code,message,artifacts.full_result
```

- [x] P3 `select` boundary remains explicit and expected: preview blocks with `Required builder: SelectFragmentBuilder`.

距离期望差距：
- [x] `construct/deconstruct` 的真实 preview/execute smoke 已覆盖：`construct Vector` 写入 `SetActorLocation`，`deconstruct Vector.X` 写入 `SmokeFloat`。
- [o] `select/control` 专用 builder seam 已有边界诊断；`select` 已进入真实 builder 实现，`control` 仍需要单独阶段推进。
- [ ] `call` 仍存在专用 spawn 分支，尚未完全统一到 `SelectedSpawner->Invoke()`。
- [ ] `get/set` 仍需完成源码审计和运行验证，确认没有变量节点直接创建 shortcut。
- [ ] `op` 的 literal 默认值、pin 类型提示和 promotion 通知需要继续收敛到共享 spawner adapter / composer lifecycle，而不是停留在 `op` 专用处理。

## Execution Update 2026-05-21 P3 Continued

- [x] 新增 `FBlueprintHelperSelectFragmentBuilder`，`select` 表达式不再通过 GenericActionCluster 的 dedicated-builder 阻塞结果进入执行路径。
- [x] `select` builder 使用 UE `UK2Node_Select` 公开 API：`AddInputPin()`、`ChangePinType()`、`GetOptionPins()`、`GetIndexPin()`、`GetReturnValuePin()`。
- [x] `select then/else` 的 DAG 映射修正为 `else -> Option 0(false)`、`then -> Option 1(true)`，避免 parser 同时写入 `Args` 与 `Options` 后被重复连接到错误 option pin。
- [x] `construct/deconstruct` 表达式进入现有 `FBlueprintHelperStructConstructionResolver` 主路径，按 `TypeName` 解析 `UScriptStruct` 并生成 `MakeStruct/BreakStruct` fragment。
- [x] `GraphComposerUtils` 的 post-link lifecycle 通知从 `UK2Node_PromotableOperator` 专用扩展为 operator/select 共用连接生命周期通知。
- [x] `deconstruct Vector` 修正为 native break/search break function 优先，避免 UE 编译器拒绝泛型 `Break Vector`。
- [x] CLI smoke 通过：`select` preview/execute 已通过；`construct Vector` + `deconstruct Vector.X` preview/execute 已通过。

距离期望差距：
- [ ] `control` 专用 FragmentBuilder 尚未实现。
- [ ] `construct/deconstruct` 当前已能运行，但仍使用现有 struct resolver 直接创建或解析 struct 节点；后续还需继续评估哪些 struct action 可由 UE NodeSpawner/ActionDatabase 表达，并把可表达部分收敛到统一 `SelectedSpawner->Invoke()` adapter。
- [ ] `call/get/set` spawn layer 收敛、shared spawner adapter、post-spawn defaults/pin normalization 仍未完成。

---
## Execution Notes

- Use UTF-8 no BOM for generated TaskSpec files:

```powershell
[System.IO.File]::WriteAllText($Path, $Json, (New-Object System.Text.UTF8Encoding($false)))
```

- Do not use `Set-Content -Encoding UTF8` for CLI TaskSpec smoke files in Windows PowerShell because it can write a BOM and break strict JSON parsing.
- Do not run `git add`, `git commit`, or `git push` automatically. At the end, output suggested commit message and manual commands only.

## Execution Update 2026-05-21 ActionResolutionCore Full-Line Convergence

- [x] `FunctionActionCluster` 在 resolved function 缺少 `NodeSpawner` 时补齐 `UBlueprintFunctionNodeSpawner::Create(Function)`，保证 `call` 结果可由统一 adapter Invoke。
- [x] `GenericAssetStructControlActionResolver` 移除 `construct/deconstruct` NotFound 占位逻辑，改为解析 `Semantic.TypeName` 并返回可 Invoke 的 `SelectedSpawner`。
- [x] `GraphStatementBuilder` 的 `call/get/set/construct/deconstruct` 均改为消费 `ActionResolutionResult.SelectedSpawner`。
- [x] 新增 `FBlueprintHelperActionNodeSpawnerAdapter` 作为统一 `SelectedSpawner->Invoke()` 入口。
- [x] 移除旧 `FBlueprintHelperStructConstructionResolver` 文件和 `FBlueprintHelperCallFunctionResolver::SpawnResolvedNode` API。

距离期望差距：
- [ ] `control` 专用 FragmentBuilder 尚未实现，本轮未覆盖。
- [x] `construct/deconstruct` 已收敛到 `ActionResolutionCore -> GenericAssetStructControlActionCluster -> SelectedSpawner->Invoke()`；native/search make/break function 通过 FunctionActionCluster 取得 spawner，direct MakeStruct/BreakStruct 通过 `UBlueprintFieldNodeSpawner` 设置 StructType。
- [x] `call/get/set` 主生成层已收敛为消费 `ActionResolutionResult.SelectedSpawner`，不再调用 `CallFunctionResolver::SpawnResolvedNode` 或变量节点直接创建 shortcut。
- [o] shared spawner adapter 已抽出 `FBlueprintHelperActionNodeSpawnerAdapter` 作为统一 Invoke 入口；post-spawn defaults、pin normalization、post-link lifecycle 还未完全迁移到该 adapter。
### Validation Update 2026-05-21

- [x] Source hygiene passed: no source matches for `BlueprintHelperStructConstructionResolver`, `SpawnResolvedNode(`, `generic_action_node_spawner_candidate_not_found`, `SpawnVariableGetNode`, or `SpawnVariableSetNode`.
- [x] UE 5.6 build passed:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReload
```

距离期望差距：
- [ ] Editor-side CLI smoke for the new full-line convergence was not run in this pass.
### CLI Runtime Smoke Update 2026-05-21

- [x] Global MCP `blueprint_open_editor` succeeded and Bridge became available.
- [x] Created fresh smoke Blueprint asset: `/Game/BlueprintHelperCliSmoke/ActionResolutionFullLine_20260521_210521/BP_AR_FullLine_20260521_210521`.
- [x] Variable task executed with 0 errors: `SmokeFloat` and `bResult`.
- [x] Graph preview passed with 0 errors for `call/get/set/op/construct/deconstruct` combined TaskSpec.
- [x] Graph execute passed with 0 errors for the same combined TaskSpec.
- [x] Global MCP `blueprint_close_editor` succeeded with `save_all=true`.

距离期望差距：
- [ ] `control` 专用 FragmentBuilder 仍未实现，不属于本轮 `construct/deconstruct/call/get/set` 收敛范围。
- [o] `FBlueprintHelperActionNodeSpawnerAdapter` 已成为统一 Invoke 入口；默认值、pin normalization、post-link lifecycle 的完全 adapter 化仍是后续架构清理项。
### CLI Runtime Smoke Update 2026-05-21 Control Branch Compiler Sync

- [x] AgentFace TypeScript compiler now allows `branch` and routes it through the existing branch statement flow instead of rejecting it at the statement whitelist.
- [x] AgentFace Python compiler now allows `branch` and routes it through the existing branch statement flow instead of rejecting it at the statement whitelist.
- [x] Task contract statement kind list now includes `branch`.
- [x] CLI preview passed for the control/branch smoke TaskSpec:

```powershell
bh.cmd task preview --file D:\UEProjects\Template\Saved\BlueprintHelper\CodexSmoke\ActionResolutionControl_20260521_214755\graph_20260521_214755.json --select status,summary,error_code,message,artifacts.full_result
```

- [x] CLI execute passed for the same control/branch smoke TaskSpec:

```powershell
bh.cmd task execute --file D:\UEProjects\Template\Saved\BlueprintHelper\CodexSmoke\ActionResolutionControl_20260521_214755\graph_20260521_214755.json --select status,summary,error_code,message,artifacts.full_result
```

距离期望差距：
- [o] `branch` 已完成 AgentFace compiler -> C++ SemanticIR -> control FragmentBuilder -> CLI preview/execute 闭环。
- [ ] `return` builder 已存在于 C++ control builder，但本轮没有单独 TaskSpec smoke 覆盖。
- [o] shared spawner adapter 已作为统一 Invoke 入口；post-spawn defaults、pin normalization、post-link lifecycle 的完全 adapter 化仍是后续架构清理项。
### Build Validation Update 2026-05-21 Control Branch Compiler Sync

- [x] AgentFace task-core build passed:

```powershell
npm.cmd run build
```

- [x] UE 5.6 project build passed:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReload
```
### Implementation Update 2026-05-21 Shared Adapter / Composer Lifecycle

- [x] 新增 `FBlueprintHelperGraphNodeLifecycle`，把 data edge link 后的节点生命周期通知从 `GraphComposerUtils` 局部函数收敛为共享 lifecycle 边界。
- [x] `GraphComposerUtils` 的 schema connection / promoted connection / conversion node connection / force-compatible connection 均统一调用 `FBlueprintHelperGraphNodeLifecycle::NotifyDataConnectionChanged`。
- [x] `FBlueprintHelperActionNodeSpawnerAdapter` 新增 direct `UBlueprintNodeSpawner` overload，使 dedicated FragmentBuilder 也可以复用同一套 spawn 后生命周期。
- [x] `FBlueprintHelperActionNodeSpawnerAdapter` 新增可失败 `NodeConfigurationHook`，用于将 dedicated builder 的 pin normalization / pin count setup 收敛到 adapter 生命周期内。
- [x] `FBlueprintHelperSelectFragmentBuilder` 不再直接 `SpawnK2Node<UK2Node_Select>`，改为 `UBlueprintNodeSpawner::Create(UK2Node_Select::StaticClass()) -> ActionNodeSpawnerAdapter::InvokeNodeSpawner`。
- [x] `select` literal defaults 不再直接调用 `FBlueprintGraphWriteFacade::ApplyDefaultValues`，改为通过 adapter `DefaultValueProvider` 进入统一 default application。
- [x] 源码卫生检查通过：不存在 `NotifySchemaDataConnectionLifecycleChanged`、`NotifyPinConnectionLifecycleChanged`、`FBlueprintGraphWriteFacade::ApplyDefaultValues(SelectNode)`、`BlueprintHelperGraphNodeFactory::SpawnK2Node<UK2Node_Select>`、`ApplyLiteralDefaults(` 残留。
- [x] UE 5.6 build passed after lifecycle convergence.
- [x] CLI control/branch preview and execute passed after lifecycle convergence.
- [x] CLI full-line `call/get/set/op/construct/deconstruct/select` preview and execute passed after lifecycle convergence.

距离期望差距：
- [x] post-spawn defaults 已收敛到 shared adapter 路径，`select` dedicated builder 已迁入。
- [x] pin normalization 的可失败配置入口已收敛到 shared adapter，`select` dedicated builder 已迁入。
- [x] post-link lifecycle 已收敛到 shared composer lifecycle helper。
- [o] 当前 lifecycle helper 明确覆盖 `UK2Node_PromotableOperator` 与 `UK2Node_Select` 的 link 后通知；如果后续新增其它需要特殊 link lifecycle 的 UE 节点，应扩展 `FBlueprintHelperGraphNodeLifecycle`，不要回到 builder/composer 局部分支。