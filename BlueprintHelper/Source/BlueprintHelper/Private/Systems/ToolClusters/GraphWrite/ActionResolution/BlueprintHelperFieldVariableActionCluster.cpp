#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionCluster.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionResolver.h"

FBlueprintHelperActionResolutionResult FBlueprintHelperFieldVariableActionCluster::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context)
{
	if (!OwnsSemanticKind(Context.GetSemantic().Kind))
	{
		return MakeUnsupportedIntentResult(Request);
	}

	if (Context.GetSemantic().Kind == EBlueprintHelperActionSemanticKind::GetProperty
		|| Context.GetSemantic().Kind == EBlueprintHelperActionSemanticKind::SetProperty)
	{
		return MakeNeedsMoreSemanticContextResult(Request);
	}

	const FBlueprintHelperFieldVariableActionResolver Resolver;
	return Resolver.Resolve(Request, Context);
}

bool FBlueprintHelperFieldVariableActionCluster::OwnsSemanticKind(EBlueprintHelperActionSemanticKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperActionSemanticKind::Get:
	case EBlueprintHelperActionSemanticKind::Set:
	case EBlueprintHelperActionSemanticKind::GetProperty:
	case EBlueprintHelperActionSemanticKind::SetProperty:
		return true;
	default:
		return false;
	}
}

FBlueprintHelperActionResolutionResult FBlueprintHelperFieldVariableActionCluster::MakeUnsupportedIntentResult(const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FieldVariableAction;
	Result.ErrorCode = TEXT("unsupported_field_variable_cluster_semantic");
	Result.Message = FString::Printf(
		TEXT("FieldVariableActionCluster does not own semantic kind '%s'."),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind));
	return Result;
}

FBlueprintHelperActionResolutionResult FBlueprintHelperFieldVariableActionCluster::MakeNeedsMoreSemanticContextResult(const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FieldVariableAction;
	Result.ErrorCode = TEXT("needs_more_semantic_context");
	Result.Message = FString::Printf(
		TEXT("FieldVariableActionCluster needs typed target/property-path context for semantic kind '%s'."),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind));
	return Result;
}
