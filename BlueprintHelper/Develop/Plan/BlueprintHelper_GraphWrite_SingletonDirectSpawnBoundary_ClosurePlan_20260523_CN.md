# Singleton Direct Spawn Boundary Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close GraphWrite Gap 3 / Gap 4 by making canonical singleton direct spawn an explicit `GenericAssetStructControlActionCluster` internal secondary semantic mapping, and by preventing `MutationCoordinator` from owning singleton spawner strategy.

**Architecture:** `branch` / `sequence` / `return` / `select` remain valid canonical singleton semantics, but direct spawn is allowed only inside `FBlueprintHelperSingletonControlFlowEvidenceProvider` after first-level dispatch to `GenericAssetStructControlAction`. `MutationCoordinator` may request a mutation-produced singleton node by enum intent, but it must not build semantic query/request details or map UE node classes; provider owns the mapping, evidence, `SelectedSpawner`, stable id, and reason. Runtime mutation still uses the shared `FBlueprintHelperActionNodeSpawnerAdapter`.

**Tech Stack:** UE 5.6 C++, BlueprintHelper GraphWrite, `ActionResolutionCore`, `GenericAssetStructControlActionCluster`, `FBlueprintHelperSingletonControlFlowEvidenceProvider`, `FBlueprintHelperActionNodeSpawnerAdapter`, Automation Tests, BuildPlugin.

---

## Files

- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSingletonControlFlowEvidenceProvider.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSingletonControlFlowEvidenceProvider.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationCoordinator.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericAssetStructControlActionClusterTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteLegacyMainlineContractTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp`
- Modify: `BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md`

## Current Baseline

- `FBlueprintHelperSingletonControlFlowEvidenceProvider` already owns `UBlueprintNodeSpawner::Create(NodeClass)` for canonical singleton nodes.
- `FBlueprintHelperGenericAssetStructControlActionResolver::ResolveNodeSpawnerCandidate` already routes `Select` and `Control` through that provider.
- `BlueprintHelperGraphWriteMutationCoordinator.cpp` no longer calls `UBlueprintNodeSpawner::Create` directly, but it still constructs a full `FBlueprintHelperActionResolutionRequest` with `Semantic.Kind = Control` and `Semantic.Query = "sequence"` inside mutation code.
- Gap 3 remains until the provider exposes an explicit canonical secondary mapping API that can be contract-tested.
- Gap 4 remains until mutation code only requests `EBlueprintHelperSingletonControlFlowKind::Sequence` from the provider and no longer owns semantic query/request construction.

## Manual Commit Policy

- Do not run `git add`, `git commit`, or `git push`.
- Final output after implementation must give a suggested commit message and manual commands only.

## Task 1: Add Provider-Owned Canonical Request API

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSingletonControlFlowEvidenceProvider.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSingletonControlFlowEvidenceProvider.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericAssetStructControlActionClusterTests.cpp`

- [ ] **Step 1: Write failing provider API test**

In `BlueprintHelperGenericAssetStructControlActionClusterTests.cpp`, extend `FBlueprintHelperSingletonControlFlowProviderPositiveTest::RunTest` after the existing provider/core assertions with an explicit canonical request assertion:

```cpp
FBlueprintHelperActionResolutionRequest CanonicalRequest;
TestTrue(FString::Printf(TEXT("%s provider builds canonical request"), *Case.ExpectedStableIdPart),
	FBlueprintHelperSingletonControlFlowEvidenceProvider::TryBuildCanonicalRequest(
		Case.ExpectedKind,
		Blueprint,
		Graph,
		TEXT("singleton_boundary_positive"),
		TEXT("test_provider_positive"),
		CanonicalRequest));
TestEqual(FString::Printf(TEXT("%s canonical request first-level cluster"), *Case.ExpectedStableIdPart),
	CanonicalRequest.ClusterKind,
	EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction);
TestEqual(FString::Printf(TEXT("%s canonical request target graph"), *Case.ExpectedStableIdPart),
	CanonicalRequest.TargetGraph,
	Graph);
if (Case.ExpectedKind == EBlueprintHelperSingletonControlFlowKind::Select)
{
	TestEqual(TEXT("select canonical semantic kind"),
		CanonicalRequest.Semantic.Kind,
		EBlueprintHelperActionSemanticKind::Select);
}
else
{
	TestEqual(TEXT("control canonical semantic kind"),
		CanonicalRequest.Semantic.Kind,
		EBlueprintHelperActionSemanticKind::Control);
	TestTrue(TEXT("control canonical query is provider-owned"),
		!CanonicalRequest.Semantic.Query.IsEmpty());
}
```

- [ ] **Step 2: Run targeted test and confirm it fails**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Generic.SingletonControlFlowProviderPositive;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_SingletonBoundary_API_RED_001'
```

Expected: compile or automation failure because `TryBuildCanonicalRequest` is not declared.

- [ ] **Step 3: Add provider API declaration**

Add this declaration to `FBlueprintHelperSingletonControlFlowEvidenceProvider` in `BlueprintHelperSingletonControlFlowEvidenceProvider.h`:

```cpp
static bool TryBuildCanonicalRequest(
	EBlueprintHelperSingletonControlFlowKind Kind,
	UBlueprint* Blueprint,
	UEdGraph* TargetGraph,
	const FString& StatementId,
	const FString& Reason,
	FBlueprintHelperActionResolutionRequest& OutRequest);

static FBlueprintHelperActionResolutionResult ResolveCanonical(
	EBlueprintHelperSingletonControlFlowKind Kind,
	UBlueprint* Blueprint,
	UEdGraph* TargetGraph,
	const FString& StatementId,
	const FString& Reason);
```

- [ ] **Step 4: Implement provider-owned semantic mapping**

Add these helpers and methods to `BlueprintHelperSingletonControlFlowEvidenceProvider.cpp`:

```cpp
static FString SingletonKindToQuery(const EBlueprintHelperSingletonControlFlowKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperSingletonControlFlowKind::Branch:
		return TEXT("branch");
	case EBlueprintHelperSingletonControlFlowKind::Sequence:
		return TEXT("sequence");
	case EBlueprintHelperSingletonControlFlowKind::Return:
		return TEXT("return");
	case EBlueprintHelperSingletonControlFlowKind::Select:
		return FString();
	default:
		return FString();
	}
}

static EBlueprintHelperActionSemanticKind SingletonKindToSemanticKind(const EBlueprintHelperSingletonControlFlowKind Kind)
{
	return Kind == EBlueprintHelperSingletonControlFlowKind::Select
		? EBlueprintHelperActionSemanticKind::Select
		: EBlueprintHelperActionSemanticKind::Control;
}

bool FBlueprintHelperSingletonControlFlowEvidenceProvider::TryBuildCanonicalRequest(
	const EBlueprintHelperSingletonControlFlowKind Kind,
	UBlueprint* Blueprint,
	UEdGraph* TargetGraph,
	const FString& StatementId,
	const FString& Reason,
	FBlueprintHelperActionResolutionRequest& OutRequest)
{
	OutRequest = FBlueprintHelperActionResolutionRequest();
	if (!Blueprint || !TargetGraph || Kind == EBlueprintHelperSingletonControlFlowKind::Unknown)
	{
		return false;
	}

	OutRequest.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	OutRequest.Blueprint = Blueprint;
	OutRequest.TargetGraph = TargetGraph;
	OutRequest.StatementId = StatementId.IsEmpty()
		? FString::Printf(TEXT("singleton_%s"), SingletonKindToStableName(Kind))
		: StatementId;
	OutRequest.ProjectedContextHash = FString::Printf(TEXT("singleton_control_flow:%s:%s:projected"), *SingletonKindToQuery(Kind), *Reason);
	OutRequest.SemanticConstraintsHash = FString::Printf(TEXT("singleton_control_flow:%s:%s:constraints"), *SingletonKindToQuery(Kind), *Reason);
	OutRequest.Semantic.Kind = SingletonKindToSemanticKind(Kind);
	OutRequest.Semantic.Query = SingletonKindToQuery(Kind);
	OutRequest.Semantic.TargetPath = OutRequest.StatementId;
	OutRequest.MaxCandidates = 1;
	return true;
}

FBlueprintHelperActionResolutionResult FBlueprintHelperSingletonControlFlowEvidenceProvider::ResolveCanonical(
	const EBlueprintHelperSingletonControlFlowKind Kind,
	UBlueprint* Blueprint,
	UEdGraph* TargetGraph,
	const FString& StatementId,
	const FString& Reason)
{
	FBlueprintHelperActionResolutionRequest Request;
	if (!TryBuildCanonicalRequest(Kind, Blueprint, TargetGraph, StatementId, Reason, Request))
	{
		FBlueprintHelperActionResolutionResult Result;
		Result.Status = EBlueprintHelperActionResolutionStatus::Blocked;
		Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
		Result.ErrorCode = TEXT("missing_required_evidence");
		Result.Message = TEXT("Singleton control-flow canonical request requires Blueprint, target graph, and known singleton kind.");
		return Result;
	}

	FBlueprintHelperSingletonControlFlowEvidence Evidence;
	if (!TryResolve(Request, Evidence))
	{
		FBlueprintHelperActionResolutionResult Result;
		Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
		Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
		Result.ErrorCode = TEXT("unsupported_singleton_control_flow_semantic");
		Result.Message = FString::Printf(TEXT("Singleton control-flow provider could not resolve canonical kind '%s'."), SingletonKindToStableName(Kind));
		return Result;
	}

	return MakeResolvedResult(Request, Evidence);
}
```

- [ ] **Step 5: Run targeted test and confirm it passes**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Generic.SingletonControlFlowProviderPositive;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_SingletonBoundary_API_GREEN_001'
```

Expected: `1` succeeded, `0` failed.

## Task 2: Move Mutation Sequence Request Construction Behind Provider

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationCoordinator.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteLegacyMainlineContractTests.cpp`

- [ ] **Step 1: Write failing source contract for mutation boundary**

In `FBlueprintHelperSingletonControlDirectSpawnProviderBoundaryContractTest::RunTest`, after the existing `MutationCoordinator` forbidden direct-spawn token assertion, add:

```cpp
bClean &= AssertNoTokens(
	*this,
	TEXT("Private/Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationCoordinator.cpp"),
	{
		TEXT("Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Control"),
		TEXT("Request.Semantic.Query = TEXT(\"sequence\")"),
		TEXT("mutation_branch_fork_sequence_projected_context"),
		TEXT("mutation_branch_fork_sequence_semantic_constraints")
	});
```

- [ ] **Step 2: Run source contract and confirm it fails**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.LegacyMainline.SingletonControlDirectSpawnProviderBoundary;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_SingletonBoundary_MutationContract_RED_001'
```

Expected: FAIL because `BlueprintHelperGraphWriteMutationCoordinator.cpp` still constructs sequence semantic request details locally.

- [ ] **Step 3: Replace local mutation request construction**

Replace lines in `SpawnSequenceNode` that create and fill `FBlueprintHelperActionResolutionRequest Request`, call `TryResolve`, and call `MakeResolvedResult` with:

```cpp
UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
if (!Blueprint || !TargetGraph)
{
	OutError = TEXT("missing_required_evidence: mutation branch-fork sequence requires target graph and Blueprint context.");
	return nullptr;
}

const FBlueprintHelperActionResolutionResult ActionResult =
	FBlueprintHelperSingletonControlFlowEvidenceProvider::ResolveCanonical(
		EBlueprintHelperSingletonControlFlowKind::Sequence,
		Blueprint,
		TargetGraph,
		IntentId.IsEmpty() ? TEXT("merge_sequence") : IntentId,
		TEXT("mutation_branch_fork"));
if (!ActionResult.IsResolved())
{
	OutError = ActionResult.Message.IsEmpty()
		? TEXT("spawn_or_link_failure: singleton control-flow sequence provider did not resolve.")
		: FString::Printf(TEXT("spawn_or_link_failure: %s"), *ActionResult.Message);
	return nullptr;
}
```

Keep the existing adapter invocation:

```cpp
UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
	TargetGraph,
	ActionResult,
	FVector2D::ZeroVector,
	Options,
	OutError);
```

- [ ] **Step 4: Verify mutation source contract passes**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.LegacyMainline.SingletonControlDirectSpawnProviderBoundary;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_SingletonBoundary_MutationContract_GREEN_001'
```

Expected: `1` succeeded, `0` failed.

## Task 3: Add Runtime Evidence Guard For Branch-Fork Sequence

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp`

- [ ] **Step 1: Strengthen branch-fork runtime test**

In `FBlueprintHelperGraphWriteTaskRuntimeMergeBranchForkOwnedBlockCallReadBackTest::RunTest`, after the existing sequence reachability assertions, add a readback evidence guard:

```cpp
FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::FGraphWriteReadbackEvidence SequenceEvidence;
SequenceEvidence.bResolverEvidence = true;
SequenceEvidence.bSpawnEvidence = true;
SequenceEvidence.SingletonStableId = TEXT("singleton_control_flow:sequence");
TestTrue(TEXT("readback locates branch_fork sequence by singleton provider evidence"),
	FBlueprintHelperGraphWriteToolResultBaseTestsLocalUtils::ReadbackFindsSingletonControlByEvidence(Graph, SequenceEvidence));
```

- [ ] **Step 2: Run runtime branch-fork test**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.TaskRuntime.Merge.BranchForkOwnedBlockCallReadBack;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_SingletonBoundary_BranchForkRuntime_GREEN_001'
```

Expected: `1` succeeded, `0` failed. The test must still verify the inserted branch and original successor branch remain linked.

## Task 4: Update Gap 3 / Gap 4 Documentation

**Files:**
- Modify: `BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md`

- [ ] **Step 1: Mark Gap 3 closed only after provider API and tests pass**

In `BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`, update Gap 3 status to closed when all of these are true:

```text
- `FBlueprintHelperSingletonControlFlowEvidenceProvider::TryBuildCanonicalRequest` owns singleton kind -> semantic kind/query mapping.
- `FBlueprintHelperSingletonControlFlowEvidenceProvider::ResolveCanonical` owns `ActionResolutionResult`, stable id, candidate evidence, and selected spawner creation.
- `GenericAssetStructControlActionResolver` continues to call the provider for `Select` / `Control`.
- Wide-surface semantics still return false / unsupported and cannot use singleton direct spawn as fallback.
```

- [ ] **Step 2: Mark Gap 4 closed only after mutation contract and runtime tests pass**

Update Gap 4 status to closed when all of these are true:

```text
- `BlueprintHelperGraphWriteMutationCoordinator.cpp` no longer constructs singleton semantic request/query/hash details.
- Mutation code requests `EBlueprintHelperSingletonControlFlowKind::Sequence` through `ResolveCanonical`.
- Mutation code still uses `FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner`.
- `BlueprintHelper.GraphWrite.TaskRuntime.Merge.BranchForkOwnedBlockCallReadBack` confirms the branch-fork sequence node exists and is explainable by singleton provider evidence.
```

- [ ] **Step 3: Update FourCluster status**

In `BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md`, update `GenericAssetStructControlActionCluster` remaining gap text:

```text
Gap 3/4 closed: canonical singleton direct spawn is fixed behind `FBlueprintHelperSingletonControlFlowEvidenceProvider` as Generic cluster internal secondary semantic mapping; mutation branch-fork sequence creation reuses provider evidence and shared spawner adapter.
```

Keep `GenericAssetStructControlActionCluster` as `部分完成` if other broad `create` / `convert` / `schedule` semantics are still open.

## Task 5: Final Verification

**Files:**
- No source changes unless verification exposes a defect.

- [ ] **Step 1: Run focused automation**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Generic.SingletonControlFlowProviderPositive;BlueprintHelper.GraphWrite.LegacyMainline.SingletonControlDirectSpawnProviderBoundary;BlueprintHelper.GraphWrite.TaskRuntime.Merge.BranchForkOwnedBlockCallReadBack;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_SingletonBoundary_Focused_GREEN_001'
```

Expected: all focused tests succeed, `0` failed.

- [ ] **Step 2: Run GraphWrite regression**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_SingletonBoundary_Regression_001'
```

Expected: no failed tests. Warnings are acceptable only if they match the existing GraphWrite warning profile and are recorded.

- [ ] **Step 3: Run UE 5.6 BuildPlugin**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\RunUAT.bat' BuildPlugin -Plugin='D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\BlueprintHelper.uplugin' -Package='D:\UEProjects\Template\Saved\BlueprintHelperBuildTest_SingletonBoundary_001' -TargetPlatforms=Win64 -StrictIncludes
```

Expected: `BUILD SUCCESSFUL`, exit `0`.

- [ ] **Step 4: Run diff hygiene check**

Run:

```powershell
git diff --check
```

Expected: exit `0`; line-ending warnings may be reported by Git but there must be no whitespace errors.

## Self-Review Checklist

- [ ] Gap 3 requirement maps to Task 1 and Task 4.
- [ ] Gap 4 requirement maps to Task 2, Task 3, and Task 4.
- [ ] Direct spawn remains legal only inside `FBlueprintHelperSingletonControlFlowEvidenceProvider`.
- [ ] `MutationCoordinator` does not contain `Request.Semantic.Query = TEXT("sequence")` or local singleton semantic hash constants.
- [ ] Runtime branch-fork behavior remains unchanged: anchor links to sequence, inserted branch calls owned block, original successor branch is preserved.
- [ ] Documentation does not mark unrelated Generic broad semantics as complete.
