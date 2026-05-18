// BlueprintHelper Review action service implementation.

#include "Systems/Review/BlueprintHelperReviewActionService.h"
#include "Systems/Review/BlueprintHelperReviewBaselineSnapshotService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "DataTableEditorUtils.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphUtilities.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Components/ActorComponent.h"
#include "Systems/Debug/BlueprintHelperDebugCaseStoreService.h"
#include "Systems/Debug/BlueprintHelperDebugEntryService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/ToolClusters/ObjectProperty/BlueprintHelperPropertyReflectionService.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/StructureEditorUtils.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "EdGraphSchema_K2.h"
#include "Misc/PackageName.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Systems/ToolClusters/CleanupOwnership/BlueprintHelperConvertBlockToUserOwnedService.h"
#include "Systems/Review/Utils/BlueprintHelperReviewActionRecordUtils.h"
#include "Systems/Review/Utils/BlueprintHelperReviewActionTargetUtils.h"
#include "Systems/Review/Utils/BlueprintHelperReviewGraphRollbackService.h"
#include "Systems/Review/Utils/BlueprintHelperReviewRejectService.h"
#include "Systems/Review/Utils/BlueprintHelperReviewSnapshotRestoreService.h"
#include "Systems/Transactions/BlueprintHelperTransactionJournalService.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Shared/Review/BlueprintHelperReviewStatusUtils.h"
#include "Shared/Review/BlueprintHelperReviewTargetKindRegistry.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "WidgetBlueprint.h"

FBlueprintHelperReviewActionService::FBlueprintHelperReviewActionService() = default;

FBlueprintHelperReviewActionService::FBlueprintHelperReviewActionService(
	const FBlueprintHelperDebugEntryService* InDebugEntryService)
	: DebugEntryService(InDebugEntryService)
{
}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewActionService::AcceptVisibleChange(
	const FBlueprintHelperReviewVisibleChange& Change) const
{
	const TArray<FBlueprintHelperReviewActionTargetUtils::FPersistedReviewTargetMatch> Matches =
		FBlueprintHelperReviewActionTargetUtils::ResolvePersistedReviewTargetMatches(Change);
	if (Matches.Num() > 0)
	{
		FBlueprintHelperReviewActionResult LastResult;
		for (const FBlueprintHelperReviewActionTargetUtils::FPersistedReviewTargetMatch& Match : Matches)
		{
			LastResult = AcceptReviewTargets(Match.ReviewRecordId, Match.TargetKeys);
			if (!LastResult.bSucceeded)
			{
				return LastResult;
			}
		}

		LastResult.bSucceeded = true;
		LastResult.TargetTransactionId = Change.LatestTransactionId;
		LastResult.NewStatus = EBlueprintHelperReviewChangeStatus::Accepted;
		LastResult.Message = TEXT("accepted");
		LastResult.bSupersededDataCompactionEligible =
			Change.SourceTransactionIds.Num() > FMath::Max(1, Change.LatestTransactionIds.Num());
		return LastResult;
	}

	FBlueprintHelperReviewActionResult Result;
	Result.bSucceeded = true;
	Result.TargetTransactionId = Change.LatestTransactionId;
	Result.NewStatus = EBlueprintHelperReviewChangeStatus::Accepted;
	Result.Message = TEXT("accepted");
	Result.bSupersededDataCompactionEligible =
		Change.SourceTransactionIds.Num() > FMath::Max(1, Change.LatestTransactionIds.Num());
	return Result;
}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewActionService::RejectVisibleChange(
	const FBlueprintHelperReviewVisibleChange& Change) const
{
	const TArray<FBlueprintHelperReviewActionTargetUtils::FPersistedReviewTargetMatch> Matches =
		FBlueprintHelperReviewActionTargetUtils::ResolvePersistedReviewTargetMatches(Change);
	if (Matches.Num() > 0)
	{
		FBlueprintHelperReviewRejectOptions Options;
		FBlueprintHelperReviewActionResult LastResult;
		for (const FBlueprintHelperReviewActionTargetUtils::FPersistedReviewTargetMatch& Match : Matches)
		{
			LastResult = RejectReviewTargets(Match.ReviewRecordId, Match.TargetKeys, Options);
			if (!LastResult.bSucceeded)
			{
				return LastResult;
			}
		}

		LastResult.bSucceeded = true;
		LastResult.TargetTransactionId = Change.LatestTransactionId;
		LastResult.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
		LastResult.Message = TEXT("rejected");
		LastResult.bSupersededDataCompactionEligible = true;
		return LastResult;
	}

	FBlueprintHelperReviewActionResult Result;
	Result.bSucceeded = false;
	Result.TargetTransactionId = Change.LatestTransactionId;
	Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
	Result.RollbackMode = TEXT("archive_baseline");
	Result.Message = TEXT("Archive-baseline rollback backend is not wired in the first Review UI slice.");
	return Result;
}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewActionService::RejectVisibleChange(
	const FBlueprintHelperReviewVisibleChange& Change,
	const FBlueprintHelperReviewRejectOptions& Options) const
{
	if (!FBlueprintHelperReviewActionRecordUtils::HasInjectedRejectOptions(Options))
	{
		const TArray<FBlueprintHelperReviewActionTargetUtils::FPersistedReviewTargetMatch> Matches =
			FBlueprintHelperReviewActionTargetUtils::ResolvePersistedReviewTargetMatches(Change);
		if (Matches.Num() > 0)
		{
			FBlueprintHelperReviewActionResult LastResult;
			for (const FBlueprintHelperReviewActionTargetUtils::FPersistedReviewTargetMatch& Match : Matches)
			{
				LastResult = RejectReviewTargets(Match.ReviewRecordId, Match.TargetKeys, Options);
				if (!LastResult.bSucceeded)
				{
					return LastResult;
				}
			}

			LastResult.bSucceeded = true;
			LastResult.TargetTransactionId = Change.LatestTransactionId;
			LastResult.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
			LastResult.Message = TEXT("rejected");
			LastResult.bSupersededDataCompactionEligible = true;
			return LastResult;
		}

		return FBlueprintHelperReviewRejectService::RejectVisibleChangeWithDefaultDispatcher(
			Change,
			&Options);
	}

	FBlueprintHelperReviewActionResult Result;
	Result.TargetTransactionId = Change.LatestTransactionId;
	Result.RollbackMode = TEXT("archive_baseline");

	if (Change.AtomicTargets.Num() == 0)
	{
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
		Result.Message = TEXT("missing_atomic_targets");
		return Result;
	}

	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		if (Target.TargetKey.IsEmpty())
		{
			Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
			Result.Message = TEXT("missing_anchor");
			return Result;
		}
		if (Target.RecordedAfterHash.IsEmpty())
		{
			Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
			Result.Message = TEXT("missing_recorded_after_hash");
			return Result;
		}
		if (FBlueprintHelperReviewTargetKindRegistry::SupportsSnapshotRestore(Target.TargetKind)
			&& Target.BeforeSnapshotJson.IsEmpty())
		{
			Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
			Result.Message = TEXT("missing_recoverable_snapshot");
			return Result;
		}

		const FString* CurrentHash = Options.CurrentHashesByTargetKey.Find(Target.TargetKey);
		if (!CurrentHash)
		{
			Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
			Result.Message = FString::Printf(TEXT("missing_current_hash:%s"), *Target.TargetKey);
			return Result;
		}
		if (!CurrentHash->Equals(Target.RecordedAfterHash, ESearchCase::CaseSensitive))
		{
			Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
			Result.Message = FString::Printf(TEXT("current_state_changed:%s"), *Target.TargetKey);
			Result.HashGuardTargetKey = Target.TargetKey;
			Result.HashGuardExpectedHash = Target.RecordedAfterHash;
			Result.HashGuardCurrentHash = *CurrentHash;
			Result.HashGuardRecordedAfterSnapshotJson = Target.AfterSnapshotJson;
			return Result;
		}
		if (FBlueprintHelperReviewSnapshotRestoreService::ShouldUseSnapshotRestore(Target))
		{
			FString SnapshotRestoreError;
			if (!FBlueprintHelperReviewSnapshotRestoreService::ExecuteSnapshotRestore(Target, SnapshotRestoreError))
			{
				Result.NewStatus = SnapshotRestoreError.Contains(TEXT("_recreate_required"))
					? EBlueprintHelperReviewChangeStatus::NeedsAction
					: EBlueprintHelperReviewChangeStatus::RejectFailed;
				Result.Message = SnapshotRestoreError;
				return Result;
			}
			continue;
		}
		if (Target.RollbackDataRef.IsEmpty())
		{
			Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
			Result.Message = TEXT("missing_rollback_data_ref");
			return Result;
		}
	}

	if (!Options.bRollbackExecutorAvailable)
	{
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
		Result.Message = TEXT("rollback_executor_unavailable");
		return Result;
	}

	if (!Options.bRollbackSucceeded)
	{
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::RejectFailed;
		Result.Message = Options.RollbackFailureMessage.IsEmpty()
			? TEXT("rollback_failed")
			: Options.RollbackFailureMessage;
		return Result;
	}

	Result.bSucceeded = true;
	Result.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
	Result.Message = TEXT("rejected");
	Result.bSupersededDataCompactionEligible = true;
	return Result;
}

FBlueprintHelperReviewCascadeActionResult FBlueprintHelperReviewActionService::RejectLifecycleRootVisibleChange(
	const FBlueprintHelperReviewVisibleChange& Root,
	const TArray<FBlueprintHelperReviewVisibleChange>& PendingChanges) const
{
	FBlueprintHelperReviewActionResult RootResult;
	FString ReviewRecordId;
	TArray<FString> TargetKeys;
	if (FBlueprintHelperReviewActionTargetUtils::TryResolvePersistedReviewChange(Root, ReviewRecordId, TargetKeys))
	{
		FBlueprintHelperReviewRejectOptions Options;
		RootResult = RejectReviewTargets(ReviewRecordId, TargetKeys, Options);
	}
	else
	{
		RootResult = RejectVisibleChange(Root);
	}

	return FBlueprintHelperReviewRejectService::CascadeRejectLifecycleChildrenAfterRootResult(
		Root,
		PendingChanges,
		RootResult,
		ReviewRecordId);
}

FBlueprintHelperReviewCascadeActionResult FBlueprintHelperReviewActionService::RejectLifecycleRootVisibleChange(
	const FBlueprintHelperReviewVisibleChange& Root,
	const TArray<FBlueprintHelperReviewVisibleChange>& PendingChanges,
	const FBlueprintHelperReviewRejectOptions& Options) const
{
	FBlueprintHelperReviewActionResult RootResult;
	FString ReviewRecordId;
	TArray<FString> TargetKeys;
	if (FBlueprintHelperReviewActionTargetUtils::TryResolvePersistedReviewChange(Root, ReviewRecordId, TargetKeys))
	{
		RootResult = RejectReviewTargets(ReviewRecordId, TargetKeys, Options);
	}
	else
	{
		RootResult = RejectVisibleChange(Root, Options);
	}

	return FBlueprintHelperReviewRejectService::CascadeRejectLifecycleChildrenAfterRootResult(
		Root,
		PendingChanges,
		RootResult,
		ReviewRecordId);
}

void FBlueprintHelperReviewActionService::RecordRejectDebugCaseBestEffort(
	FBlueprintHelperReviewRecord& Record,
	const TArray<FString>& TargetKeys,
	const FString& SourceTransactionId,
	EBlueprintHelperReviewChangeStatus RejectStatus,
	const FString& RejectMessage) const
{
	if (!DebugEntryService ||
		(RejectStatus != EBlueprintHelperReviewChangeStatus::NeedsAction &&
		 RejectStatus != EBlueprintHelperReviewChangeStatus::RejectFailed))
	{
		return;
	}

	TSharedRef<FJsonObject> ToolSummary = MakeShared<FJsonObject>();
	ToolSummary->SetStringField(TEXT("review_record_id"), Record.ReviewRecordId);
	ToolSummary->SetStringField(TEXT("archive_session_id"), Record.ArchiveSessionId);
	ToolSummary->SetStringField(TEXT("asset_path"), Record.AssetPath);
	ToolSummary->SetStringField(TEXT("review_status"), BlueprintHelperReviewChangeStatusToString(RejectStatus));
	if (!SourceTransactionId.IsEmpty())
	{
		ToolSummary->SetStringField(TEXT("source_transaction_id"), SourceTransactionId);
	}
	if (!RejectMessage.IsEmpty())
	{
		ToolSummary->SetStringField(TEXT("message"), RejectMessage);
	}
	TArray<TSharedPtr<FJsonValue>> TargetKeyValues;
	for (const FString& TargetKey : TargetKeys)
	{
		if (!TargetKey.IsEmpty())
		{
			TargetKeyValues.Add(MakeShared<FJsonValueString>(TargetKey));
		}
	}
	if (TargetKeyValues.Num() > 0)
	{
		ToolSummary->SetArrayField(TEXT("target_keys"), TargetKeyValues);
	}

	FBlueprintHelperDebugEntryEventInput DebugInput;
	DebugInput.SourceLayer = TEXT("review");
	DebugInput.Source = RejectStatus == EBlueprintHelperReviewChangeStatus::RejectFailed
		? TEXT("review_reject_failed")
		: TEXT("review_reject_needs_action");
	DebugInput.Operation = TEXT("reject_review_targets");
	DebugInput.Stage = TEXT("reject");
	DebugInput.Severity = EBlueprintHelperDebugSeverity::Error;
	if (Record.SourceTaskRunIds.Num() > 0)
	{
		DebugInput.TaskRunId = Record.SourceTaskRunIds[0];
	}
	if (!Record.AssetPath.IsEmpty())
	{
		DebugInput.AssetPaths.Add(Record.AssetPath);
	}
	if (!Record.ReviewRecordId.IsEmpty())
	{
		DebugInput.ReviewRecordIds.Add(Record.ReviewRecordId);
	}
	if (!SourceTransactionId.IsEmpty())
	{
		FBlueprintHelperDebugTransactionLink TransactionLink;
		TransactionLink.TransactionId = SourceTransactionId;
		TransactionLink.Role = TEXT("review_reject_failed");
		TransactionLink.Source = TEXT("review");
		TransactionLink.Summary = TEXT("source transaction for review reject action");
		DebugInput.TransactionLinks.Add(TransactionLink);
	}
	DebugInput.Error.Code = DebugInput.Source;
	DebugInput.Error.Message = RejectMessage;
	DebugInput.RecommendedNext = TEXT("get_debug_case");
	DebugInput.ToolResultSummary = ToolSummary;

	const FBlueprintHelperDebugEntryRecordResult DebugResult =
		DebugEntryService->RecordEventBestEffort(DebugInput);
	if (DebugResult.bRecorded && !DebugResult.DebugCaseId.IsEmpty())
	{
		Record.DebugCaseIds.AddUnique(DebugResult.DebugCaseId);
	}
}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewActionService::AcceptReviewTargets(
	const FString& ReviewRecordId,
	const TArray<FString>& TargetKeys) const
{
	FBlueprintHelperReviewActionResult Result;
	FBlueprintHelperReviewStoreService Store;
	FBlueprintHelperReviewRecord Record;
	FString Error;
	if (!Store.LoadReviewRecordById(ReviewRecordId, Record, Error))
	{
		Result.Message = Error;
		return Result;
	}

	bool bMatchedAny = false;
	FString SourceTransactionId;
	for (FBlueprintHelperReviewVisibleChange& Change : Record.VisibleChanges)
	{
		for (FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			if (!FBlueprintHelperReviewStatusUtils::ReviewTargetMatches(Target, TargetKeys))
			{
				continue;
			}
			bMatchedAny = true;
			Target.Status = EBlueprintHelperReviewChangeStatus::Accepted;
			SourceTransactionId = Target.LatestTransactionId;
		}
	}

	if (!bMatchedAny)
	{
		Result.Message = TEXT("target_keys_not_found");
		return Result;
	}

	FBlueprintHelperReviewStatusUtils::RefreshReviewRecordStatus(Record);
	Record.ReviewActions.Add(FBlueprintHelperReviewActionRecordUtils::MakeReviewActionRecord(
		TEXT("accept"),
		TargetKeys,
		TEXT("keep_managed"),
		SourceTransactionId,
		TEXT("accepted")));

	if (!Store.SaveReviewRecord(Record, Error))
	{
		Result.Message = Error;
		return Result;
	}

	Result.bSucceeded = true;
	Result.TargetTransactionId = SourceTransactionId;
	Result.NewStatus = Record.Status;
	Result.Message = TEXT("accepted");
	Result.bSupersededDataCompactionEligible = true;
	return Result;
}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewActionService::RejectReviewTargets(
	const FString& ReviewRecordId,
	const TArray<FString>& TargetKeys,
	const FBlueprintHelperReviewRejectOptions& Options) const
{
	FBlueprintHelperReviewActionResult Result;
	FBlueprintHelperReviewStoreService Store;
	FBlueprintHelperReviewRecord Record;
	FString Error;
	if (!Store.LoadReviewRecordById(ReviewRecordId, Record, Error))
	{
		Result.Message = Error;
		return Result;
	}

	bool bMatchedAny = false;
	bool bAllRejected = true;
	bool bAllTargetStatusesRejected = true;
	const bool bUseInjectedOptions = FBlueprintHelperReviewActionRecordUtils::HasInjectedRejectOptions(Options);
	FString SourceTransactionId;
	FString LastMessage;
	EBlueprintHelperReviewChangeStatus LastStatus = EBlueprintHelperReviewChangeStatus::Rejected;

	for (FBlueprintHelperReviewVisibleChange& Change : Record.VisibleChanges)
	{
		for (FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			if (!FBlueprintHelperReviewStatusUtils::ReviewTargetMatches(Target, TargetKeys))
			{
				continue;
			}

			bMatchedAny = true;
			FBlueprintHelperReviewVisibleChange TargetChange = Change;
			TargetChange.AtomicTargets.Reset();
			FBlueprintHelperReviewAtomicTarget TargetForReject = Target;
			if (TargetForReject.BeforeSnapshotJson.IsEmpty())
			{
				TargetForReject.BeforeSnapshotJson = Change.BeforeSnapshotJson;
			}
			if (TargetForReject.AfterSnapshotJson.IsEmpty())
			{
				TargetForReject.AfterSnapshotJson = Change.AfterSnapshotJson;
			}
			if (TargetForReject.BaselineHash.IsEmpty())
			{
				TargetForReject.BaselineHash = Change.BeforeHash;
			}
			if (TargetForReject.RecordedAfterHash.IsEmpty())
			{
				TargetForReject.RecordedAfterHash = Change.AfterHash;
			}
			TargetChange.AtomicTargets.Add(TargetForReject);
			const FBlueprintHelperReviewActionResult TargetResult = bUseInjectedOptions
				? RejectVisibleChange(TargetChange, Options)
				: FBlueprintHelperReviewRejectService::RejectVisibleChangeWithDefaultDispatcher(
					TargetChange,
					&Options);
			const bool bTargetStatusRejected = TargetResult.NewStatus == EBlueprintHelperReviewChangeStatus::Rejected;
			Target.Status = TargetResult.NewStatus;
			Change.NeedsActionReason = bTargetStatusRejected ? FString() : TargetResult.Message;
			SourceTransactionId = Target.LatestTransactionId;
			LastMessage = TargetResult.Message;
			LastStatus = TargetResult.NewStatus;
			bAllRejected &= TargetResult.bSucceeded;
			bAllTargetStatusesRejected &= bTargetStatusRejected;
		}
	}

	if (!bMatchedAny)
	{
		Result.Message = TEXT("target_keys_not_found");
		return Result;
	}

	if (bAllTargetStatusesRejected)
	{
		TArray<FString> DebugCaseIdsToDelete;
		bool bRecordDeleted = false;
		if (!Store.PurgeReviewTargets(ReviewRecordId, TargetKeys, DebugCaseIdsToDelete, bRecordDeleted, Error))
		{
			Result.Message = Error;
			return Result;
		}
		if (bRecordDeleted
			&& !FBlueprintHelperReviewActionRecordUtils::DeleteDebugCasesForReviewRecord(
				ReviewRecordId,
				DebugCaseIdsToDelete,
				Error))
		{
			Result.Message = Error;
			return Result;
		}

		Result.bSucceeded = true;
		Result.TargetTransactionId = SourceTransactionId;
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
		Result.RollbackMode = TEXT("archive_baseline");
		Result.Message = LastMessage.IsEmpty() ? TEXT("rejected_purged") : LastMessage;
		Result.bSupersededDataCompactionEligible = true;
		return Result;
	}

	FBlueprintHelperReviewStatusUtils::RefreshReviewRecordStatus(Record);
	Record.ReviewActions.Add(FBlueprintHelperReviewActionRecordUtils::MakeReviewActionRecord(
		TEXT("reject"),
		TargetKeys,
		TEXT("archive_baseline"),
		SourceTransactionId,
		LastMessage));
	RecordRejectDebugCaseBestEffort(
		Record,
		TargetKeys,
		SourceTransactionId,
		bAllTargetStatusesRejected ? EBlueprintHelperReviewChangeStatus::Rejected : LastStatus,
		LastMessage);

	if (!Store.SaveReviewRecord(Record, Error))
	{
		Result.Message = Error;
		return Result;
	}

	Result.bSucceeded = bAllTargetStatusesRejected;
	Result.TargetTransactionId = SourceTransactionId;
	Result.NewStatus = bAllTargetStatusesRejected ? EBlueprintHelperReviewChangeStatus::Rejected : LastStatus;
	Result.RollbackMode = TEXT("archive_baseline");
	Result.Message = LastMessage;
	Result.bSupersededDataCompactionEligible = bAllTargetStatusesRejected;
	return Result;
}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewActionService::RejectAll(
	const FBlueprintHelperReviewRecordQuery& Query,
	const FBlueprintHelperReviewRejectOptions& Options) const
{
	FBlueprintHelperReviewActionResult Result;
	FBlueprintHelperReviewStoreService Store;
	const TArray<FBlueprintHelperReviewRecord> Records = Store.QueryReviewRecords(Query);
	if (Records.Num() == 0)
	{
		Result.bSucceeded = true;
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
		Result.Message = TEXT("no_pending_review_targets");
		return Result;
	}

	bool bAllRejected = true;
	EBlueprintHelperReviewChangeStatus LastStatus = EBlueprintHelperReviewChangeStatus::Rejected;
	FString LastMessage;
	for (const FBlueprintHelperReviewRecord& Record : Records)
	{
		const TArray<FString> TargetKeys = FBlueprintHelperReviewActionTargetUtils::CollectPendingTargetKeys(Record);
		if (TargetKeys.Num() == 0)
		{
			continue;
		}

		const FBlueprintHelperReviewActionResult RecordResult = RejectReviewTargets(
			Record.ReviewRecordId,
			TargetKeys,
			Options);
		bAllRejected &= RecordResult.bSucceeded;
		LastStatus = RecordResult.NewStatus;
		LastMessage = RecordResult.Message;
	}

	Result.bSucceeded = bAllRejected;
	Result.NewStatus = bAllRejected ? EBlueprintHelperReviewChangeStatus::Rejected : LastStatus;
	Result.RollbackMode = TEXT("archive_baseline");
	Result.Message = LastMessage.IsEmpty() ? TEXT("reject_all") : LastMessage;
	Result.bSupersededDataCompactionEligible = bAllRejected;
	return Result;
}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewActionService::ConvertOwnerBlock(
	const FBlueprintHelperReviewConvertOwnerBlockRequest& Request) const
{
	FBlueprintHelperReviewActionResult Result;
	FBlueprintHelperReviewStoreService Store;
	FBlueprintHelperReviewRecord Record;
	FString Error;
	if (!Store.LoadReviewRecordById(Request.ReviewRecordId, Record, Error))
	{
		Result.Message = Error;
		return Result;
	}

	auto PersistFailure = [&Store, &Record, &Request, &Result](
		const FString& Message,
		const FString& SourceTransactionId) -> FBlueprintHelperReviewActionResult
	{
		TArray<FString> TargetKeys;
		if (!Request.BlockTargetKey.IsEmpty())
		{
			TargetKeys.Add(Request.BlockTargetKey);
		}
		Record.ReviewActions.Add(FBlueprintHelperReviewActionRecordUtils::MakeReviewActionRecord(
			TEXT("convert_owner_block"),
			TargetKeys,
			Request.Direction,
			SourceTransactionId,
			Message));

		FString SaveError;
		if (!Store.SaveReviewRecord(Record, SaveError))
		{
			Result.Message = SaveError;
			return Result;
		}

		Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
		Result.Message = Message;
		return Result;
	};

	if (!Request.bSettingProfileAllowsConversion)
	{
		return PersistFailure(TEXT("convert_owner_block_not_allowed_by_setting_profile"), FString());
	}
	if (Request.Direction != TEXT("bh_to_user") && Request.Direction != TEXT("user_to_bh"))
	{
		return PersistFailure(TEXT("invalid_convert_owner_block_direction"), FString());
	}
	if (Request.BlockTargetKey.IsEmpty() || Request.EntryAnchor.IsEmpty() || Request.DesiredBlockRef.IsEmpty())
	{
		return PersistFailure(TEXT("missing_convert_owner_block_anchor"), FString());
	}
	if (Request.NodeAnchors.Num() == 0)
	{
		return PersistFailure(TEXT("missing_convert_owner_block_node_anchors"), FString());
	}

	FBlueprintHelperReviewAtomicTarget MatchedTarget;
	if (!FBlueprintHelperReviewActionTargetUtils::TryFindReviewAtomicTarget(Record, Request.BlockTargetKey, MatchedTarget))
	{
		return PersistFailure(TEXT("convert_owner_block_target_not_found"), FString());
	}
	if (!FBlueprintHelperReviewTargetKindRegistry::IsGraphBlockTarget(MatchedTarget.TargetKind, MatchedTarget.TargetKey))
	{
		return PersistFailure(TEXT("convert_owner_block_requires_graph_block_target"), FString());
	}
	if (MatchedTarget.AssetPath.IsEmpty())
	{
		MatchedTarget.AssetPath = Record.AssetPath;
	}

	FBlueprintHelperTransactionJournalService JournalService;
	const FString ConversionTransactionId = FBlueprintHelperReviewGraphRollbackService::ResolveConversionTransactionId(Request, JournalService);
	FString ConversionError;
	const bool bConverted = Request.Direction == TEXT("bh_to_user")
		? FBlueprintHelperReviewGraphRollbackService::ExecuteBhToUserOwnerBlockConversion(
			MatchedTarget,
			Request,
			ConversionTransactionId,
			ConversionError)
		: FBlueprintHelperReviewGraphRollbackService::ExecuteUserToBhOwnerBlockConversion(
			MatchedTarget,
			Request,
			ConversionTransactionId,
			ConversionError);
	if (!bConverted)
	{
		const FString FailureMessage = ConversionError.IsEmpty()
			? TEXT("convert_owner_block_failed")
			: ConversionError;
		return PersistFailure(FailureMessage, ConversionTransactionId);
	}

	bool bMatchedAny = false;
	const FString NewOwnership = Request.Direction == TEXT("bh_to_user")
		? TEXT("user_owned")
		: TEXT("blueprinthelper_owned");
	for (FBlueprintHelperReviewVisibleChange& Change : Record.VisibleChanges)
	{
		for (FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			if (Target.TargetKey == Request.BlockTargetKey)
			{
				Target.Ownership = NewOwnership;
				bMatchedAny = true;
			}
		}
	}

	TArray<FString> ConvertedTargetKeys;
	ConvertedTargetKeys.Add(Request.BlockTargetKey);
	Record.ReviewActions.Add(FBlueprintHelperReviewActionRecordUtils::MakeReviewActionRecord(
		TEXT("convert_owner_block"),
		ConvertedTargetKeys,
		Request.Direction,
		ConversionTransactionId,
		TEXT("converted_owner_block")));
	Record.SourceTransactionSummary.TransactionIds.AddUnique(ConversionTransactionId);
	Record.SourceTransactionSummary.OperationKinds.AddUnique(TEXT("convert_owner_block"));
	if (!Record.AssetPath.IsEmpty())
	{
		Record.SourceTransactionSummary.AssetPaths.AddUnique(Record.AssetPath);
	}
	Record.SourceTransactionSummary.TransactionCount = Record.SourceTransactionSummary.TransactionIds.Num();

	if (!Store.SaveReviewRecord(Record, Error))
	{
		Result.Message = Error;
		return Result;
	}

	Result.bSucceeded = true;
	Result.TargetTransactionId = ConversionTransactionId;
	Result.NewStatus = Record.Status;
	Result.Message = TEXT("converted_owner_block");
	return Result;
}
