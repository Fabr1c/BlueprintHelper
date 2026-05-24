#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperTypePromotionSpawnerEvidenceResolver.h"

#include "BlueprintActionDatabase.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "BlueprintTypePromotion.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_PromotableOperator.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionClusterContextView.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h"

namespace
{
static FString NormalizeTypePromotionToken(const FString& Value)
{
	FString Result = Value.TrimStartAndEnd().ToLower();
	Result.ReplaceInline(TEXT(" "), TEXT(""));
	Result.ReplaceInline(TEXT("_"), TEXT(""));
	Result.ReplaceInline(TEXT("-"), TEXT(""));
	return Result;
}

static bool TryBuildPrimitivePinType(const FString& TypeToken, FEdGraphPinType& OutPinType)
{
	OutPinType = FEdGraphPinType();

	const FString Normalized = NormalizeTypePromotionToken(TypeToken);
	if (Normalized.IsEmpty())
	{
		return false;
	}

	if (Normalized == TEXT("int") || Normalized == TEXT("int32") || Normalized == TEXT("integer"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
		return true;
	}
	if (Normalized == TEXT("int64") || Normalized == TEXT("integer64"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
		return true;
	}
	if (Normalized == TEXT("byte"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
		return true;
	}
	if (Normalized == TEXT("bool") || Normalized == TEXT("boolean"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		return true;
	}
	if (Normalized == TEXT("float") || Normalized == TEXT("single"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
		return true;
	}
	if (Normalized == TEXT("double") || Normalized == TEXT("real"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
		return true;
	}
	if (Normalized == TEXT("wildcard"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
		return true;
	}

	return false;
}

static bool IsPromotionCompatible(const FEdGraphPinType& SourcePinType, const FEdGraphPinType& TargetPinType)
{
	return SourcePinType == TargetPinType
		|| FTypePromotion::IsValidPromotion(SourcePinType, TargetPinType);
}

static UBlueprintFunctionNodeSpawner* FindRegisteredTypePromotionSpawner(FName OperatorName)
{
	if (OperatorName.IsNone())
	{
		return nullptr;
	}

	FTypePromotion::Get();
	if (UBlueprintFunctionNodeSpawner* Spawner = FTypePromotion::GetOperatorSpawner(OperatorName))
	{
		return Spawner;
	}

	FBlueprintActionDatabase::Get().RefreshAll();
	return FTypePromotion::GetOperatorSpawner(OperatorName);
}

static FBlueprintHelperActionResolutionResult MakeInvalidResult(
	const FString& ErrorCode,
	const FString& Message)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = ErrorCode;
	Result.Message = Message;
	return Result;
}

static FBlueprintHelperActionResolutionResult MakeNotFoundResult(
	const FBlueprintHelperProjectedTypePromotionEvidence& Evidence)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::NotFound;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = TEXT("type_promotion_spawner_not_found");
	Result.Message = FString::Printf(
		TEXT("FTypePromotion did not expose a registered operator spawner for '%s'."),
		Evidence.OperatorName.IsEmpty() ? TEXT("none") : *Evidence.OperatorName);
	Result.MatchReason = FString::Printf(
		TEXT("type_promotion operator=%s source=%s target=%s provider=FTypePromotion spawner=not_found"),
		Evidence.OperatorName.IsEmpty() ? TEXT("none") : *Evidence.OperatorName,
		Evidence.SourcePinType.IsEmpty() ? TEXT("none") : *Evidence.SourcePinType,
		Evidence.TargetPinType.IsEmpty() ? TEXT("none") : *Evidence.TargetPinType);
	return Result;
}

static FBlueprintHelperCallFunctionCandidateInfo MakeCandidateInfo(
	const FString& StableId,
	const FBlueprintHelperProjectedTypePromotionEvidence& Evidence,
	UBlueprintFunctionNodeSpawner* Spawner)
{
	FBlueprintHelperCallFunctionCandidateInfo Candidate;
	Candidate.StableId = StableId;
	Candidate.DisplayName = Evidence.OperatorName;
	Candidate.Category = TEXT("Utilities|Operators");
	Candidate.NodeClassPath = UK2Node_PromotableOperator::StaticClass()->GetPathName();
	Candidate.MatchReason = FString::Printf(
		TEXT("type_promotion operator=%s source=%s target=%s provider=FTypePromotion"),
		*Evidence.OperatorName,
		*Evidence.SourcePinType,
		*Evidence.TargetPinType);
	Candidate.ReturnType = Evidence.ResultPinType.IsEmpty() ? Evidence.TargetPinType : Evidence.ResultPinType;
	Candidate.Score = 100;
	Candidate.bGraphCompatible = Spawner != nullptr;
	Candidate.bFromActionDatabase = true;
	Candidate.bBlueprintCallable = true;
	Candidate.bBlueprintPure = true;
	return Candidate;
}
}

FBlueprintHelperActionResolutionResult FBlueprintHelperTypePromotionSpawnerEvidenceResolver::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request,
	const FBlueprintHelperActionClusterContextView& Context)
{
	if (Context.GetSemantic().Kind != EBlueprintHelperActionSemanticKind::Convert)
	{
		return MakeInvalidResult(
			TEXT("type_promotion_requires_convert_semantic"),
			TEXT("type_promotion projected spawner evidence resolver only accepts Semantic.Kind=Convert."));
	}

	const FBlueprintHelperProjectedTypePromotionEvidence Evidence =
		FBlueprintHelperProjectedSpawnerEvidence::ReadTypePromotionEvidence(Request);
	if (!Evidence.IsComplete())
	{
		return MakeInvalidResult(
			TEXT("needs_more_semantic_context"),
			TEXT("type_promotion requires projected type-promotion spawner evidence."));
	}

	FEdGraphPinType SourcePinType;
	if (!TryBuildPrimitivePinType(Evidence.SourcePinType, SourcePinType))
	{
		return MakeInvalidResult(
			TEXT("invalid_type_promotion_source_pin_type"),
			FString::Printf(TEXT("type_promotion source pin type '%s' is not a supported primitive promotion token."), *Evidence.SourcePinType));
	}

	FEdGraphPinType TargetPinType;
	if (!TryBuildPrimitivePinType(Evidence.TargetPinType, TargetPinType))
	{
		return MakeInvalidResult(
			TEXT("invalid_type_promotion_target_pin_type"),
			FString::Printf(TEXT("type_promotion target pin type '%s' is not a supported primitive promotion token."), *Evidence.TargetPinType));
	}

	if (!IsPromotionCompatible(SourcePinType, TargetPinType))
	{
		return MakeInvalidResult(
			TEXT("invalid_type_promotion_evidence"),
			FString::Printf(
				TEXT("type_promotion evidence '%s' -> '%s' is not a valid UE primitive promotion."),
				*Evidence.SourcePinType,
				*Evidence.TargetPinType));
	}

	if (!Evidence.ResultPinType.IsEmpty())
	{
		FEdGraphPinType ResultPinType;
		if (!TryBuildPrimitivePinType(Evidence.ResultPinType, ResultPinType))
		{
			return MakeInvalidResult(
				TEXT("invalid_type_promotion_result_pin_type"),
				FString::Printf(TEXT("type_promotion result pin type '%s' is not a supported primitive promotion token."), *Evidence.ResultPinType));
		}

		if (!IsPromotionCompatible(TargetPinType, ResultPinType))
		{
			return MakeInvalidResult(
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
		return MakeInvalidResult(
			TEXT("type_promotion_stable_id_mismatch"),
			FString::Printf(
				TEXT("type_promotion stable id '%s' does not match projected evidence '%s'."),
				*Evidence.StableId,
				*ComputedStableId));
	}

	const FName OperatorName(*Evidence.OperatorName);
	UBlueprintFunctionNodeSpawner* Spawner = FindRegisteredTypePromotionSpawner(OperatorName);
	if (!Spawner)
	{
		return MakeNotFoundResult(Evidence);
	}

	const FString SelectedStableId = Evidence.StableId.IsEmpty() ? ComputedStableId : Evidence.StableId;

	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.SelectedStableId = SelectedStableId;
	Result.SelectedSpawner = Spawner;
	Result.CandidateActions.Add(MakeCandidateInfo(SelectedStableId, Evidence, Spawner));
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
