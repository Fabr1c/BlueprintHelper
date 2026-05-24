#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h"

#include "BlueprintNodeSpawner.h"

namespace
{
static FString ReadTrimmedEvidenceValue(
	const FBlueprintHelperActionResolutionRequest& Request,
	const TCHAR* Key)
{
	if (const FString* Value = Request.ContextEvidence.Find(Key))
	{
		return Value->TrimStartAndEnd();
	}
	return FString();
}
}

bool FBlueprintHelperProjectedAssetActionEvidence::HasSelector() const
{
	return !StableId.IsEmpty()
		|| !NodeClassPath.IsEmpty()
		|| !SpawnerSignature.IsEmpty()
		|| !OwnerPath.IsEmpty()
		|| !Query.IsEmpty()
		|| !MenuName.IsEmpty()
		|| !Category.IsEmpty();
}

bool FBlueprintHelperProjectedTypePromotionEvidence::IsComplete() const
{
	return !OperatorName.IsEmpty()
		&& !SourcePinType.IsEmpty()
		&& !TargetPinType.IsEmpty();
}

FBlueprintHelperProjectedAssetActionEvidence FBlueprintHelperProjectedSpawnerEvidence::ReadAssetActionEvidence(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperProjectedAssetActionEvidence Evidence;
	Evidence.StableId = ReadTrimmedEvidenceValue(Request, TEXT("asset_action_stable_id"));
	Evidence.NodeClassPath = ReadTrimmedEvidenceValue(Request, TEXT("asset_action_node_class"));
	Evidence.SpawnerSignature = ReadTrimmedEvidenceValue(Request, TEXT("asset_action_spawner_signature"));
	Evidence.OwnerPath = ReadTrimmedEvidenceValue(Request, TEXT("asset_action_owner_path"));
	Evidence.Query = ReadTrimmedEvidenceValue(Request, TEXT("asset_action_query"));
	Evidence.MenuName = ReadTrimmedEvidenceValue(Request, TEXT("asset_action_menu_name"));
	Evidence.Category = ReadTrimmedEvidenceValue(Request, TEXT("asset_action_category"));
	return Evidence;
}

FBlueprintHelperProjectedTypePromotionEvidence FBlueprintHelperProjectedSpawnerEvidence::ReadTypePromotionEvidence(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperProjectedTypePromotionEvidence Evidence;
	Evidence.StableId = ReadTrimmedEvidenceValue(Request, TEXT("type_promotion_stable_id"));
	Evidence.OperatorName = ReadTrimmedEvidenceValue(Request, TEXT("type_promotion_operator"));
	Evidence.SourcePinType = ReadTrimmedEvidenceValue(Request, TEXT("type_promotion_source_pin_type"));
	Evidence.TargetPinType = ReadTrimmedEvidenceValue(Request, TEXT("type_promotion_target_pin_type"));
	Evidence.ResultPinType = ReadTrimmedEvidenceValue(Request, TEXT("type_promotion_result_pin_type"));
	return Evidence;
}

FString FBlueprintHelperProjectedSpawnerEvidence::MakeAssetActionStableId(
	const UObject* ActionOwner,
	const UBlueprintNodeSpawner* Spawner,
	const UClass* NodeClass)
{
	return FString::Printf(
		TEXT("action_database:%s:%s:%s"),
		ActionOwner ? *ActionOwner->GetPathName() : TEXT("none"),
		NodeClass ? *NodeClass->GetPathName() : TEXT("none"),
		Spawner ? *Spawner->GetSpawnerSignature().ToString() : TEXT("none"));
}

FString FBlueprintHelperProjectedSpawnerEvidence::MakeTypePromotionStableId(
	const FString& OperatorName,
	const FString& SourcePinType,
	const FString& TargetPinType)
{
	return FString::Printf(
		TEXT("type_promotion:%s:%s:%s"),
		*OperatorName.TrimStartAndEnd(),
		*SourcePinType.TrimStartAndEnd(),
		*TargetPinType.TrimStartAndEnd());
}
