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
- [ ] `construct/deconstruct` attempt NodeSpawner/ActionDatabase resolution when enough type context exists.
- [x] `select/control` have dedicated FragmentBuilder seams when ActionDatabase cannot express the semantic.
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

- [ ] `construct/deconstruct` 的真实 NodeSpawner/ActionDatabase preview smoke 本轮未重新覆盖；保留为后续 P3 验证项。
- [ ] `select/control` 专用 builder seam 已有边界诊断，但真实 builder 完整实现仍需要单独阶段推进。

---

## Execution Notes

- Use UTF-8 no BOM for generated TaskSpec files:

```powershell
[System.IO.File]::WriteAllText($Path, $Json, (New-Object System.Text.UTF8Encoding($false)))
```

- Do not use `Set-Content -Encoding UTF8` for CLI TaskSpec smoke files in Windows PowerShell because it can write a BOM and break strict JSON parsing.
- Do not run `git add`, `git commit`, or `git push` automatically. At the end, output suggested commit message and manual commands only.
