#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionCluster.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericActionProviderBoundary.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericAssetStructControlActionResolver.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperStructTypeStructureActionResolver.h"

namespace
{
static FBlueprintHelperActionResolutionResult MakeNeedsMoreSemanticContextResult(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperGenericActionProviderBoundary& Boundary)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode =
		(Request.Semantic.Kind == EBlueprintHelperActionSemanticKind::Construct
			|| Request.Semantic.Kind == EBlueprintHelperActionSemanticKind::Deconstruct)
		? TEXT("missing_required_evidence")
		: TEXT("needs_more_semantic_context");
	Result.Message = Boundary.Reason;
	return Result;
}

static FBlueprintHelperActionResolutionResult MakeDedicatedFragmentBuilderRequiredResult(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperGenericActionProviderBoundary& Boundary)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::Blocked;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = TEXT("dedicated_fragment_builder_required");
	Result.Message = FString::Printf(
		TEXT("%s Required builder: %s."),
		*Boundary.Reason,
		Boundary.RequiredBuilder.IsEmpty() ? TEXT("unknown") : *Boundary.RequiredBuilder);
	return Result;
}

static FBlueprintHelperActionResolutionResult MakeUnsupportedProviderBoundaryResult(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperGenericActionProviderBoundary& Boundary)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::UnsupportedIntent;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = TEXT("unsupported_generic_action_provider_boundary");
	Result.Message = FString::Printf(
		TEXT("%s Semantic kind: '%s'."),
		*Boundary.Reason,
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind));
	return Result;
}

static bool IsStructTypeStructureOperation(const FBlueprintHelperActionSemanticConstraints& Semantic)
{
	const bool bStructFamily = Semantic.SemanticFamily == EBlueprintHelperActionSemanticFamily::Struct
		|| Semantic.SemanticFamily == EBlueprintHelperActionSemanticFamily::TypeStructure;
	const bool bTypeOperation = Semantic.TypeOperation == EBlueprintHelperTypeOperation::Construct
		|| Semantic.TypeOperation == EBlueprintHelperTypeOperation::Deconstruct;
	return bStructFamily && bTypeOperation;
}
}

FBlueprintHelperActionResolutionResult FBlueprintHelperGenericAssetStructControlActionCluster::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context)
{
	if (!OwnsSemanticKind(Context.GetSemantic().Kind))
	{
		return MakeUnsupportedIntentResult(Request);
	}

	if (IsStructTypeStructureOperation(Context.GetSemantic()))
	{
		return FBlueprintHelperStructTypeStructureActionResolver::Resolve(Request, Context);
	}

	const FBlueprintHelperGenericActionProviderBoundary Boundary =
		FBlueprintHelperGenericActionProviderBoundaryService::Classify(Request);
	switch (Boundary.Mode)
	{
	case EBlueprintHelperGenericActionProviderMode::NodeSpawnerCandidate:
		return FBlueprintHelperGenericAssetStructControlActionResolver::ResolveNodeSpawnerCandidate(Request, Context);
	case EBlueprintHelperGenericActionProviderMode::DedicatedFragmentBuilderRequired:
		return MakeDedicatedFragmentBuilderRequiredResult(Request, Boundary);
	case EBlueprintHelperGenericActionProviderMode::NeedsMoreSemanticContext:
		return MakeNeedsMoreSemanticContextResult(Request, Boundary);
	case EBlueprintHelperGenericActionProviderMode::Unsupported:
	default:
		return MakeUnsupportedProviderBoundaryResult(Request, Boundary);
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
