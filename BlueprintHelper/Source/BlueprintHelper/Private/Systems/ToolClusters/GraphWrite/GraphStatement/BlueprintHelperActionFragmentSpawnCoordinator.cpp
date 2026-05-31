#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentSpawnCoordinator.h"

#include "K2Node.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentBuildUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/GraphWriteGraphStatementUtils.h"

bool FBlueprintHelperActionFragmentSpawnCoordinator::ValidateResolvedActionFragment(
	const FBlueprintHelperActionFragmentSpawnCoordinatorRequest& Request,
	FString& OutError,
	FString* OutSelectedStableId,
	TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions)
{
	if (OutSelectedStableId)
	{
		OutSelectedStableId->Reset();
	}
	if (!Request.BuildRequest)
	{
		OutError = TEXT("action fragment spawn coordinator requires a build request.");
		return false;
	}

	const FBlueprintHelperActionResolutionResult ActionResult =
		FBlueprintGraphWriteFacade::ResolveActionForGraph(Request.ActionRequest);
	if (!ActionResult.IsResolved())
	{
		FBlueprintHelperActionFragmentBuildUtils::AppendCandidateActionGroup(
			Request.CandidateGroupTarget,
			ActionResult,
			OutCandidateFunctions);
		OutError = ActionResult.Message.IsEmpty()
			? FString::Printf(TEXT("%s: %s"), *Request.FailurePrefix, *Request.CandidateGroupTarget)
			: ActionResult.Message;
		return false;
	}

	if (OutSelectedStableId)
	{
		*OutSelectedStableId = ActionResult.SelectedStableId;
	}
	return true;
}

bool FBlueprintHelperActionFragmentSpawnCoordinator::BuildResolvedActionFragment(
	const FBlueprintHelperActionFragmentSpawnCoordinatorRequest& Request,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError,
	TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions)
{
	OutFragment = FBlueprintHelperNodeFragment();
	if (!Request.BuildRequest)
	{
		OutError = TEXT("action fragment spawn coordinator requires a build request.");
		return false;
	}
	const FBlueprintHelperGraphFragmentBuildRequest& BuildRequest = *Request.BuildRequest;

	const FBlueprintHelperActionResolutionResult ActionResult =
		FBlueprintGraphWriteFacade::ResolveActionForGraph(Request.ActionRequest);
	if (!ActionResult.IsResolved())
	{
		FBlueprintHelperActionFragmentBuildUtils::AppendCandidateActionGroup(
			Request.CandidateGroupTarget,
			ActionResult,
			OutCandidateFunctions);
		OutError = ActionResult.Message.IsEmpty()
			? FString::Printf(TEXT("%s: %s"), *Request.FailurePrefix, *Request.CandidateGroupTarget)
			: ActionResult.Message;
		return false;
	}

	FBlueprintHelperActionNodeSpawnOptions SpawnOptions = Request.SpawnOptions;
	SpawnOptions.NodeId = BuildRequest.FragmentId;
	SpawnOptions.DefaultValues = BuildRequest.DefaultValues;
	UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
		Request.TargetGraph,
		ActionResult,
		FVector2D(BuildRequest.Location.X, BuildRequest.Location.Y),
		SpawnOptions,
		OutError);
	if (!SpawnedNode)
	{
		return false;
	}

	OutFragment.FragmentId = BuildRequest.FragmentId;
	OutFragment.SourceStatementId = BuildRequest.SourceStatementId.IsEmpty()
		? BuildRequest.FragmentId
		: BuildRequest.SourceStatementId;
	OutFragment.PrimaryNode = SpawnedNode;
	OutFragment.Nodes.Add(SpawnedNode);
	FBlueprintHelperActionFragmentBuildUtils::PopulatePins(
		Request.PinProfile,
		SpawnedNode,
		OutFragment);
	FBlueprintHelperActionFragmentBuildUtils::PopulateCommonMetadata(
		BuildRequest,
		OutFragment);
	if (Request.bAppendSemanticKindOwnershipTag)
	{
		OutFragment.OwnershipTags.Add(
			TEXT("semantic_kind"),
			FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.SemanticKind));
	}
	UGraphWriteGraphStatementUtils::AppendResolvedActionCandidateFacts(ActionResult, OutFragment);

	return true;
}
