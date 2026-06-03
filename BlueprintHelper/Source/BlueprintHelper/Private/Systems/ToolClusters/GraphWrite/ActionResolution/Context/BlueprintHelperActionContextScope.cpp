#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextScope.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextRevisionGuard.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextRevisionService.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextSnapshotBuilder.h"

FBlueprintHelperActionContextRevisionToken FBlueprintHelperActionContextScope::MakeRevision(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FString& TaskRunId,
	const FString& PlanHash)
{
	return FBlueprintHelperActionContextRevisionService::BuildRevisionToken(
		Blueprint,
		Graph,
		TaskRunId,
		PlanHash);
}

bool FBlueprintHelperActionContextScope::Build(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const TArray<FBlueprintHelperActionContextDemand>& Demands,
	const FBlueprintHelperActionContextRevisionToken& Revision,
	FBlueprintHelperActionContextScope& OutScope,
	FString& OutError)
{
	OutScope = FBlueprintHelperActionContextScope();
	OutError.Reset();

	FBlueprintHelperActionContextSnapshot ContextSnapshot =
		FBlueprintHelperActionContextSnapshotBuilder::BuildSnapshot(Blueprint, Graph, Demands, Revision);
	FBlueprintHelperResolvedActionContextBundle ContextBundle =
		FBlueprintHelperActionContextInferenceService::Infer(ContextSnapshot, Demands);

	FString RevisionError;
	if (!FBlueprintHelperActionContextRevisionGuard::Validate(Revision, ContextBundle.Revision, RevisionError))
	{
		OutError = RevisionError;
		return false;
	}

	OutScope = FromResolved(MoveTemp(ContextSnapshot), MoveTemp(ContextBundle));
	return true;
}

FBlueprintHelperActionContextScope FBlueprintHelperActionContextScope::FromResolved(
	FBlueprintHelperActionContextSnapshot&& Snapshot,
	FBlueprintHelperResolvedActionContextBundle&& Bundle)
{
	FBlueprintHelperActionContextScope Scope;
	Scope.Snapshot = MoveTemp(Snapshot);
	Scope.Bundle = MoveTemp(Bundle);
	Scope.bValid = true;
	return Scope;
}

bool FBlueprintHelperActionContextScope::TryBuildRequest(
	const FString& StatementId,
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	FBlueprintHelperActionResolutionRequest& OutRequest,
	FString& OutError) const
{
	if (!bValid)
	{
		OutError = TEXT("action_context_scope_invalid");
		return false;
	}

	return FBlueprintHelperActionContextBundleProjector::TryBuildRequest(
		Bundle,
		StatementId,
		Blueprint,
		Graph,
		OutRequest,
		OutError);
}
