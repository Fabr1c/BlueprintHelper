// BlueprintHelper Review snapshot restore adapter implementation.

#include "Systems/Review/BlueprintHelperReviewSnapshotRestoreAdapter.h"
#include "Systems/Review/Utils/BlueprintHelperReviewSnapshotRestoreService.h"

FBlueprintHelperReviewSnapshotRestoreAdapter::FBlueprintHelperReviewSnapshotRestoreAdapter(const FString& InTargetKind)
	: TargetKind(InTargetKind)
{
}

FString FBlueprintHelperReviewSnapshotRestoreAdapter::GetTargetKind() const
{
	return TargetKind;
}

FBlueprintHelperReviewRestoreResult FBlueprintHelperReviewSnapshotRestoreAdapter::RestoreBeforeSnapshot(
	const FBlueprintHelperReviewVisibleChange& Change) const
{
	FBlueprintHelperReviewRestoreResult Result;
	Result.RollbackMode = TEXT("archive_baseline");
	if (Change.AtomicTargets.Num() != 1)
	{
		Result.Message = TEXT("restore_adapter_requires_single_atomic_target");
		return Result;
	}

	const FBlueprintHelperReviewAtomicTarget& Target = Change.AtomicTargets[0];
	if (Target.TargetKey.IsEmpty())
	{
		Result.Message = TEXT("missing_anchor");
		return Result;
	}
	if (Target.BeforeSnapshotJson.IsEmpty())
	{
		Result.Message = TEXT("missing_recoverable_snapshot");
		return Result;
	}
	if (!FBlueprintHelperReviewSnapshotRestoreService::ShouldUseSnapshotRestore(Target))
	{
		Result.Message = FString::Printf(TEXT("snapshot_restore_unsupported_target_kind:%s"), *Target.TargetKind);
		return Result;
	}

	FString SnapshotRestoreError;
	if (!FBlueprintHelperReviewSnapshotRestoreService::ExecuteSnapshotRestore(Target, SnapshotRestoreError))
	{
		Result.NewStatus = SnapshotRestoreError.Contains(TEXT("_recreate_required"))
			? EBlueprintHelperReviewChangeStatus::NeedsAction
			: EBlueprintHelperReviewChangeStatus::RejectFailed;
		Result.Message = SnapshotRestoreError;
		return Result;
	}

	Result.bSucceeded = true;
	Result.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
	Result.Message = TEXT("rejected");
	Result.bSupersededDataCompactionEligible = true;
	return Result;
}

TArray<FString> FBlueprintHelperReviewSnapshotRestoreAdapter::GetSupportedTargetKinds()
{
	return {
		TEXT("graph_block"),
		TEXT("graph_external_boundary"),
		TEXT("graph_external_link"),
		TEXT("graph_external_node"),
		TEXT("graph_external_body"),
		TEXT("graph_node"),
		TEXT("graph_pin"),
		TEXT("graph_link"),
		TEXT("material_expression"),
		TEXT("material_expression_link"),
		TEXT("material_output_link"),
		TEXT("component"),
		TEXT("blueprint_variable"),
		TEXT("variable_default"),
		TEXT("signature"),
		TEXT("dispatcher"),
		TEXT("delegate"),
		TEXT("function"),
		TEXT("macro"),
		TEXT("class_setting"),
		TEXT("class_setting_interface"),
		TEXT("class_default_property"),
		TEXT("umg_widget_tree"),
		TEXT("umg_widget"),
		TEXT("umg_widget_property"),
		TEXT("slot_property"),
		TEXT("widget_variable"),
		TEXT("datatable_row"),
		TEXT("struct_field"),
		TEXT("structure_field"),
		TEXT("data_asset_property"),
		TEXT("object_property")
	};
}
