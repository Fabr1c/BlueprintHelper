#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"

FBlueprintHelperActionClusterContextView::FBlueprintHelperActionClusterContextView(
	const FBlueprintHelperActionResolutionRequest& InRequest)
	: Request(InRequest)
{
}

const FBlueprintHelperActionResolutionRequest& FBlueprintHelperActionClusterContextView::GetRequest() const
{
	return Request;
}

EBlueprintHelperSpawnerClusterKind FBlueprintHelperActionClusterContextView::GetClusterKind() const
{
	return Request.ClusterKind;
}

const FBlueprintHelperActionSemanticConstraints& FBlueprintHelperActionClusterContextView::GetSemantic() const
{
	return Request.Semantic;
}

const FString& FBlueprintHelperActionClusterContextView::GetStatementId() const
{
	return Request.StatementId;
}

const FString& FBlueprintHelperActionClusterContextView::GetProjectedContextHash() const
{
	return Request.ProjectedContextHash;
}

const FString& FBlueprintHelperActionClusterContextView::GetSemanticConstraintsHash() const
{
	return Request.SemanticConstraintsHash;
}

const TMap<FString, FString>& FBlueprintHelperActionClusterContextView::GetEvidence() const
{
	return Request.ContextEvidence;
}

bool FBlueprintHelperActionClusterContextView::HasGraphContext() const
{
	return Request.Blueprint != nullptr && Request.TargetGraph != nullptr;
}

bool FBlueprintHelperActionClusterContextView::HasSemanticKind() const
{
	return Request.Semantic.Kind != EBlueprintHelperActionSemanticKind::Unknown;
}

bool FBlueprintHelperActionClusterContextView::HasStableIdentity() const
{
	return !Request.StatementId.IsEmpty()
		&& !Request.ProjectedContextHash.IsEmpty()
		&& !Request.SemanticConstraintsHash.IsEmpty();
}

bool FBlueprintHelperActionClusterContextView::IsCompleteForCluster(
	EBlueprintHelperSpawnerClusterKind ExpectedCluster,
	FString& OutCode,
	FString& OutMessage) const
{
	if (Request.ClusterKind != ExpectedCluster)
	{
		OutCode = TEXT("invalid_cluster_dispatch");
		OutMessage = FString::Printf(
			TEXT("Invalid ActionResolution cluster dispatch: expected=%s actual=%s."),
			*FBlueprintHelperActionResolutionCore::ClusterKindToString(ExpectedCluster),
			*FBlueprintHelperActionResolutionCore::ClusterKindToString(Request.ClusterKind));
		return false;
	}

	if (!HasGraphContext())
	{
		OutCode = TEXT("action_context_graph_missing");
		OutMessage = TEXT("Projected action context is missing Blueprint or target graph.");
		return false;
	}

	if (!HasSemanticKind())
	{
		OutCode = TEXT("semantic_kind_missing");
		OutMessage = TEXT("Projected action context is missing semantic kind.");
		return false;
	}

	if (!HasStableIdentity())
	{
		OutCode = TEXT("action_context_identity_missing");
		OutMessage = TEXT("Projected action context is missing statement/context identity.");
		return false;
	}

	return true;
}
