#include "Systems/Review/BlueprintHelperClassDefaultSetterRestoreAdapter.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassSettingsService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"

class FBlueprintHelperClassDefaultSetterRestoreAdapterLocal
{
public:
	static bool TryBuildRestoreSetting(
		const TSharedPtr<FJsonObject>& Snapshot,
		FString& OutAssetPath,
		FBlueprintHelperClassDefaultPropertySetting& OutSetting,
		FString& OutError)
	{
		if (!Snapshot.IsValid())
		{
			OutError = TEXT("class_default_setter_restore_snapshot_parse_failed");
			return false;
		}

		FString Schema;
		Snapshot->TryGetStringField(TEXT("schema"), Schema);
		if (!Schema.Equals(TEXT("BlueprintHelper.ReviewTargetSnapshot.v2"), ESearchCase::IgnoreCase))
		{
			OutError = TEXT("class_default_setter_restore_snapshot_invalid");
			return false;
		}

		FString TargetKind;
		Snapshot->TryGetStringField(TEXT("target_kind"), TargetKind);
		if (!TargetKind.Equals(TEXT("class_default_setter_property"), ESearchCase::IgnoreCase))
		{
			OutError = TEXT("class_default_setter_restore_snapshot_invalid");
			return false;
		}

		FString AssetPath;
		FString PropertyPath;
		Snapshot->TryGetStringField(TEXT("asset_path"), AssetPath);
		Snapshot->TryGetStringField(TEXT("property_path"), PropertyPath);
		const TSharedPtr<FJsonValue> Value = Snapshot->TryGetField(TEXT("value"));
		if (AssetPath.IsEmpty() || PropertyPath.IsEmpty() || !Value.IsValid())
		{
			OutError = TEXT("class_default_setter_restore_snapshot_missing_target");
			return false;
		}

		OutAssetPath = AssetPath;
		OutSetting.PropertyPath = PropertyPath;
		OutSetting.Value = Value;
		OutSetting.MutationStrategy = TEXT("setter_aware_property");
		return true;
	}
};

FString FBlueprintHelperClassDefaultSetterRestoreAdapter::GetTargetKind() const
{
	return TEXT("class_default_setter_property");
}

FBlueprintHelperReviewRestoreResult FBlueprintHelperClassDefaultSetterRestoreAdapter::RestoreBeforeSnapshot(
	const FBlueprintHelperReviewVisibleChange& Change) const
{
	FBlueprintHelperReviewRestoreResult Result;
	Result.RollbackMode = TEXT("setter_aware_class_default");
	if (Change.AtomicTargets.Num() != 1)
	{
		Result.Message = TEXT("restore_adapter_requires_single_atomic_target");
		return Result;
	}

	const FBlueprintHelperReviewAtomicTarget& Target = Change.AtomicTargets[0];
	if (Target.BeforeSnapshotJson.IsEmpty())
	{
		Result.Message = TEXT("missing_recoverable_snapshot");
		return Result;
	}

	TSharedPtr<FJsonObject> Snapshot;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Target.BeforeSnapshotJson);
	if (!FJsonSerializer::Deserialize(Reader, Snapshot) || !Snapshot.IsValid())
	{
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::RejectFailed;
		Result.Message = TEXT("class_default_setter_restore_snapshot_parse_failed");
		return Result;
	}

	FString AssetPath;
	FBlueprintHelperClassDefaultPropertySetting Setting;
	FString SnapshotError;
	if (!FBlueprintHelperClassDefaultSetterRestoreAdapterLocal::TryBuildRestoreSetting(
		Snapshot,
		AssetPath,
		Setting,
		SnapshotError))
	{
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::RejectFailed;
		Result.Message = SnapshotError;
		return Result;
	}

	FBlueprintHelperGraphResolver Resolver;
	const FBlueprintHelperClassSettingsService Service(Resolver);
	const FBlueprintHelperToolResultBase RestoreResult = Service.SetClassDefaultProperties(
		AssetPath,
		{ Setting },
		false);
	if (!RestoreResult.bOk && RestoreResult.Status != EBlueprintHelperToolStatus::NoOp)
	{
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::RejectFailed;
		Result.Message = RestoreResult.Error.IsSet()
			? RestoreResult.Error->Code
			: TEXT("class_default_setter_restore_failed");
		return Result;
	}

	Result.bSucceeded = true;
	Result.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
	Result.Message = TEXT("rejected");
	Result.bSupersededDataCompactionEligible = true;
	return Result;
}
