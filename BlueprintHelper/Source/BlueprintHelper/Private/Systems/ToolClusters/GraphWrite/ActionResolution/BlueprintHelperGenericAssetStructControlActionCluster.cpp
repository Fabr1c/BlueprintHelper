#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionCluster.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericActionProviderBoundary.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionResolver.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericCreateActionResolver.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericTransformScheduleActionResolver.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperStructTypeStructureActionResolver.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionClusterUtils.h"

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericAssetStructControlActionCluster::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context)
{
	if (!OwnsSemanticKind(Context.GetSemantic().Kind))
	{
		return MakeUnsupportedIntentResult(Request);
	}

	if (UGraphWriteActionClusterUtils::IsStructTypeStructureOperation(Context.GetSemantic()))
	{
		return FBlueprintHelperStructTypeStructureActionResolver::Resolve(Request, Context);
	}

	if (Context.GetSemantic().Kind == EBlueprintHelperActionSemanticKind::Create)
	{
		return FBlueprintHelperGenericCreateActionResolver::Resolve(Request, Context);
	}

	if (Context.GetSemantic().Kind == EBlueprintHelperActionSemanticKind::Convert
		|| Context.GetSemantic().Kind == EBlueprintHelperActionSemanticKind::Schedule)
	{
		return FBlueprintHelperGenericTransformScheduleActionResolver::Resolve(Request, Context);
	}

	const FBlueprintHelperGenericActionProviderBoundary Boundary =
		FBlueprintHelperGenericActionProviderBoundaryService::Classify(Request);
	switch (Boundary.Mode)
	{
	case EBlueprintHelperGenericActionProviderMode::NodeSpawnerCandidate:
		return FBlueprintHelperGenericAssetStructControlActionResolver::ResolveNodeSpawnerCandidate(Request, Context);
	case EBlueprintHelperGenericActionProviderMode::DedicatedFragmentBuilderRequired:
		return FBlueprintHelperGenericAssetStructControlActionResolver::ResolveNodeSpawnerCandidate(Request, Context);
	case EBlueprintHelperGenericActionProviderMode::NeedsMoreSemanticContext:
		return UGraphWriteActionClusterUtils::MakeNeedsMoreSemanticContextResult(Request, Boundary);
	case EBlueprintHelperGenericActionProviderMode::Unsupported:
	default:
		return UGraphWriteActionClusterUtils::MakeUnsupportedProviderBoundaryResult(Request, Boundary);
	}
}

bool FBlueprintHelperGenericAssetStructControlActionCluster::OwnsSemanticKind(EBlueprintHelperActionSemanticKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperActionSemanticKind::Construct:
	case EBlueprintHelperActionSemanticKind::Deconstruct:
	case EBlueprintHelperActionSemanticKind::Select:
	case EBlueprintHelperActionSemanticKind::Control:
	case EBlueprintHelperActionSemanticKind::Create:
	case EBlueprintHelperActionSemanticKind::Convert:
	case EBlueprintHelperActionSemanticKind::Schedule:
		return true;
	default:
		return false;
	}
}

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericAssetStructControlActionCluster::MakeUnsupportedIntentResult(const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = TEXT("unsupported_generic_asset_struct_control_cluster_semantic");
	Result.Message = FString::Printf(
		TEXT("GenericAssetStructControlActionCluster does not own semantic kind '%s'."),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind));
	return Result;
}
