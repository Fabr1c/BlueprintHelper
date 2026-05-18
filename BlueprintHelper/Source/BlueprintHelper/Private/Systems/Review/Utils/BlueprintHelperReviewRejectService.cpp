// BlueprintHelper Review BlueprintHelperReviewRejectService implementation.

#include "Systems/Review/Utils/BlueprintHelperReviewRejectService.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ActorComponent.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "DataTableEditorUtils.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphUtilities.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "HAL/FileManager.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/StructureEditorUtils.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Shared/Review/BlueprintHelperReviewStatusUtils.h"
#include "Shared/Review/BlueprintHelperReviewTargetKindRegistry.h"
#include "Systems/Debug/BlueprintHelperDebugCaseStoreService.h"
#include "Systems/Debug/BlueprintHelperDebugEntryService.h"
#include "Systems/Review/BlueprintHelperReviewBaselineSnapshotService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/ToolClusters/ObjectProperty/BlueprintHelperPropertyReflectionService.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "WidgetBlueprint.h"
#include "Systems/Review/Utils/BlueprintHelperReviewActionRecordUtils.h"
#include "Systems/Review/Utils/BlueprintHelperReviewActionTargetUtils.h"
#include "Systems/Review/Utils/BlueprintHelperReviewSnapshotRestoreService.h"

namespace
{
	static void CaptureReviewRejectCurrentStateDiagnostic(
		const FBlueprintHelperReviewAtomicTarget& Target,
		FBlueprintHelperReviewActionResult& InOutDiagnostic)
	{
		if (!InOutDiagnostic.HashGuardTargetKey.IsEmpty())
		{
			return;
		}

		if (Target.RecordedAfterHash.IsEmpty())
		{
			InOutDiagnostic.HashGuardTargetKey = Target.TargetKey;
			InOutDiagnostic.HashGuardExpectedHash = TEXT("<missing_recorded_after_hash>");
			return;
		}

		FBlueprintHelperReviewBaselineSnapshotService SnapshotService;
		FString CurrentSnapshotJson;
		FString CurrentHash;
		FString SnapshotError;
		if (!SnapshotService.CaptureTargetSnapshot(Target, CurrentSnapshotJson, CurrentHash, SnapshotError))
		{
			InOutDiagnostic.HashGuardTargetKey = Target.TargetKey;
			InOutDiagnostic.HashGuardExpectedHash = Target.RecordedAfterHash;
			InOutDiagnostic.HashGuardCurrentHash = FString::Printf(TEXT("<current_hash_unavailable:%s>"), *SnapshotError);
			InOutDiagnostic.HashGuardRecordedAfterSnapshotJson = Target.AfterSnapshotJson;
			return;
		}

		if (!CurrentHash.Equals(Target.RecordedAfterHash, ESearchCase::CaseSensitive))
		{
			InOutDiagnostic.HashGuardTargetKey = Target.TargetKey;
			InOutDiagnostic.HashGuardExpectedHash = Target.RecordedAfterHash;
			InOutDiagnostic.HashGuardCurrentHash = CurrentHash;
			InOutDiagnostic.HashGuardCurrentSnapshotJson = CurrentSnapshotJson;
			InOutDiagnostic.HashGuardRecordedAfterSnapshotJson = Target.AfterSnapshotJson;
		}
	}
}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewRejectService::RejectVisibleChangeWithDefaultDispatcher(
		const FBlueprintHelperReviewVisibleChange& Change,
		const FBlueprintHelperReviewRejectOptions* Options)
	{
		if (Change.AtomicTargets.Num() == 0)
		{
			return FBlueprintHelperReviewActionRecordUtils::MakeRejectFailureResult(
				Change,
				EBlueprintHelperReviewChangeStatus::NeedsAction,
				TEXT("missing_atomic_targets"));
		}

		FBlueprintHelperReviewActionResult CurrentStateDiagnostic;
		for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			if (FBlueprintHelperReviewSnapshotRestoreService::IsAssetFactoryTarget(Target))
			{
				const FBlueprintHelperReviewActionResult AssetFactoryResult =
					FBlueprintHelperReviewSnapshotRestoreService::RejectAssetFactoryTargetWithDefaultDispatcher(Change, Target);
				if (!AssetFactoryResult.bSucceeded)
				{
					return AssetFactoryResult;
				}
				continue;
			}

			if (Target.TargetKey.IsEmpty())
			{
				return FBlueprintHelperReviewActionRecordUtils::MakeRejectFailureResult(Change, EBlueprintHelperReviewChangeStatus::NeedsAction, TEXT("missing_anchor"));
			}
			if (FBlueprintHelperReviewTargetKindRegistry::SupportsSnapshotRestore(Target.TargetKind)
				&& Target.BeforeSnapshotJson.IsEmpty())
			{
				return FBlueprintHelperReviewActionRecordUtils::MakeRejectFailureResult(
					Change,
					EBlueprintHelperReviewChangeStatus::NeedsAction,
					TEXT("missing_recoverable_snapshot"));
			}
			CaptureReviewRejectCurrentStateDiagnostic(Target, CurrentStateDiagnostic);
			if (FBlueprintHelperReviewSnapshotRestoreService::ShouldUseSnapshotRestore(Target))
			{
				FString SnapshotRestoreError;
				if (!FBlueprintHelperReviewSnapshotRestoreService::ExecuteSnapshotRestore(Target, SnapshotRestoreError))
				{
					return FBlueprintHelperReviewActionRecordUtils::MakeRejectFailureResult(
						Change,
						SnapshotRestoreError.Contains(TEXT("_recreate_required"))
							? EBlueprintHelperReviewChangeStatus::NeedsAction
							: EBlueprintHelperReviewChangeStatus::RejectFailed,
						SnapshotRestoreError);
				}
				continue;
			}
			return FBlueprintHelperReviewActionRecordUtils::MakeRejectFailureResult(
				Change,
				EBlueprintHelperReviewChangeStatus::NeedsAction,
				FString::Printf(TEXT("snapshot_restore_unsupported_target_kind:%s"), *Target.TargetKind));
		}

		FBlueprintHelperReviewActionResult Result;
		Result.bSucceeded = true;
		Result.TargetEvidenceId = Change.LatestEvidenceId;
		Result.RollbackMode = TEXT("archive_baseline");
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
		Result.Message = TEXT("rejected");
		Result.bSupersededDataCompactionEligible = true;
		Result.HashGuardTargetKey = CurrentStateDiagnostic.HashGuardTargetKey;
		Result.HashGuardExpectedHash = CurrentStateDiagnostic.HashGuardExpectedHash;
		Result.HashGuardCurrentHash = CurrentStateDiagnostic.HashGuardCurrentHash;
		Result.HashGuardCurrentSnapshotJson = CurrentStateDiagnostic.HashGuardCurrentSnapshotJson;
		Result.HashGuardRecordedAfterSnapshotJson = CurrentStateDiagnostic.HashGuardRecordedAfterSnapshotJson;
		return Result;
	}
FBlueprintHelperReviewCascadeActionResult FBlueprintHelperReviewRejectService::CascadeRejectLifecycleChildrenAfterRootResult(
		const FBlueprintHelperReviewVisibleChange& Root,
		const TArray<FBlueprintHelperReviewVisibleChange>& PendingChanges,
		const FBlueprintHelperReviewActionResult& RootResult,
		const FString& ResolvedReviewRecordId)
	{
		FBlueprintHelperReviewCascadeActionResult CascadeResult;
		CascadeResult.RootResult = RootResult;
		if (!RootResult.bSucceeded || !Root.bRejectRemovesChildren || Root.ChangeId.IsEmpty())
		{
			return CascadeResult;
		}

		TSet<FString> ChildChangeIds;
		TSet<FString> ChildReviewRecordIdsToDelete;
		const bool bRootIsAssetFactory = Root.AtomicTargets.ContainsByPredicate(
			[](const FBlueprintHelperReviewAtomicTarget& Target)
			{
				return FBlueprintHelperReviewTargetKindRegistry::IsAssetFactoryTargetKind(Target.TargetKind);
			});
		for (const FBlueprintHelperReviewVisibleChange& PendingChange : PendingChanges)
		{
			const bool bActionable =
				PendingChange.Status == EBlueprintHelperReviewChangeStatus::Pending
				|| PendingChange.Status == EBlueprintHelperReviewChangeStatus::NeedsAction;
			const bool bLinkedChild =
				!bRootIsAssetFactory
				&& PendingChange.ParentChangeId == Root.ChangeId;
			const bool bAssetLifecycleChild =
				bRootIsAssetFactory
				&& PendingChange.AssetPath == Root.AssetPath
				&& PendingChange.ChangeId != Root.ChangeId;
			if (!bActionable
				|| PendingChange.ChangeId.IsEmpty()
				|| (!bLinkedChild && !bAssetLifecycleChild))
			{
				continue;
			}

			ChildChangeIds.Add(PendingChange.ChangeId);
			FString ChildReviewRecordId;
			TArray<FString> ChildTargetKeys;
			if (FBlueprintHelperReviewActionTargetUtils::TryResolvePersistedReviewChange(
				PendingChange,
				ChildReviewRecordId,
				ChildTargetKeys)
				&& !ChildReviewRecordId.IsEmpty())
			{
				ChildReviewRecordIdsToDelete.Add(ChildReviewRecordId);
			}
		}

		FString ReviewRecordId = ResolvedReviewRecordId;
		TArray<FString> RootTargetKeys;
		if (ReviewRecordId.IsEmpty()
			&& !FBlueprintHelperReviewActionTargetUtils::TryResolvePersistedReviewChange(Root, ReviewRecordId, RootTargetKeys))
		{
			return CascadeResult;
		}

		FBlueprintHelperReviewStoreService Store;
		FString Error;
		if (!FBlueprintHelperReviewActionRecordUtils::DeleteReviewRecordAndLinkedDebugCases(Store, ReviewRecordId, Error))
		{
			CascadeResult.RootResult.bSucceeded = false;
			CascadeResult.RootResult.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
			CascadeResult.RootResult.Message = Error;
			return CascadeResult;
		}

		ChildReviewRecordIdsToDelete.Remove(ReviewRecordId);
		for (const FString& ChildReviewRecordId : ChildReviewRecordIdsToDelete)
		{
			if (!FBlueprintHelperReviewActionRecordUtils::DeleteReviewRecordAndLinkedDebugCases(Store, ChildReviewRecordId, Error))
			{
				CascadeResult.RootResult.bSucceeded = false;
				CascadeResult.RootResult.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
				CascadeResult.RootResult.Message = Error;
				return CascadeResult;
			}
		}

		for (const FString& ChildChangeId : ChildChangeIds)
		{
			CascadeResult.RemovedChildChangeIds.Add(ChildChangeId);
		}
		CascadeResult.bChildrenRemoved = CascadeResult.RemovedChildChangeIds.Num() > 0;
		return CascadeResult;
	}
