// BlueprintHelper Review asset factory restore adapter implementation.

#include "Systems/Review/BlueprintHelperReviewAssetFactoryRestoreAdapter.h"
#include "Systems/Review/Utils/BlueprintHelperReviewSnapshotRestoreService.h"

#include "ObjectTools.h"
#include "UObject/UObjectGlobals.h"

FString FBlueprintHelperReviewAssetFactoryRestoreAdapter::GetTargetKind() const
{
	return TEXT("asset_factory");
}

FBlueprintHelperReviewRestoreResult FBlueprintHelperReviewAssetFactoryRestoreAdapter::RestoreBeforeSnapshot(
	const FBlueprintHelperReviewVisibleChange& Change) const
{
	FBlueprintHelperReviewRestoreResult Result;
	Result.RollbackMode = TEXT("asset_lifecycle_delete");
	if (Change.AtomicTargets.Num() != 1)
	{
		Result.Message = TEXT("asset_factory_restore_requires_single_atomic_target");
		return Result;
	}

	const FBlueprintHelperReviewAtomicTarget& Target = Change.AtomicTargets[0];
	const FString ObjectPath = FBlueprintHelperReviewSnapshotRestoreService::MakeObjectPathFromAssetPath(
		Target.AssetPath.IsEmpty() ? Change.AssetPath : Target.AssetPath);
	if (ObjectPath.IsEmpty())
	{
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
		Result.Message = TEXT("asset_factory_missing_asset_path");
		return Result;
	}

	UObject* AssetObject = FindObject<UObject>(nullptr, *ObjectPath);
	if (!AssetObject)
	{
		AssetObject = LoadObject<UObject>(nullptr, *ObjectPath);
	}
	if (!AssetObject)
	{
		Result.bSucceeded = true;
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
		Result.Message = FString::Printf(TEXT("asset_already_missing:%s"), *ObjectPath);
		Result.bSupersededDataCompactionEligible = true;
		return Result;
	}

	TArray<UObject*> ObjectsToDelete;
	ObjectsToDelete.Add(AssetObject);
	const int32 DeletedCount = ObjectTools::ForceDeleteObjects(ObjectsToDelete, false);
	if (DeletedCount != ObjectsToDelete.Num())
	{
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::RejectFailed;
		Result.Message = FString::Printf(TEXT("asset_delete_failed:%s"), *ObjectPath);
		return Result;
	}

	Result.bSucceeded = true;
	Result.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
	Result.Message = FString::Printf(TEXT("asset_deleted:%s"), *ObjectPath);
	Result.bSupersededDataCompactionEligible = true;
	return Result;
}
