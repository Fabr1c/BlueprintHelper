#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperTypePromotionSpawnerEvidenceResolver.h"

#include "BlueprintActionDatabase.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "BlueprintTypePromotion.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_PromotableOperator.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/BlueprintHelperGraphActionUtils.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionResolverUtils.h"

FBlueprintHelperActionResolutionResult FBlueprintHelperTypePromotionSpawnerEvidenceResolver::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context)
{
	if (Context.GetSemantic().Kind != EBlueprintHelperActionSemanticKind::Convert)
	{
		return FBlueprintHelperGraphActionUtils::MakeInvalidResult(
			TEXT("type_promotion_requires_convert_semantic"),
			TEXT("type_promotion projected spawner evidence resolver only accepts Semantic.Kind=Convert."));
	}

	const FBlueprintHelperProjectedTypePromotionEvidence Evidence =
		FBlueprintHelperProjectedSpawnerEvidence::ReadTypePromotionEvidence(Request);
	if (!Evidence.IsComplete())
	{
		return FBlueprintHelperGraphActionUtils::MakeInvalidResult(
			TEXT("needs_more_semantic_context"),
			TEXT("type_promotion requires projected type-promotion spawner evidence."));
	}

	FEdGraphPinType SourcePinType;
	if (!UGraphWriteActionResolverUtils::TryBuildPrimitivePinType(Evidence.SourcePinType, SourcePinType))
	{
		return FBlueprintHelperGraphActionUtils::MakeInvalidResult(
			TEXT("invalid_type_promotion_source_pin_type"),
			FString::Printf(TEXT("type_promotion source pin type '%s' is not a supported primitive promotion token."), *Evidence.SourcePinType));
	}

	FEdGraphPinType TargetPinType;
	if (!UGraphWriteActionResolverUtils::TryBuildPrimitivePinType(Evidence.TargetPinType, TargetPinType))
	{
		return FBlueprintHelperGraphActionUtils::MakeInvalidResult(
			TEXT("invalid_type_promotion_target_pin_type"),
			FString::Printf(TEXT("type_promotion target pin type '%s' is not a supported primitive promotion token."), *Evidence.TargetPinType));
	}

	if (!UGraphWriteActionResolverUtils::IsPromotionCompatible(SourcePinType, TargetPinType))
	{
		return FBlueprintHelperGraphActionUtils::MakeInvalidResult(
			TEXT("invalid_type_promotion_evidence"),
			FString::Printf(
				TEXT("type_promotion evidence '%s' -> '%s' is not a valid UE primitive promotion."),
				*Evidence.SourcePinType,
				*Evidence.TargetPinType));
	}

	if (!Evidence.ResultPinType.IsEmpty())
	{
		FEdGraphPinType ResultPinType;
		if (!UGraphWriteActionResolverUtils::TryBuildPrimitivePinType(Evidence.ResultPinType, ResultPinType))
		{
			return FBlueprintHelperGraphActionUtils::MakeInvalidResult(
				TEXT("invalid_type_promotion_result_pin_type"),
				FString::Printf(TEXT("type_promotion result pin type '%s' is not a supported primitive promotion token."), *Evidence.ResultPinType));
		}

		if (!UGraphWriteActionResolverUtils::IsPromotionCompatible(TargetPinType, ResultPinType))
		{
			return FBlueprintHelperGraphActionUtils::MakeInvalidResult(
				TEXT("invalid_type_promotion_result_evidence"),
				FString::Printf(
					TEXT("type_promotion result pin type '%s' is not compatible with target pin type '%s'."),
					*Evidence.ResultPinType,
					*Evidence.TargetPinType));
		}
	}

	const FString ComputedStableId = FBlueprintHelperProjectedSpawnerEvidence::MakeTypePromotionStableId(
		Evidence.OperatorName,
		Evidence.SourcePinType,
		Evidence.TargetPinType);
	if (!Evidence.StableId.IsEmpty() && !Evidence.StableId.Equals(ComputedStableId, ESearchCase::IgnoreCase))
	{
		return FBlueprintHelperGraphActionUtils::MakeInvalidResult(
			TEXT("type_promotion_stable_id_mismatch"),
			FString::Printf(
				TEXT("type_promotion stable id '%s' does not match projected evidence '%s'."),
				*Evidence.StableId,
				*ComputedStableId));
	}

	const FName OperatorName(*Evidence.OperatorName);
	UBlueprintFunctionNodeSpawner* Spawner = UGraphWriteActionResolverUtils::FindRegisteredTypePromotionSpawner(OperatorName);
	if (!Spawner)
	{
		return UGraphWriteActionResolverUtils::MakeNotFoundResult(Evidence);
	}

	const FString SelectedStableId = Evidence.StableId.IsEmpty() ? ComputedStableId : Evidence.StableId;

	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.SelectedStableId = SelectedStableId;
	Result.SelectedSpawner = Spawner;
	Result.CandidateActions.Add(UGraphWriteActionResolverUtils::MakeCandidateInfo(SelectedStableId, Evidence, Spawner));
	Result.SpawnerClass = Spawner->GetClass()->GetPathName();
	Result.NodeClass = UK2Node_PromotableOperator::StaticClass()->GetPathName();
	Result.MatchReason = FString::Printf(
		TEXT("type_promotion operator=%s source=%s target=%s provider=FTypePromotion"),
		*Evidence.OperatorName,
		*Evidence.SourcePinType,
		*Evidence.TargetPinType);
	Result.Message = FString::Printf(
		TEXT("Resolved type_promotion from FTypePromotion operator spawner '%s'."),
		*SelectedStableId);
	return Result;
}