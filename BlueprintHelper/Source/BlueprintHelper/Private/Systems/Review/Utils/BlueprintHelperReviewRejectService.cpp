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
#include "Systems/Review/BlueprintHelperReviewHashService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Systems/ToolClusters/CleanupOwnership/BlueprintHelperConvertBlockToUserOwnedService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/ToolClusters/ObjectProperty/BlueprintHelperPropertyReflectionService.h"
#include "Systems/Transactions/BlueprintHelperTransactionJournalService.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "WidgetBlueprint.h"
#include "Systems/Review/Utils/BlueprintHelperReviewActionRecordUtils.h"
#include "Systems/Review/Utils/BlueprintHelperReviewActionTargetUtils.h"
#include "Systems/Review/Utils/BlueprintHelperReviewGraphRollbackService.h"
#include "Systems/Review/Utils/BlueprintHelperReviewSnapshotRestoreService.h"

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
			if (Target.RollbackDataRef.IsEmpty())
			{
				return FBlueprintHelperReviewActionRecordUtils::MakeRejectFailureResult(Change, EBlueprintHelperReviewChangeStatus::NeedsAction, TEXT("missing_rollback_data_ref"));
			}
			if (Target.RecordedAfterHash.IsEmpty())
			{
				return FBlueprintHelperReviewActionRecordUtils::MakeRejectFailureResult(Change, EBlueprintHelperReviewChangeStatus::NeedsAction, TEXT("missing_recorded_after_hash"));
			}

			FString CurrentHash;
			FString HashError;
			if (!FBlueprintHelperReviewHashService::ComputeAtomicTargetHash(Target, CurrentHash, HashError))
			{
				if (HashError.Contains(TEXT("graph_not_found"))
					|| HashError.Contains(TEXT("anchor_not_found"))
					|| HashError.Contains(TEXT("node_not_found")))
				{
					FBlueprintHelperReviewActionResult Result;
					Result.bSucceeded = true;
					Result.TargetTransactionId = Change.LatestTransactionId;
					Result.RollbackMode = TEXT("archive_baseline");
					Result.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
					Result.Message = FString::Printf(TEXT("target_already_missing:%s"), *HashError);
					Result.bSupersededDataCompactionEligible = true;
					return Result;
				}
				return FBlueprintHelperReviewActionRecordUtils::MakeRejectFailureResult(
					Change,
					EBlueprintHelperReviewChangeStatus::NeedsAction,
					FString::Printf(TEXT("current_hash_unavailable:%s"), *HashError));
			}
			if (!CurrentHash.Equals(Target.RecordedAfterHash, ESearchCase::CaseSensitive))
			{
				return FBlueprintHelperReviewActionRecordUtils::MakeRejectFailureResult(
					Change,
					EBlueprintHelperReviewChangeStatus::NeedsAction,
					FString::Printf(TEXT("current_state_changed:%s"), *Target.TargetKey));
			}
			FString RollbackTransactionId;
			if (!FBlueprintHelperReviewGraphRollbackService::ExtractRollbackTransactionId(Target.RollbackDataRef, RollbackTransactionId))
			{
				return FBlueprintHelperReviewActionRecordUtils::MakeRejectFailureResult(
					Change,
					EBlueprintHelperReviewChangeStatus::NeedsAction,
					FString::Printf(TEXT("rollback_ref_unresolved:%s"), *Target.RollbackDataRef));
			}

			TSharedPtr<FJsonObject> JournalRecord;
			FString JournalError;
			if (Options)
			{
				if (const FBlueprintHelperReviewPreparedRollbackJournal* Prepared =
					Options->PreparedRollbackJournalsByTransactionId.Find(RollbackTransactionId))
				{
					if (!Prepared->Error.IsEmpty())
					{
						return FBlueprintHelperReviewActionRecordUtils::MakeRejectFailureResult(Change, EBlueprintHelperReviewChangeStatus::NeedsAction, Prepared->Error);
					}
					JournalRecord = FBlueprintHelperReviewActionRecordUtils::BuildJournalRecordFromPreparedRollbackJournal(*Prepared);
				}
			}
			if (!JournalRecord.IsValid()
				&& !FBlueprintHelperReviewGraphRollbackService::LoadJournalRecordForReviewRollback(RollbackTransactionId, JournalRecord, JournalError))
			{
				return FBlueprintHelperReviewActionRecordUtils::MakeRejectFailureResult(Change, EBlueprintHelperReviewChangeStatus::NeedsAction, JournalError);
			}

			FString RollbackError;
			if (!FBlueprintHelperReviewGraphRollbackService::ExecuteGraphAppendRollback(Target, JournalRecord, RollbackError))
			{
				return FBlueprintHelperReviewActionRecordUtils::MakeRejectFailureResult(Change, EBlueprintHelperReviewChangeStatus::RejectFailed, RollbackError);
			}
		}

		FBlueprintHelperReviewActionResult Result;
		Result.bSucceeded = true;
		Result.TargetTransactionId = Change.LatestTransactionId;
		Result.RollbackMode = TEXT("archive_baseline");
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
		Result.Message = TEXT("rejected");
		Result.bSupersededDataCompactionEligible = true;
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
		for (const FBlueprintHelperReviewVisibleChange& PendingChange : PendingChanges)
		{
			if (PendingChange.AssetPath == Root.AssetPath
				&& !PendingChange.bIsAssetLifecycleRoot
				&& PendingChange.Status == EBlueprintHelperReviewChangeStatus::Pending
				&& !PendingChange.ChangeId.IsEmpty())
			{
				ChildChangeIds.Add(PendingChange.ChangeId);
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

		for (const FString& ChildChangeId : ChildChangeIds)
		{
			CascadeResult.RemovedChildChangeIds.Add(ChildChangeId);
		}
		CascadeResult.bChildrenRemoved = CascadeResult.RemovedChildChangeIds.Num() > 0;
		return CascadeResult;
	}
