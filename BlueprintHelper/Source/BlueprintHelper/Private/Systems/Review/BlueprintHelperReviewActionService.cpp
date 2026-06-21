// BlueprintHelper Review action service implementation.

#include "Systems/Review/BlueprintHelperReviewActionService.h"
#include "Systems/Review/BlueprintHelperReviewAdapterRegistry.h"
#include "Systems/Review/BlueprintHelperReviewBaselineSnapshotService.h"
#include "Systems/Review/BlueprintHelperReviewPerformanceTrace.h"
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
#include "Runtime/TaskRuntime/BlueprintHelperSequentialReviewSessionService.h"
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
#include "Systems/Review/Utils/BlueprintHelperReviewActionRecordUtils.h"
#include "Systems/Review/Utils/BlueprintHelperReviewActionTargetUtils.h"
#include "Systems/Review/Utils/BlueprintHelperReviewRejectService.h"
#include "Systems/Review/Utils/BlueprintHelperReviewSnapshotRestoreService.h"
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

	static bool BlueprintHelperReviewVisibleChangeHasAssetFactoryTarget(const FBlueprintHelperReviewVisibleChange& Change)
	{
		return Change.AtomicTargets.ContainsByPredicate(
			[](const FBlueprintHelperReviewAtomicTarget& Target)
			{
				return FBlueprintHelperReviewTargetKindRegistry::IsAssetFactoryTargetKind(Target.TargetKind);
			});
	}

	static FBlueprintHelperReviewActionResult BlueprintHelperReviewMakeCascadeChildFailure(
		const FBlueprintHelperReviewVisibleChange& Root,
		const FBlueprintHelperReviewVisibleChange& Child,
		const FString& Message)
	{
		FBlueprintHelperReviewActionResult Result;
		Result.bSucceeded = false;
		Result.TargetEvidenceId = Root.LatestEvidenceId;
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
		Result.Message = FString::Printf(
			TEXT("cascade_child_reject_failed:%s:%s"),
			*Child.ChangeId,
			*Message);
		return Result;
	}

FBlueprintHelperReviewActionService::FBlueprintHelperReviewActionService()
	: AdapterRegistry(FBlueprintHelperReviewAdapterRegistry::CreateDefault())
{
}

FBlueprintHelperReviewActionService::FBlueprintHelperReviewActionService(
	const FBlueprintHelperDebugEntryService* InDebugEntryService)
	: DebugEntryService(InDebugEntryService)
	, AdapterRegistry(FBlueprintHelperReviewAdapterRegistry::CreateDefault())
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
		LastResult.TargetEvidenceId = Change.LatestEvidenceId;
		LastResult.NewStatus = EBlueprintHelperReviewChangeStatus::Accepted;
		LastResult.Message = TEXT("accepted");
		LastResult.bSupersededDataCompactionEligible =
			Change.SourceEvidenceIds.Num() > FMath::Max(1, Change.LatestEvidenceIds.Num());
		return LastResult;
	}

	FBlueprintHelperReviewActionResult Result;
	Result.TargetEvidenceId = Change.LatestEvidenceId;
	Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
	Result.Message = TEXT("persisted_review_targets_not_found");
	Result.bSucceeded = true;
	Result.NewStatus = EBlueprintHelperReviewChangeStatus::Accepted;
	Result.Message = TEXT("accepted");
	Result.bSupersededDataCompactionEligible =
		Change.SourceEvidenceIds.Num() > FMath::Max(1, Change.LatestEvidenceIds.Num());
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
		LastResult.TargetEvidenceId = Change.LatestEvidenceId;
		LastResult.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
		LastResult.Message = TEXT("rejected");
		LastResult.bSupersededDataCompactionEligible = true;
		return LastResult;
	}

	FBlueprintHelperReviewActionResult Result;
	Result.bSucceeded = false;
	Result.TargetEvidenceId = Change.LatestEvidenceId;
	Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
	Result.Message = TEXT("persisted_review_targets_not_found");
	return Result;
}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewActionService::RejectVisibleChange(
	const FBlueprintHelperReviewVisibleChange& Change,
	const FBlueprintHelperReviewRejectOptions& Options) const
{
	FBlueprintHelperReviewPerformanceScope Scope(TEXT("ReviewReject.RejectVisibleChange"));
	Scope.AddText(TEXT("change_id"), Change.ChangeId);
	Scope.AddText(TEXT("asset"), Change.AssetPath);
	Scope.AddCount(TEXT("atomic_targets"), Change.AtomicTargets.Num());
	if (!FBlueprintHelperReviewActionRecordUtils::HasInjectedRejectOptions(Options))
	{
		const TArray<FBlueprintHelperReviewActionTargetUtils::FPersistedReviewTargetMatch> Matches =
			FBlueprintHelperReviewActionTargetUtils::ResolvePersistedReviewTargetMatches(Change);
		Scope.AddCount(TEXT("persisted_matches"), Matches.Num());
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
			LastResult.TargetEvidenceId = Change.LatestEvidenceId;
			LastResult.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
			LastResult.Message = TEXT("rejected");
			LastResult.bSupersededDataCompactionEligible = true;
			return LastResult;
		}

	}

	return RejectResolvedVisibleChange(Change, Options);
}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewActionService::RejectResolvedVisibleChange(
	const FBlueprintHelperReviewVisibleChange& Change,
	const FBlueprintHelperReviewRejectOptions& Options) const
{
	FBlueprintHelperReviewActionResult Result;
	Result.TargetEvidenceId = Change.LatestEvidenceId;
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
		if (FBlueprintHelperReviewTargetKindRegistry::SupportsSnapshotRestore(Target.TargetKind)
			&& Target.BeforeSnapshotJson.IsEmpty())
		{
			Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
			Result.Message = TEXT("missing_recoverable_snapshot");
			return Result;
		}

		const FString* CurrentHash = Options.CurrentHashesByTargetKey.Find(Target.TargetKey);
		if (CurrentHash
			&& !Target.RecordedAfterHash.IsEmpty()
			&& !CurrentHash->Equals(Target.RecordedAfterHash, ESearchCase::CaseSensitive)
			&& Result.HashGuardTargetKey.IsEmpty())
		{
			Result.HashGuardTargetKey = Target.TargetKey;
			Result.HashGuardExpectedHash = Target.RecordedAfterHash;
			Result.HashGuardCurrentHash = *CurrentHash;
			Result.HashGuardRecordedAfterSnapshotJson = Target.AfterSnapshotJson;
		}
		FBlueprintHelperReviewVisibleChange TargetChange = Change;
		TargetChange.AtomicTargets.Reset();
		TargetChange.AtomicTargets.Add(Target);
		const FBlueprintHelperReviewRestoreAdapterLookup RestoreLookup =
			AdapterRegistry.IsValid()
				? AdapterRegistry->FindRestoreAdapter(Target.TargetKind)
				: FBlueprintHelperReviewRestoreAdapterLookup();
		if (RestoreLookup.bAvailable && RestoreLookup.Adapter.IsValid())
		{
			const FBlueprintHelperReviewRestoreResult RestoreResult =
				RestoreLookup.Adapter->RestoreBeforeSnapshot(TargetChange);
			if (!RestoreResult.bSucceeded)
			{
				Result.NewStatus = RestoreResult.NewStatus;
				Result.RollbackMode = RestoreResult.RollbackMode;
				Result.Message = RestoreResult.Message;
				return Result;
			}
			continue;
		}
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::NeedsAction;
		Result.Message = RestoreLookup.Message.IsEmpty()
			? FString::Printf(TEXT("restore_adapter_unavailable:%s"), *Target.TargetKind)
			: RestoreLookup.Message;
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
	FBlueprintHelperReviewRejectOptions Options;
	return RejectLifecycleRootVisibleChange(Root, PendingChanges, Options);
}

FBlueprintHelperReviewCascadeActionResult FBlueprintHelperReviewActionService::RejectLifecycleRootVisibleChange(
	const FBlueprintHelperReviewVisibleChange& Root,
	const TArray<FBlueprintHelperReviewVisibleChange>& PendingChanges,
	const FBlueprintHelperReviewRejectOptions& Options) const
{
	if (!BlueprintHelperReviewVisibleChangeHasAssetFactoryTarget(Root))
	{
		FBlueprintHelperReviewCascadeActionResult CascadeResult;
		const TArray<FBlueprintHelperReviewVisibleChange> Descendants =
			FBlueprintHelperReviewRejectService::CollectLifecycleDescendantsDeepestFirst(Root, PendingChanges);
		for (const FBlueprintHelperReviewVisibleChange& Descendant : Descendants)
		{
			FString ChildReviewRecordId;
			TArray<FString> ChildTargetKeys;
			if (!FBlueprintHelperReviewActionTargetUtils::TryResolvePersistedReviewChange(
				Descendant,
				ChildReviewRecordId,
				ChildTargetKeys)
				|| ChildReviewRecordId.IsEmpty()
				|| ChildTargetKeys.Num() == 0)
			{
				CascadeResult.RootResult = BlueprintHelperReviewMakeCascadeChildFailure(
					Root,
					Descendant,
					TEXT("child_review_target_not_found"));
				return CascadeResult;
			}

			const FBlueprintHelperReviewActionResult ChildResult =
				RejectReviewTargets(ChildReviewRecordId, ChildTargetKeys, Options);
			if (!ChildResult.bSucceeded)
			{
				CascadeResult.RootResult = BlueprintHelperReviewMakeCascadeChildFailure(
					Root,
					Descendant,
					ChildResult.Message);
				return CascadeResult;
			}

			CascadeResult.RemovedChildChangeIds.Add(Descendant.ChangeId);
		}

		FString RootReviewRecordId;
		TArray<FString> RootTargetKeys;
		if (FBlueprintHelperReviewActionTargetUtils::TryResolvePersistedReviewChange(Root, RootReviewRecordId, RootTargetKeys))
		{
			CascadeResult.RootResult = RejectReviewTargets(RootReviewRecordId, RootTargetKeys, Options);
		}
		else
		{
			CascadeResult.RootResult = RejectVisibleChange(Root, Options);
		}
		CascadeResult.bChildrenRemoved = CascadeResult.RemovedChildChangeIds.Num() > 0;
		return CascadeResult;
	}

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

FBlueprintHelperReviewCascadeActionResult FBlueprintHelperReviewActionService::RejectLifecycleRootVisibleChange(
	const FBlueprintHelperReviewVisibleChange& Root,
	const FBlueprintHelperReviewRejectOptions& Options) const
{
	FBlueprintHelperReviewStoreService Store;
	const TArray<FBlueprintHelperReviewVisibleChange> PendingChanges =
		Store.LoadPendingVisibleChanges(Root.AssetPath);
	return RejectLifecycleRootVisibleChange(Root, PendingChanges, Options);
}

void FBlueprintHelperReviewActionService::RecordRejectDebugCaseBestEffort(
	FBlueprintHelperReviewRecord& Record,
	const TArray<FString>& TargetKeys,
	const FString& SourceEvidenceId,
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
	if (!SourceEvidenceId.IsEmpty())
	{
		ToolSummary->SetStringField(TEXT("source_evidence_id"), SourceEvidenceId);
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
	if (!SourceEvidenceId.IsEmpty())
	{
		FBlueprintHelperDebugEvidenceLink EvidenceLink;
		EvidenceLink.EvidenceId = SourceEvidenceId;
		EvidenceLink.Role = TEXT("review_reject_failed");
		EvidenceLink.Source = TEXT("review");
		EvidenceLink.Summary = TEXT("source evidence for review reject action");
		DebugInput.EvidenceLinks.Add(EvidenceLink);
	}
	DebugInput.Error.Code = DebugInput.Source;
	DebugInput.Error.Message = RejectStatus == EBlueprintHelperReviewChangeStatus::RejectFailed
		&& !RejectMessage.Contains(TEXT("rollback_backend_failed"))
		? FString::Printf(TEXT("rollback_backend_failed:%s"), *RejectMessage)
		: RejectMessage;
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
	FString SourceEvidenceId;
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
			SourceEvidenceId = Target.LatestEvidenceId;
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
		SourceEvidenceId,
		TEXT("accepted")));

	if (!Store.SaveReviewRecord(Record, Error))
	{
		Result.Message = Error;
		return Result;
	}

	Result.bSucceeded = true;
	Result.TargetEvidenceId = SourceEvidenceId;
	Result.NewStatus = Record.Status;
	Result.Message = TEXT("accepted");
	Result.bSupersededDataCompactionEligible = true;
	if (Record.Status == EBlueprintHelperReviewChangeStatus::Accepted)
	{
		FString SessionError;
		FBlueprintHelperSequentialReviewSessionService().CloseSessionsForReviewRecord(
			ReviewRecordId,
			EBlueprintHelperSequentialReviewSessionStatus::Accepted,
			SessionError);
	}
	return Result;
}

FBlueprintHelperReviewActionResult FBlueprintHelperReviewActionService::RejectReviewTargets(
	const FString& ReviewRecordId,
	const TArray<FString>& TargetKeys,
	const FBlueprintHelperReviewRejectOptions& Options) const
{
	FBlueprintHelperReviewPerformanceScope Scope(TEXT("ReviewReject.RejectReviewTargets"));
	Scope.AddText(TEXT("record"), ReviewRecordId);
	Scope.AddCount(TEXT("target_keys"), TargetKeys.Num());

	FBlueprintHelperReviewActionResult Result;
	FBlueprintHelperReviewStoreService Store;
	FBlueprintHelperReviewRecord Record;
	FString Error;
	{
		FBlueprintHelperReviewPerformanceScope LoadScope(TEXT("ReviewReject.LoadReviewRecord"));
		LoadScope.AddText(TEXT("record"), ReviewRecordId);
		if (!Store.LoadReviewRecordById(ReviewRecordId, Record, Error))
		{
			Result.Message = Error;
			return Result;
		}
	}

	Scope.AddCount(TEXT("visible_changes"), Record.VisibleChanges.Num());
	int64 AtomicTargetCount = 0;
	for (const FBlueprintHelperReviewVisibleChange& Change : Record.VisibleChanges)
	{
		AtomicTargetCount += Change.AtomicTargets.Num();
	}
	Scope.AddCount(TEXT("record_atomic_targets"), AtomicTargetCount);

	bool bMatchedAny = false;
	bool bAllRejected = true;
	bool bAllTargetStatusesRejected = true;
	FString SourceEvidenceId;
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
			FBlueprintHelperReviewActionResult TargetResult;
			{
				FBlueprintHelperReviewPerformanceScope TargetScope(TEXT("ReviewReject.TargetRollback"));
				TargetScope.AddText(TEXT("target_kind"), TargetForReject.TargetKind);
				TargetScope.AddText(TEXT("target_key"), TargetForReject.TargetKey);
				TargetScope.AddBytes(TEXT("before_snapshot"), TargetForReject.BeforeSnapshotJson.Len());
				TargetResult = RejectResolvedVisibleChange(TargetChange, Options);
				TargetScope.AddText(TEXT("status"), BlueprintHelperReviewChangeStatusToString(TargetResult.NewStatus));
				TargetScope.AddText(TEXT("message"), TargetResult.Message);
			}
			const bool bTargetStatusRejected = TargetResult.NewStatus == EBlueprintHelperReviewChangeStatus::Rejected;
			Target.Status = TargetResult.NewStatus;
			Change.NeedsActionReason = bTargetStatusRejected ? FString() : TargetResult.Message;
			SourceEvidenceId = Target.LatestEvidenceId;
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
		{
			FBlueprintHelperReviewPerformanceScope PurgeScope(TEXT("ReviewReject.PurgeReviewTargets"));
			PurgeScope.AddText(TEXT("record"), ReviewRecordId);
			PurgeScope.AddCount(TEXT("target_keys"), TargetKeys.Num());
			if (!Store.PurgeReviewTargets(ReviewRecordId, TargetKeys, DebugCaseIdsToDelete, bRecordDeleted, Error))
			{
				Result.Message = Error;
				return Result;
			}
			PurgeScope.AddCount(TEXT("debug_cases"), DebugCaseIdsToDelete.Num());
			PurgeScope.AddCount(TEXT("record_deleted"), bRecordDeleted ? 1 : 0);
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
		Result.TargetEvidenceId = SourceEvidenceId;
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
		Result.RollbackMode = TEXT("archive_baseline");
		Result.Message = LastMessage.IsEmpty() ? TEXT("rejected_purged") : LastMessage;
		Result.bSupersededDataCompactionEligible = true;
		FString SessionError;
		FBlueprintHelperSequentialReviewSessionService().CloseSessionsForReviewRecord(
			ReviewRecordId,
			EBlueprintHelperSequentialReviewSessionStatus::Rejected,
			SessionError);
		return Result;
	}

	FBlueprintHelperReviewStatusUtils::RefreshReviewRecordStatus(Record);
	Record.ReviewActions.Add(FBlueprintHelperReviewActionRecordUtils::MakeReviewActionRecord(
		TEXT("reject"),
		TargetKeys,
		TEXT("archive_baseline"),
		SourceEvidenceId,
		LastMessage));
	RecordRejectDebugCaseBestEffort(
		Record,
		TargetKeys,
		SourceEvidenceId,
		bAllTargetStatusesRejected ? EBlueprintHelperReviewChangeStatus::Rejected : LastStatus,
		LastMessage);

	{
		FBlueprintHelperReviewPerformanceScope SaveScope(TEXT("ReviewReject.SaveReviewRecord"));
		SaveScope.AddText(TEXT("record"), ReviewRecordId);
		if (!Store.SaveReviewRecord(Record, Error))
		{
			Result.Message = Error;
			return Result;
		}
	}

	Result.bSucceeded = bAllTargetStatusesRejected;
	Result.TargetEvidenceId = SourceEvidenceId;
	Result.NewStatus = bAllTargetStatusesRejected ? EBlueprintHelperReviewChangeStatus::Rejected : LastStatus;
	Result.RollbackMode = TEXT("archive_baseline");
	Result.Message = LastMessage;
	Result.bSupersededDataCompactionEligible = bAllTargetStatusesRejected;
	if (bAllTargetStatusesRejected)
	{
		FString SessionError;
		FBlueprintHelperSequentialReviewSessionService().CloseSessionsForReviewRecord(
			ReviewRecordId,
			EBlueprintHelperSequentialReviewSessionStatus::Rejected,
			SessionError);
	}
	return Result;
}

FBlueprintHelperReviewBatchActionResult FBlueprintHelperReviewActionService::AcceptReviewTargetsBatch(
	const TMap<FString, TArray<FString>>& TargetKeysByReviewRecordId) const
{
	FBlueprintHelperReviewBatchActionResult BatchResult;
	for (const TPair<FString, TArray<FString>>& Pair : TargetKeysByReviewRecordId)
	{
		BatchResult.RequestedCount += Pair.Value.Num();
		if (Pair.Key.IsEmpty() || Pair.Value.Num() == 0)
		{
			continue;
		}

		const FBlueprintHelperReviewActionResult RecordResult =
			AcceptReviewTargets(Pair.Key, Pair.Value);
		BatchResult.Results.Add(RecordResult);
		if (RecordResult.bSucceeded)
		{
			BatchResult.SucceededCount += Pair.Value.Num();
			BatchResult.ChangedReviewRecordIds.AddUnique(Pair.Key);
		}
		else
		{
			BatchResult.FailedCount += Pair.Value.Num();
			BatchResult.Message = RecordResult.Message;
		}
	}

	BatchResult.bSucceeded = BatchResult.RequestedCount > 0 && BatchResult.FailedCount == 0;
	if (BatchResult.Message.IsEmpty())
	{
		BatchResult.Message = BatchResult.bSucceeded ? TEXT("accepted_batch") : TEXT("no_pending_review_targets");
	}
	return BatchResult;
}

FBlueprintHelperReviewBatchActionResult FBlueprintHelperReviewActionService::RejectReviewTargetsBatch(
	const TMap<FString, TArray<FString>>& TargetKeysByReviewRecordId,
	const FBlueprintHelperReviewRejectOptions& Options) const
{
	FBlueprintHelperReviewBatchActionResult BatchResult;
	for (const TPair<FString, TArray<FString>>& Pair : TargetKeysByReviewRecordId)
	{
		BatchResult.RequestedCount += Pair.Value.Num();
		if (Pair.Key.IsEmpty() || Pair.Value.Num() == 0)
		{
			continue;
		}

		const FBlueprintHelperReviewActionResult RecordResult =
			RejectReviewTargets(Pair.Key, Pair.Value, Options);
		BatchResult.Results.Add(RecordResult);
		if (RecordResult.bSucceeded)
		{
			BatchResult.SucceededCount += Pair.Value.Num();
			BatchResult.ChangedReviewRecordIds.AddUnique(Pair.Key);
		}
		else
		{
			BatchResult.FailedCount += Pair.Value.Num();
			BatchResult.Message = RecordResult.Message;
		}
	}

	BatchResult.bSucceeded = BatchResult.RequestedCount > 0 && BatchResult.FailedCount == 0;
	if (BatchResult.Message.IsEmpty())
	{
		BatchResult.Message = BatchResult.bSucceeded ? TEXT("rejected_batch") : TEXT("no_pending_review_targets");
	}
	return BatchResult;
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

