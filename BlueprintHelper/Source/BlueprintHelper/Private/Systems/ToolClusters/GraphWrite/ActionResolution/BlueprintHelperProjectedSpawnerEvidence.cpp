#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionEvidenceUtils.h"

#include "BlueprintNodeSpawner.h"

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

bool FBlueprintHelperProjectedAssetActionEvidence::HasProjectedIdentity() const
{
	return !StableId.IsEmpty()
		&& !NodeClassPath.IsEmpty()
		&& !SpawnerSignature.IsEmpty()
		&& !OwnerPath.IsEmpty();
}

bool FBlueprintHelperProjectedTypePromotionEvidence::IsComplete() const
{
	return !OperatorName.IsEmpty()
		&& !SourcePinType.IsEmpty()
		&& !TargetPinType.IsEmpty();
}

bool FBlueprintHelperProjectedScheduleActionEvidence::HasSelector() const
{
	return !StableId.IsEmpty()
		|| !NodeClassPath.IsEmpty()
		|| !SpawnerSignature.IsEmpty()
		|| !OwnerPath.IsEmpty()
		|| !Query.IsEmpty()
		|| !MenuName.IsEmpty()
		|| !Category.IsEmpty();
}

bool FBlueprintHelperProjectedScheduleActionEvidence::HasProjectedIdentity() const
{
	return !StableId.IsEmpty()
		&& !NodeClassPath.IsEmpty()
		&& !SpawnerSignature.IsEmpty()
		&& !OwnerPath.IsEmpty();
}

bool FBlueprintHelperProjectedScheduleActionEvidence::HasTimerHandlerEvidence() const
{
	return !HandlerName.IsEmpty()
		&& !HandlerFunctionPath.IsEmpty()
		&& !HandlerSourceCluster.IsEmpty()
		&& !SignatureEvidenceId.IsEmpty();
}

bool FBlueprintHelperProjectedScheduleActionEvidence::IsGraphLatentAllowed() const
{
	const FString Normalized = GraphLatentAllowed.TrimStartAndEnd().ToLower();
	return Normalized == TEXT("true") || Normalized == TEXT("1") || Normalized == TEXT("yes");
}

FBlueprintHelperProjectedAssetActionEvidence FBlueprintHelperProjectedSpawnerEvidence::ReadAssetActionEvidence(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperProjectedAssetActionEvidence Evidence;
	Evidence.StableId = UGraphWriteActionEvidenceUtils::ReadTrimmedEvidenceValue(Request, TEXT("asset_action_stable_id"));
	Evidence.NodeClassPath = UGraphWriteActionEvidenceUtils::ReadTrimmedEvidenceValue(Request, TEXT("asset_action_node_class"));
	Evidence.SpawnerSignature = UGraphWriteActionEvidenceUtils::ReadTrimmedEvidenceValue(Request, TEXT("asset_action_spawner_signature"));
	Evidence.OwnerPath = UGraphWriteActionEvidenceUtils::ReadTrimmedEvidenceValue(Request, TEXT("asset_action_owner_path"));
	Evidence.Query = UGraphWriteActionEvidenceUtils::ReadTrimmedEvidenceValue(Request, TEXT("asset_action_query"));
	Evidence.MenuName = UGraphWriteActionEvidenceUtils::ReadTrimmedEvidenceValue(Request, TEXT("asset_action_menu_name"));
	Evidence.Category = UGraphWriteActionEvidenceUtils::ReadTrimmedEvidenceValue(Request, TEXT("asset_action_category"));
	return Evidence;
}

void FBlueprintHelperProjectedSpawnerEvidence::WriteAssetActionEvidence(
	const FBlueprintHelperProjectedAssetActionEvidence& Evidence,
	TMap<FString, FString>& OutContextEvidence)
{
	OutContextEvidence.Add(TEXT("asset_action_stable_id"), Evidence.StableId);
	OutContextEvidence.Add(TEXT("asset_action_node_class"), Evidence.NodeClassPath);
	OutContextEvidence.Add(TEXT("asset_action_spawner_signature"), Evidence.SpawnerSignature);
	OutContextEvidence.Add(TEXT("asset_action_owner_path"), Evidence.OwnerPath);
	OutContextEvidence.Add(TEXT("asset_action_query"), Evidence.Query);
	OutContextEvidence.Add(TEXT("asset_action_menu_name"), Evidence.MenuName);
	OutContextEvidence.Add(TEXT("asset_action_category"), Evidence.Category);
}

FBlueprintHelperProjectedTypePromotionEvidence FBlueprintHelperProjectedSpawnerEvidence::ReadTypePromotionEvidence(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperProjectedTypePromotionEvidence Evidence;
	Evidence.StableId = UGraphWriteActionEvidenceUtils::ReadTrimmedEvidenceValue(Request, TEXT("type_promotion_stable_id"));
	Evidence.OperatorName = UGraphWriteActionEvidenceUtils::ReadTrimmedEvidenceValue(Request, TEXT("type_promotion_operator"));
	Evidence.SourcePinType = UGraphWriteActionEvidenceUtils::ReadTrimmedEvidenceValue(Request, TEXT("type_promotion_source_pin_type"));
	Evidence.TargetPinType = UGraphWriteActionEvidenceUtils::ReadTrimmedEvidenceValue(Request, TEXT("type_promotion_target_pin_type"));
	Evidence.ResultPinType = UGraphWriteActionEvidenceUtils::ReadTrimmedEvidenceValue(Request, TEXT("type_promotion_result_pin_type"));
	return Evidence;
}

FBlueprintHelperProjectedScheduleActionEvidence FBlueprintHelperProjectedSpawnerEvidence::ReadScheduleActionEvidence(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	FBlueprintHelperProjectedScheduleActionEvidence Evidence;
	Evidence.StableId = UGraphWriteActionEvidenceUtils::ReadTrimmedEvidenceValue(Request, TEXT("schedule_action_stable_id"));
	Evidence.NodeClassPath = UGraphWriteActionEvidenceUtils::ReadTrimmedEvidenceValue(Request, TEXT("schedule_node_class"));
	Evidence.SpawnerSignature = UGraphWriteActionEvidenceUtils::ReadTrimmedEvidenceValue(Request, TEXT("schedule_spawner_signature"));
	Evidence.OwnerPath = UGraphWriteActionEvidenceUtils::ReadTrimmedEvidenceValue(Request, TEXT("schedule_owner_path"));
	Evidence.Query = UGraphWriteActionEvidenceUtils::ReadTrimmedEvidenceValue(Request, TEXT("schedule_query"));
	Evidence.MenuName = UGraphWriteActionEvidenceUtils::ReadTrimmedEvidenceValue(Request, TEXT("schedule_menu_name"));
	Evidence.Category = UGraphWriteActionEvidenceUtils::ReadTrimmedEvidenceValue(Request, TEXT("schedule_category"));
	Evidence.DelegatePinName = UGraphWriteActionEvidenceUtils::ReadTrimmedEvidenceValue(Request, TEXT("schedule_delegate_pin_name"));
	Evidence.HandlerName = UGraphWriteActionEvidenceUtils::ReadTrimmedEvidenceValue(Request, TEXT("handler_name"));
	Evidence.HandlerFunctionPath = UGraphWriteActionEvidenceUtils::ReadTrimmedEvidenceValue(Request, TEXT("handler_function_path"));
	Evidence.HandlerSourceCluster = UGraphWriteActionEvidenceUtils::ReadTrimmedEvidenceValue(Request, TEXT("handler_source_cluster"));
	Evidence.SignatureEvidenceId = UGraphWriteActionEvidenceUtils::ReadTrimmedEvidenceValue(Request, TEXT("signature_evidence_id"));
	Evidence.GraphLatentAllowed = UGraphWriteActionEvidenceUtils::ReadTrimmedEvidenceValue(Request, TEXT("graph_latent_allowed"));
	return Evidence;
}

void FBlueprintHelperProjectedSpawnerEvidence::WriteScheduleActionEvidence(
	const FBlueprintHelperProjectedScheduleActionEvidence& Evidence,
	TMap<FString, FString>& OutContextEvidence)
{
	OutContextEvidence.Add(TEXT("schedule_action_stable_id"), Evidence.StableId);
	OutContextEvidence.Add(TEXT("schedule_node_class"), Evidence.NodeClassPath);
	OutContextEvidence.Add(TEXT("schedule_spawner_signature"), Evidence.SpawnerSignature);
	OutContextEvidence.Add(TEXT("schedule_owner_path"), Evidence.OwnerPath);
	OutContextEvidence.Add(TEXT("schedule_query"), Evidence.Query);
	OutContextEvidence.Add(TEXT("schedule_menu_name"), Evidence.MenuName);
	OutContextEvidence.Add(TEXT("schedule_category"), Evidence.Category);
	OutContextEvidence.Add(TEXT("schedule_delegate_pin_name"), Evidence.DelegatePinName);
	OutContextEvidence.Add(TEXT("handler_name"), Evidence.HandlerName);
	OutContextEvidence.Add(TEXT("handler_function_path"), Evidence.HandlerFunctionPath);
	OutContextEvidence.Add(TEXT("handler_source_cluster"), Evidence.HandlerSourceCluster);
	OutContextEvidence.Add(TEXT("signature_evidence_id"), Evidence.SignatureEvidenceId);
	OutContextEvidence.Add(TEXT("graph_latent_allowed"), Evidence.GraphLatentAllowed);
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

FString FBlueprintHelperProjectedSpawnerEvidence::MakeScheduleActionStableId(
	const UObject* ActionOwner,
	const UBlueprintNodeSpawner* Spawner,
	const UClass* NodeClass)
{
	return MakeAssetActionStableId(ActionOwner, Spawner, NodeClass);
}
