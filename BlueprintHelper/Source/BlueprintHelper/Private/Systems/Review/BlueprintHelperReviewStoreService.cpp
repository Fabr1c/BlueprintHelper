// BlueprintHelper Review Store service implementation.

#include "Systems/Review/BlueprintHelperReviewStoreService.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Shared/Review/BlueprintHelperReviewEnumUtils.h"
#include "Shared/Review/BlueprintHelperReviewStatusUtils.h"
#include "Shared/Review/BlueprintHelperReviewTargetKindRegistry.h"
#include "Systems/Review/BlueprintHelperReviewBaselineSnapshotService.h"
#include "Systems/Review/Utils/BlueprintHelperReviewStoreJsonUtils.h"
#include "Systems/Review/Utils/BlueprintHelperReviewStoreMergeUtils.h"
#include "Systems/Review/Utils/BlueprintHelperReviewStorePathUtils.h"
#include "Systems/Review/Utils/BlueprintHelperReviewStoreTargetUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentEvidence.h"

namespace
{
	static void NormalizeReviewTargetSemanticSnapshots(
		const FBlueprintHelperWriteReviewEvidence& Evidence,
		FBlueprintHelperReviewAtomicTarget& Target)
	{
		FBlueprintHelperReviewBaselineSnapshotService SnapshotService;

		if (!Target.BeforeSnapshotJson.IsEmpty())
		{
			Target.BaselineHash =
				FBlueprintHelperReviewBaselineSnapshotService::ComputeSemanticSnapshotHash(Target.BeforeSnapshotJson);
		}
		else
		{
			FString BaselineSnapshotJson;
			FString BaselineSnapshotHash;
			FString BaselineSnapshotError;
			if (SnapshotService.TryLoadBaselineTargetSnapshot(
				Evidence.ArchiveSessionId,
				Target,
				BaselineSnapshotJson,
				BaselineSnapshotHash,
				BaselineSnapshotError))
			{
				Target.BeforeSnapshotJson = BaselineSnapshotJson;
				Target.BaselineHash = BaselineSnapshotHash;
			}
			else if (Evidence.ChangeKind == EBlueprintHelperReviewChangeKind::Added
				&& !FBlueprintHelperReviewTargetKindRegistry::SupportsSnapshotRestore(Target.TargetKind))
			{
				FBlueprintHelperReviewBaselineSnapshotService::MakeMissingTargetSnapshot(
					Target,
					true,
					Target.BeforeSnapshotJson,
					Target.BaselineHash);
			}
		}

		if (!Target.AfterSnapshotJson.IsEmpty())
		{
			Target.RecordedAfterHash =
				FBlueprintHelperReviewBaselineSnapshotService::ComputeSemanticSnapshotHash(Target.AfterSnapshotJson);
		}
		else if (Target.RecordedAfterHash.IsEmpty())
		{
			FString AfterSnapshotJson;
			FString AfterSnapshotHash;
			FString AfterSnapshotError;
			if (SnapshotService.CaptureTargetSnapshot(Target, AfterSnapshotJson, AfterSnapshotHash, AfterSnapshotError))
			{
				Target.AfterSnapshotJson = AfterSnapshotJson;
				Target.RecordedAfterHash = AfterSnapshotHash;
			}
		}
	}
}

FString FBlueprintHelperReviewStoreService::NormalizeGraphBlockTargetId(
	const FString& GraphName,
	const FString& BlockRefOrId)
{
	FString Normalized = BlockRefOrId;
	Normalized.TrimStartAndEndInline();
	if (GraphName.IsEmpty() || Normalized.IsEmpty())
	{
		return Normalized;
	}

	const FString GraphPrefix = GraphName + TEXT("_");
	if (Normalized.StartsWith(GraphPrefix, ESearchCase::CaseSensitive))
	{
		return Normalized;
	}

	return GraphPrefix + Normalized;
}

FString FBlueprintHelperReviewStoreService::MakeReviewRecordId(
	const FString& ArchiveSessionId,
	const FString& AssetPath)
{
	FString NormalizedAsset = AssetPath;
	NormalizedAsset.TrimStartAndEndInline();
	NormalizedAsset.ReplaceInline(TEXT("/"), TEXT("_"));
	NormalizedAsset.ReplaceInline(TEXT("\\"), TEXT("_"));
	NormalizedAsset.ReplaceInline(TEXT(":"), TEXT("_"));
	NormalizedAsset.ReplaceInline(TEXT("."), TEXT("_"));
	NormalizedAsset.ReplaceInline(TEXT(" "), TEXT("_"));
	while (NormalizedAsset.Contains(TEXT("__")))
	{
		NormalizedAsset.ReplaceInline(TEXT("__"), TEXT("_"));
	}
	while (NormalizedAsset.StartsWith(TEXT("_")))
	{
		NormalizedAsset.RemoveFromStart(TEXT("_"));
	}
	while (NormalizedAsset.EndsWith(TEXT("_")))
	{
		NormalizedAsset.RemoveFromEnd(TEXT("_"));
	}
	return FString::Printf(TEXT("review_%s_%s"), *ArchiveSessionId, *NormalizedAsset);
}

TArray<FBlueprintHelperReviewVisibleChange> FBlueprintHelperReviewStoreService::BuildVisibleChanges(
	const TArray<FBlueprintHelperReviewTransactionInput>& Transactions) const
{
	TMap<FString, FBlueprintHelperReviewVisibleChange> AtomicChanges;
	TArray<FString> AtomicOrder;

	for (const FBlueprintHelperReviewTransactionInput& Input : Transactions)
	{
		if (Input.ChangeKind == EBlueprintHelperReviewChangeKind::Renamed)
		{
			FBlueprintHelperReviewTransactionInput Removed = Input;
			Removed.ChangeKind = EBlueprintHelperReviewChangeKind::Removed;
			Removed.LocationKey = Input.LocationKey + TEXT(":rename_removed");
			Removed.AfterSummary = TEXT("");
			for (FBlueprintHelperReviewAtomicTarget& Target : Removed.AtomicTargets)
			{
				Target.TargetKey += TEXT(":rename_removed");
				Target.VisualGroupKey += TEXT(":rename_removed");
			}
			AddAtomicTargetsForInput(Removed, AtomicChanges, AtomicOrder);

			FBlueprintHelperReviewTransactionInput Added = Input;
			Added.ChangeKind = EBlueprintHelperReviewChangeKind::Added;
			Added.LocationKey = Input.LocationKey + TEXT(":rename_added");
			Added.BeforeSummary = TEXT("");
			for (FBlueprintHelperReviewAtomicTarget& Target : Added.AtomicTargets)
			{
				Target.TargetKey += TEXT(":rename_added");
				Target.VisualGroupKey += TEXT(":rename_added");
			}
			AddAtomicTargetsForInput(Added, AtomicChanges, AtomicOrder);
			continue;
		}

		AddAtomicTargetsForInput(Input, AtomicChanges, AtomicOrder);
	}

	TArray<FBlueprintHelperReviewVisibleChange> Changes;
	TMap<FString, int32> GroupToIndex;
	for (const FString& AtomicKey : AtomicOrder)
	{
		if (const FBlueprintHelperReviewVisibleChange* AtomicChange = AtomicChanges.Find(AtomicKey))
		{
			GroupAtomicVisibleChange(*AtomicChange, GroupToIndex, Changes);
		}
	}

	return Changes;
}

TArray<FBlueprintHelperReviewRecord> FBlueprintHelperReviewStoreService::BuildReviewRecordsFromEvidence(
	const TArray<FBlueprintHelperWriteReviewEvidence>& Evidences) const
{
	TMap<FString, FBlueprintHelperReviewRecord> RecordsById;
	TArray<FString> RecordOrder;

	for (const FBlueprintHelperWriteReviewEvidence& Evidence : Evidences)
	{
		if (Evidence.ArchiveSessionId.IsEmpty() || Evidence.AssetPath.IsEmpty())
		{
			continue;
		}

		const FString RecordId = MakeReviewRecordId(Evidence.ArchiveSessionId, Evidence.AssetPath);
		FBlueprintHelperReviewRecord* Record = RecordsById.Find(RecordId);
		if (!Record)
		{
			FBlueprintHelperReviewRecord NewRecord;
			NewRecord.ReviewRecordId = RecordId;
			NewRecord.ArchiveSessionId = Evidence.ArchiveSessionId;
			NewRecord.AssetPath = Evidence.AssetPath;
			NewRecord.SourceTransactionSummary.AssetPaths.AddUnique(Evidence.AssetPath);
			RecordsById.Add(RecordId, NewRecord);
			RecordOrder.Add(RecordId);
			Record = RecordsById.Find(RecordId);
		}

		if (!Record)
		{
			continue;
		}

		if (!Evidence.TaskRunId.IsEmpty())
		{
			Record->SourceTaskRunIds.AddUnique(Evidence.TaskRunId);
			Record->SourceTransactionSummary.TaskRunIds.AddUnique(Evidence.TaskRunId);
		}
		if (!Evidence.OperationKind.IsEmpty())
		{
			Record->SourceTransactionSummary.OperationKinds.AddUnique(Evidence.OperationKind);
		}
		if (!Evidence.TransactionId.IsEmpty())
		{
			Record->SourceTransactionSummary.TransactionIds.AddUnique(Evidence.TransactionId);
		}
		if (!Evidence.CreatedAt.IsEmpty())
		{
			if (Record->SourceTransactionSummary.CreatedAtFirst.IsEmpty()
				|| Evidence.CreatedAt < Record->SourceTransactionSummary.CreatedAtFirst)
			{
				Record->SourceTransactionSummary.CreatedAtFirst = Evidence.CreatedAt;
			}
			if (Record->SourceTransactionSummary.CreatedAtLast.IsEmpty()
				|| Evidence.CreatedAt > Record->SourceTransactionSummary.CreatedAtLast)
			{
				Record->SourceTransactionSummary.CreatedAtLast = Evidence.CreatedAt;
			}
		}
		for (const FString& DebugCaseId : Evidence.DebugCaseIds)
		{
			Record->DebugCaseIds.AddUnique(DebugCaseId);
		}

		AddEvidenceAtomicTargets(Evidence, *Record);
		Record->SourceTransactionSummary.TransactionCount =
			Record->SourceTransactionSummary.TransactionIds.Num();
	}

	TArray<FBlueprintHelperReviewRecord> Records;
	for (const FString& RecordId : RecordOrder)
	{
		if (FBlueprintHelperReviewRecord* Record = RecordsById.Find(RecordId))
		{
			FBlueprintHelperReviewStoreTargetUtils::LinkPendingChildrenToLifecycleRoots(Record->VisibleChanges);
			FBlueprintHelperReviewStoreTargetUtils::SortVisibleChangesByReviewOrder(Record->VisibleChanges);
			if (Record->VisibleChanges.Num() == 0)
			{
				continue;
			}

			bool bNeedsAction = false;
			bool bHasPending = false;
			for (const FBlueprintHelperReviewVisibleChange& Change : Record->VisibleChanges)
			{
				bNeedsAction |= Change.Status == EBlueprintHelperReviewChangeStatus::NeedsAction;
				bHasPending |= Change.Status == EBlueprintHelperReviewChangeStatus::Pending;
			}
			Record->Status = bNeedsAction
				? EBlueprintHelperReviewChangeStatus::NeedsAction
				: (bHasPending ? EBlueprintHelperReviewChangeStatus::Pending : Record->Status);
			Record->SourceTransactionSummary.FinalReviewStatus = Record->Status;
			Records.Add(*Record);
		}
	}

	return Records;
}

TArray<FBlueprintHelperReviewRecord> FBlueprintHelperReviewStoreService::BuildReviewRecordsFromFragmentEvidence(
	const FBlueprintHelperGraphFragmentEvidenceBundle& FragmentEvidence,
	const FString& ArchiveSessionId,
	const FString& AssetPath,
	const FString& OperationKind,
	const FString& TaskRunId,
	const FString& TransactionId,
	const FString& CreatedAt) const
{
	if (ArchiveSessionId.IsEmpty() || AssetPath.IsEmpty() || FragmentEvidence.IsEmpty())
	{
		return {};
	}

	FBlueprintHelperWriteReviewEvidence Evidence;
	Evidence.ArchiveSessionId = ArchiveSessionId;
	Evidence.TaskRunId = TaskRunId;
	Evidence.TransactionId = TransactionId.IsEmpty() ? FragmentEvidence.BundleId : TransactionId;
	Evidence.CreatedAt = CreatedAt;
	Evidence.AssetPath = AssetPath;
	Evidence.OperationKind = OperationKind.IsEmpty() ? TEXT("graph_fragment") : OperationKind;
	Evidence.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	Evidence.DisplayLabel = TEXT("Graph body changes");
	Evidence.AfterSummary = FString::Printf(
		TEXT("Fragment evidence scopes=%d fragments=%d diagnostics=%d"),
		FragmentEvidence.ReviewScopes.Num(),
		FragmentEvidence.Fragments.Num(),
		FragmentEvidence.Diagnostics.Num());

	for (const FBlueprintHelperGraphFragmentEvidenceReviewScope& Scope : FragmentEvidence.ReviewScopes)
	{
		const FString GraphName = !Scope.GraphName.IsEmpty()
			? Scope.GraphName
			: (!Scope.ScopeName.IsEmpty() ? Scope.ScopeName : TEXT("Graph"));
		const FString ScopeLabel = !Scope.ScopeName.IsEmpty() ? Scope.ScopeName : GraphName;

		FBlueprintHelperReviewAtomicTarget Target;
		Target.AssetPath = AssetPath;
		Target.Surface = EBlueprintHelperReviewSurface::Graph;
		Target.GraphName = GraphName;
		Target.TargetKind = TEXT("graph_body");
		Target.TargetKey = Scope.ScopeId.IsEmpty()
			? FString::Printf(TEXT("graph_body:%s"), *GraphName)
			: Scope.ScopeId;
		Target.VisualGroupKey = TEXT("graph_body|") + GraphName;
		Target.DisplayLabel = FString::Printf(TEXT("Modified [%s] graph body"), *ScopeLabel);
		if (!Evidence.TransactionId.IsEmpty())
		{
			Target.LatestTransactionId = Evidence.TransactionId;
			Target.SourceTransactionIds.AddUnique(Evidence.TransactionId);
		}
		Evidence.AtomicTargets.Add(Target);
	}

	if (Evidence.AtomicTargets.Num() == 0)
	{
		FBlueprintHelperReviewAtomicTarget Target;
		Target.AssetPath = AssetPath;
		Target.Surface = EBlueprintHelperReviewSurface::Graph;
		Target.GraphName = TEXT("Graph");
		Target.TargetKind = TEXT("graph_body");
		Target.TargetKey = FragmentEvidence.BundleId.IsEmpty() ? TEXT("graph_body") : FragmentEvidence.BundleId;
		Target.VisualGroupKey = TEXT("graph_body|Graph");
		Target.DisplayLabel = TEXT("Modified graph body");
		if (!Evidence.TransactionId.IsEmpty())
		{
			Target.LatestTransactionId = Evidence.TransactionId;
			Target.SourceTransactionIds.AddUnique(Evidence.TransactionId);
		}
		Evidence.AtomicTargets.Add(Target);
	}

	return BuildReviewRecordsFromEvidence({Evidence});
}

TArray<FBlueprintHelperReviewRecord> FBlueprintHelperReviewStoreService::QueryReviewRecords(
	const FBlueprintHelperReviewRecordQuery& Query) const
{
	TArray<FBlueprintHelperReviewRecord> Records;
	const FString RecordsDir = FPaths::ProjectSavedDir()
		/ TEXT("BlueprintHelper")
		/ TEXT("Review")
		/ TEXT("Records");

	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(RecordsDir / TEXT("*.json")), true, false);
	for (const FString& File : Files)
	{
		const FString Path = RecordsDir / File;
		FString Content;
		if (!FFileHelper::LoadFileToString(Content, *Path))
		{
			continue;
		}

		TSharedPtr<FJsonObject> Json;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
		if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
		{
			continue;
		}

		FBlueprintHelperReviewRecord Record;
		if (!FBlueprintHelperReviewStoreJsonUtils::ReadReviewRecordFromJson(Json, Record))
		{
			continue;
		}
		if (Record.VisibleChanges.Num() == 0)
		{
			continue;
		}

		if (!Query.ArchiveSessionIdFilter.IsEmpty() && Record.ArchiveSessionId != Query.ArchiveSessionIdFilter)
		{
			continue;
		}
		if (!Query.AssetPathFilter.IsEmpty() && Record.AssetPath != Query.AssetPathFilter)
		{
			continue;
		}
		if (!Query.TaskRunIdFilter.IsEmpty()
			&& !Record.SourceTaskRunIds.Contains(Query.TaskRunIdFilter)
			&& !Record.SourceTransactionSummary.TaskRunIds.Contains(Query.TaskRunIdFilter))
		{
			continue;
		}
		if (Query.bPendingOnly
			&& (Record.Status == EBlueprintHelperReviewChangeStatus::Accepted
				|| Record.Status == EBlueprintHelperReviewChangeStatus::Rejected))
		{
			continue;
		}

		Records.Add(Record);
	}

	return Records;
}

bool FBlueprintHelperReviewStoreService::LoadReviewRecordById(
	const FString& ReviewRecordId,
	FBlueprintHelperReviewRecord& OutRecord,
	FString& OutError) const
{
	if (ReviewRecordId.IsEmpty())
	{
		OutError = TEXT("review_record_id is required");
		return false;
	}

	const FString Path = FPaths::ProjectSavedDir()
		/ TEXT("BlueprintHelper")
		/ TEXT("Review")
		/ TEXT("Records")
		/ FString::Printf(TEXT("%s.json"), *ReviewRecordId);

	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *Path))
	{
		OutError = FString::Printf(TEXT("review record not found: %s"), *ReviewRecordId);
		return false;
	}

	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
	if (!FJsonSerializer::Deserialize(Reader, Json) || !FBlueprintHelperReviewStoreJsonUtils::ReadReviewRecordFromJson(Json, OutRecord))
	{
		OutError = FString::Printf(TEXT("failed to parse review record: %s"), *ReviewRecordId);
		return false;
	}

	return true;
}

bool FBlueprintHelperReviewStoreService::DeleteReviewRecord(
	const FString& ReviewRecordId,
	FString& OutError) const
{
	if (!FBlueprintHelperReviewStorePathUtils::IsSafeReviewRecordId(ReviewRecordId))
	{
		OutError = TEXT("invalid review_record_id");
		return false;
	}

	const FString Path = FBlueprintHelperReviewStorePathUtils::GetRecordPath(ReviewRecordId);
	if (!IFileManager::Get().FileExists(*Path))
	{
		OutError.Reset();
		return true;
	}

	if (!IFileManager::Get().Delete(*Path, false, true))
	{
		OutError = FString::Printf(TEXT("failed to delete review record: %s"), *ReviewRecordId);
		return false;
	}

	OutError.Reset();
	return true;
}

bool FBlueprintHelperReviewStoreService::PurgeReviewTargets(
	const FString& ReviewRecordId,
	const TArray<FString>& TargetKeys,
	TArray<FString>& OutDebugCaseIdsToDelete,
	bool& bOutRecordDeleted,
	FString& OutError) const
{
	OutDebugCaseIdsToDelete.Reset();
	bOutRecordDeleted = false;

	FBlueprintHelperReviewRecord Record;
	if (!LoadReviewRecordById(ReviewRecordId, Record, OutError))
	{
		return false;
	}

	TSet<FString> TargetKeySet;
	for (const FString& TargetKey : TargetKeys)
	{
		if (!TargetKey.IsEmpty())
		{
			TargetKeySet.Add(TargetKey);
		}
	}

	bool bMatchedAny = false;
	for (int32 ChangeIndex = Record.VisibleChanges.Num() - 1; ChangeIndex >= 0; --ChangeIndex)
	{
		FBlueprintHelperReviewVisibleChange& Change = Record.VisibleChanges[ChangeIndex];
		const int32 RemovedTargetCount = Change.AtomicTargets.RemoveAll(
			[&TargetKeySet](const FBlueprintHelperReviewAtomicTarget& Target)
			{
				return FBlueprintHelperReviewStatusUtils::ReviewTargetMatches(Target, TargetKeySet);
			});
		if (RemovedTargetCount > 0)
		{
			bMatchedAny = true;
		}
		if (Change.AtomicTargets.Num() == 0)
		{
			Record.VisibleChanges.RemoveAt(ChangeIndex);
		}
	}

	if (!bMatchedAny)
	{
		OutError = TEXT("target_keys_not_found");
		return false;
	}

	if (Record.VisibleChanges.Num() == 0)
	{
		OutDebugCaseIdsToDelete = Record.DebugCaseIds;
		if (!DeleteReviewRecord(ReviewRecordId, OutError))
		{
			return false;
		}
		bOutRecordDeleted = true;
		return true;
	}

	FBlueprintHelperReviewStatusUtils::RefreshReviewRecordStatus(Record);
	return SaveReviewRecord(Record, OutError);
}

TSharedRef<FJsonObject> FBlueprintHelperReviewStoreService::BuildReviewRecordSummaryArtifact(
	const FBlueprintHelperReviewRecord& Record) const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.ReviewSummaryArtifact.v1"));
	Json->SetStringField(TEXT("review_record_id"), Record.ReviewRecordId);
	Json->SetStringField(TEXT("archive_session_id"), Record.ArchiveSessionId);
	Json->SetStringField(TEXT("asset_path"), Record.AssetPath);
	Json->SetStringField(TEXT("status"), BlueprintHelperReviewChangeStatusToString(Record.Status));
	Json->SetStringField(TEXT("storage_status"), BlueprintHelperReviewStorageStatusToString(Record.StorageStatus));
	Json->SetArrayField(TEXT("source_task_run_ids"), FBlueprintHelperReviewStoreJsonUtils::MakeReviewJsonStringArray(Record.SourceTaskRunIds));
	Json->SetArrayField(TEXT("debug_case_ids"), FBlueprintHelperReviewStoreJsonUtils::MakeReviewJsonStringArray(Record.DebugCaseIds));

	FBlueprintHelperReviewArchiveSession ArchiveSession;
	FString ArchiveSessionError;
	if (!Record.ArchiveSessionId.IsEmpty() && LoadArchiveSession(Record.ArchiveSessionId, ArchiveSession, ArchiveSessionError))
	{
		TSharedRef<FJsonObject> Baseline = MakeShared<FJsonObject>();
		if (!ArchiveSession.BaselineDirtyAssetPolicy.IsEmpty())
		{
			Baseline->SetStringField(TEXT("dirty_asset_policy"), ArchiveSession.BaselineDirtyAssetPolicy);
		}
		if (!ArchiveSession.BaselineSnapshotTrust.IsEmpty())
		{
			Baseline->SetStringField(TEXT("snapshot_trust"), ArchiveSession.BaselineSnapshotTrust);
		}
		Baseline->SetStringField(TEXT("hash_source"), TEXT("semantic_target_snapshot"));
		Baseline->SetStringField(TEXT("snapshot_schema"), TEXT("BlueprintHelper.ReviewBaselineSemanticSnapshot.v1"));
		Baseline->SetStringField(TEXT("retention_mode"), TEXT("standard"));
		Baseline->SetArrayField(TEXT("dirty_target_assets"),
			FBlueprintHelperReviewStoreJsonUtils::MakeReviewJsonStringArray(ArchiveSession.DirtyTargetAssets));
		Baseline->SetArrayField(TEXT("warnings"),
			FBlueprintHelperReviewStoreJsonUtils::MakeReviewJsonStringArray(ArchiveSession.BaselineWarnings));
		Baseline->SetArrayField(TEXT("disk_snapshot_refs"),
			FBlueprintHelperReviewStoreJsonUtils::MakeReviewJsonStringArray(ArchiveSession.BaselineSnapshotRefs));
		Baseline->SetArrayField(TEXT("semantic_snapshot_refs"),
			FBlueprintHelperReviewStoreJsonUtils::MakeReviewJsonStringArray(ArchiveSession.BaselineSemanticSnapshotRefs));
		Json->SetObjectField(TEXT("baseline"), Baseline);
	}

	TSharedRef<FJsonObject> SourceSummary = MakeShared<FJsonObject>();
	SourceSummary->SetNumberField(TEXT("transaction_count"), Record.SourceTransactionSummary.TransactionCount);
	SourceSummary->SetArrayField(TEXT("task_run_ids"), FBlueprintHelperReviewStoreJsonUtils::MakeReviewJsonStringArray(Record.SourceTransactionSummary.TaskRunIds));
	SourceSummary->SetArrayField(TEXT("operation_kinds"), FBlueprintHelperReviewStoreJsonUtils::MakeReviewJsonStringArray(Record.SourceTransactionSummary.OperationKinds));
	SourceSummary->SetArrayField(TEXT("asset_paths"), FBlueprintHelperReviewStoreJsonUtils::MakeReviewJsonStringArray(Record.SourceTransactionSummary.AssetPaths));
	SourceSummary->SetArrayField(TEXT("transaction_ids"), FBlueprintHelperReviewStoreJsonUtils::MakeReviewJsonStringArray(Record.SourceTransactionSummary.TransactionIds));
	if (!Record.SourceTransactionSummary.CreatedAtFirst.IsEmpty())
	{
		SourceSummary->SetStringField(TEXT("created_at_first"), Record.SourceTransactionSummary.CreatedAtFirst);
	}
	if (!Record.SourceTransactionSummary.CreatedAtLast.IsEmpty())
	{
		SourceSummary->SetStringField(TEXT("created_at_last"), Record.SourceTransactionSummary.CreatedAtLast);
	}
	SourceSummary->SetStringField(TEXT("final_review_status"),
		BlueprintHelperReviewChangeStatusToString(Record.SourceTransactionSummary.FinalReviewStatus));
	Json->SetObjectField(TEXT("source_transaction_summary"), SourceSummary);

	TArray<TSharedPtr<FJsonValue>> ChangeSummaries;
	for (const FBlueprintHelperReviewVisibleChange& Change : Record.VisibleChanges)
	{
		TSharedRef<FJsonObject> ChangeJson = MakeShared<FJsonObject>();
		ChangeJson->SetStringField(TEXT("change_id"), Change.ChangeId);
		ChangeJson->SetStringField(TEXT("asset_path"), Change.AssetPath);
		if (!Change.GraphName.IsEmpty())
		{
			ChangeJson->SetStringField(TEXT("graph_name"), Change.GraphName);
		}
		if (!Change.LocationKey.IsEmpty())
		{
			ChangeJson->SetStringField(TEXT("visual_group_key"), Change.LocationKey);
		if (!Change.ScopeIdentity.IsEmpty()) ChangeJson->SetStringField(TEXT("scope_identity"), Change.ScopeIdentity);
		}
		ChangeJson->SetStringField(TEXT("change_kind"), BlueprintHelperReviewChangeKindToString(Change.ChangeKind));
		ChangeJson->SetStringField(TEXT("status"), BlueprintHelperReviewChangeStatusToString(Change.Status));
		if (!Change.DisplayLabel.IsEmpty())
		{
			ChangeJson->SetStringField(TEXT("display_label"), Change.DisplayLabel);
		}
		if (!Change.BeforeSummary.IsEmpty())
		{
			ChangeJson->SetStringField(TEXT("before_summary"), Change.BeforeSummary);
		}
		if (!Change.AfterSummary.IsEmpty())
		{
			ChangeJson->SetStringField(TEXT("after_summary"), Change.AfterSummary);
		}
		if (!Change.NeedsActionReason.IsEmpty())
		{
			ChangeJson->SetStringField(TEXT("needs_action_reason"), Change.NeedsActionReason);
		}
		if (!Change.ParentChangeId.IsEmpty())
		{
			ChangeJson->SetStringField(TEXT("parent_change_id"), Change.ParentChangeId);
		}
		if (Change.ExecutionOrder != INDEX_NONE)
		{
			ChangeJson->SetNumberField(TEXT("execution_order"), Change.ExecutionOrder);
		}
		if (Change.TaskStepIndex != INDEX_NONE)
		{
			ChangeJson->SetNumberField(TEXT("task_step_index"), Change.TaskStepIndex);
		}
		if (Change.AtomicIndex != INDEX_NONE)
		{
			ChangeJson->SetNumberField(TEXT("atomic_index"), Change.AtomicIndex);
		}
		if (Change.bIsAssetLifecycleRoot)
		{
			ChangeJson->SetBoolField(TEXT("is_asset_lifecycle_root"), true);
		}
		if (Change.bRejectRemovesChildren)
		{
			ChangeJson->SetBoolField(TEXT("reject_removes_children"), true);
		}
		ChangeJson->SetNumberField(TEXT("atomic_target_count"), Change.AtomicTargets.Num());
		ChangeSummaries.Add(MakeShared<FJsonValueObject>(ChangeJson));
	}
	Json->SetNumberField(TEXT("visible_change_count"), Record.VisibleChanges.Num());
	Json->SetArrayField(TEXT("visible_changes"), ChangeSummaries);
	Json->SetNumberField(TEXT("review_action_count"), Record.ReviewActions.Num());
	return Json;
}

bool FBlueprintHelperReviewStoreService::SaveReviewRecord(
	const FBlueprintHelperReviewRecord& Record,
	FString& OutError) const
{
	if (Record.ReviewRecordId.IsEmpty())
	{
		OutError = TEXT("review_record_id is required");
		return false;
	}

	const FString RecordsDir = FPaths::ProjectSavedDir()
		/ TEXT("BlueprintHelper")
		/ TEXT("Review")
		/ TEXT("Records");
	if (!IFileManager::Get().DirectoryExists(*RecordsDir))
	{
		IFileManager::Get().MakeDirectory(*RecordsDir, true);
	}

	FBlueprintHelperReviewRecord RecordToWrite = Record;
	FBlueprintHelperReviewStoreMergeUtils::CollapseVisibleChangesLatestWins(RecordToWrite.VisibleChanges);
	FBlueprintHelperReviewStoreTargetUtils::LinkPendingChildrenToLifecycleRoots(RecordToWrite.VisibleChanges);
	FBlueprintHelperReviewStoreTargetUtils::SortVisibleChangesByReviewOrder(RecordToWrite.VisibleChanges);
	if (RecordToWrite.VisibleChanges.Num() == 0)
	{
		return DeleteReviewRecord(Record.ReviewRecordId, OutError);
	}
	FBlueprintHelperReviewStatusUtils::RefreshReviewRecordStatus(RecordToWrite);

	FString JsonText;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	if (!FJsonSerializer::Serialize(FBlueprintHelperReviewStoreJsonUtils::ReviewRecordToJson(RecordToWrite), Writer))
	{
		OutError = FString::Printf(TEXT("failed to serialize review record: %s"), *Record.ReviewRecordId);
		return false;
	}

	const FString Path = RecordsDir / FString::Printf(TEXT("%s.json"), *Record.ReviewRecordId);
	if (!FFileHelper::SaveStringToFile(JsonText, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("failed to write review record: %s"), *Path);
		return false;
	}

	return true;
}

bool FBlueprintHelperReviewStoreService::SaveReviewRecords(
	const TArray<FBlueprintHelperReviewRecord>& Records,
	FString& OutError) const
{
	const FString RecordsDir = FPaths::ProjectSavedDir()
		/ TEXT("BlueprintHelper")
		/ TEXT("Review")
		/ TEXT("Records");
	if (!IFileManager::Get().DirectoryExists(*RecordsDir))
	{
		IFileManager::Get().MakeDirectory(*RecordsDir, true);
	}

	for (const FBlueprintHelperReviewRecord& Record : Records)
	{
		if (Record.ReviewRecordId.IsEmpty())
		{
			OutError = TEXT("review_record_id is required");
			return false;
		}

		FBlueprintHelperReviewRecord RecordToWrite = Record;
		const FString Path = RecordsDir / FString::Printf(TEXT("%s.json"), *Record.ReviewRecordId);
		FString ExistingContent;
		if (FFileHelper::LoadFileToString(ExistingContent, *Path))
		{
			TSharedPtr<FJsonObject> ExistingJson;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ExistingContent);
			FBlueprintHelperReviewRecord ExistingRecord;
			if (FJsonSerializer::Deserialize(Reader, ExistingJson)
				&& FBlueprintHelperReviewStoreJsonUtils::ReadReviewRecordFromJson(ExistingJson, ExistingRecord))
			{
				FBlueprintHelperReviewStoreMergeUtils::MergeReviewRecord(ExistingRecord, Record);
				RecordToWrite = ExistingRecord;
			}
		}

		FBlueprintHelperReviewStoreTargetUtils::LinkPendingChildrenToLifecycleRoots(RecordToWrite.VisibleChanges);
		FBlueprintHelperReviewStoreMergeUtils::CollapseVisibleChangesLatestWins(RecordToWrite.VisibleChanges);
		if (RecordToWrite.VisibleChanges.Num() == 0)
		{
			if (!DeleteReviewRecord(Record.ReviewRecordId, OutError))
			{
				return false;
			}
			continue;
		}
		FBlueprintHelperReviewStoreTargetUtils::SortVisibleChangesByReviewOrder(RecordToWrite.VisibleChanges);
		FBlueprintHelperReviewStatusUtils::RefreshReviewRecordStatus(RecordToWrite);

		FString JsonText;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
		if (!FJsonSerializer::Serialize(FBlueprintHelperReviewStoreJsonUtils::ReviewRecordToJson(RecordToWrite), Writer))
		{
			OutError = FString::Printf(TEXT("failed to serialize review record: %s"), *Record.ReviewRecordId);
			return false;
		}

		if (!FFileHelper::SaveStringToFile(JsonText, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			OutError = FString::Printf(TEXT("failed to write review record: %s"), *Path);
			return false;
		}
	}

	return true;
}

bool FBlueprintHelperReviewStoreService::SaveArchiveSession(
	const FBlueprintHelperReviewArchiveSession& ArchiveSession,
	FString& OutError) const
{
	if (ArchiveSession.ArchiveSessionId.IsEmpty())
	{
		OutError = TEXT("archive_session_id is required");
		return false;
	}

	const FString SessionsDir = FPaths::ProjectSavedDir()
		/ TEXT("BlueprintHelper")
		/ TEXT("Review")
		/ TEXT("ArchiveSessions");
	if (!IFileManager::Get().DirectoryExists(*SessionsDir))
	{
		IFileManager::Get().MakeDirectory(*SessionsDir, true);
	}

	FString JsonText;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	if (!FJsonSerializer::Serialize(FBlueprintHelperReviewStoreJsonUtils::ReviewArchiveSessionToJson(ArchiveSession), Writer))
	{
		OutError = FString::Printf(TEXT("failed to serialize archive session: %s"), *ArchiveSession.ArchiveSessionId);
		return false;
	}

	const FString Path = SessionsDir / FString::Printf(TEXT("%s.json"), *ArchiveSession.ArchiveSessionId);
	if (!FFileHelper::SaveStringToFile(JsonText, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("failed to write archive session: %s"), *Path);
		return false;
	}

	return true;
}

bool FBlueprintHelperReviewStoreService::LoadArchiveSession(
	const FString& ArchiveSessionId,
	FBlueprintHelperReviewArchiveSession& OutArchiveSession,
	FString& OutError) const
{
	if (ArchiveSessionId.IsEmpty())
	{
		OutError = TEXT("archive_session_id is required");
		return false;
	}

	const FString Path = FPaths::ProjectSavedDir()
		/ TEXT("BlueprintHelper")
		/ TEXT("Review")
		/ TEXT("ArchiveSessions")
		/ FString::Printf(TEXT("%s.json"), *ArchiveSessionId);

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *Path))
	{
		OutError = FString::Printf(TEXT("archive session not found: %s"), *ArchiveSessionId);
		return false;
	}

	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Json)
		|| !FBlueprintHelperReviewStoreJsonUtils::ReadReviewArchiveSessionFromJson(Json, OutArchiveSession))
	{
		OutError = FString::Printf(TEXT("failed to parse archive session: %s"), *ArchiveSessionId);
		return false;
	}

	OutError.Empty();
	return true;
}

void FBlueprintHelperReviewStoreService::AddAtomicTargetsForInput(
	const FBlueprintHelperReviewTransactionInput& Input,
	TMap<FString, FBlueprintHelperReviewVisibleChange>& AtomicChanges,
	TArray<FString>& AtomicOrder) const
{
	TArray<FBlueprintHelperReviewAtomicTarget> AtomicTargets = MakeAtomicTargetsForInput(Input);
	for (FBlueprintHelperReviewAtomicTarget& Target : AtomicTargets)
	{
		Target.AssetPath = Target.AssetPath.IsEmpty() ? Input.AssetPath : Target.AssetPath;
		Target.GraphName = Target.GraphName.IsEmpty() ? Input.GraphName : Target.GraphName;
		Target.DisplayLabel = Target.DisplayLabel.IsEmpty() ? Input.DisplayLabel : Target.DisplayLabel;
		Target.VisualGroupKey = Target.VisualGroupKey.IsEmpty() ? Target.TargetKey : Target.VisualGroupKey;
		Target.LatestTransactionId = Input.TransactionId;

		const FString AtomicKey = FString::Printf(
			TEXT("%s|%s|%s|%s"),
			*Target.AssetPath,
			BlueprintHelperReviewSurfaceToString(Target.Surface),
			*Target.GraphName,
			*Target.TargetKey);

		FBlueprintHelperReviewVisibleChange* Existing = AtomicChanges.Find(AtomicKey);
		if (!Existing)
		{
			FBlueprintHelperReviewVisibleChange AtomicChange = MakeVisibleChange(Input);
			AtomicChange.AssetPath = Target.AssetPath;
			AtomicChange.GraphName = Target.GraphName;
			AtomicChange.LocationKey = Target.VisualGroupKey;
			AtomicChange.DisplayLabel = Target.DisplayLabel.IsEmpty() ? AtomicChange.DisplayLabel : Target.DisplayLabel;
			AtomicChange.AtomicTargets.Reset();
			AtomicChange.AtomicTargets.Add(Target);
			AtomicChange.LatestTransactionIds.Reset();
			AtomicChange.LatestTransactionIds.Add(Input.TransactionId);
			FBlueprintHelperReviewStoreTargetUtils::ApplyAssetLifecycleRootMetadata(AtomicChange);
			AtomicChanges.Add(AtomicKey, AtomicChange);
			AtomicOrder.Add(AtomicKey);
			continue;
		}

		FBlueprintHelperReviewVisibleChange& AtomicChange = *Existing;
		FBlueprintHelperReviewAtomicTarget& ExistingTarget = AtomicChange.AtomicTargets[0];
		ExistingTarget.LatestTransactionId = Input.TransactionId;
		ExistingTarget.SourceTransactionIds.Add(Input.TransactionId);
		ExistingTarget.GraphName = Target.GraphName;
		ExistingTarget.VisualGroupKey = Target.VisualGroupKey;
		ExistingTarget.DisplayLabel = Target.DisplayLabel;
		ExistingTarget.NodeGuid = Target.NodeGuid;
		ExistingTarget.PinPath = Target.PinPath;
		ExistingTarget.PropertyPath = Target.PropertyPath;
		ExistingTarget.ComponentPath = Target.ComponentPath;
		ExistingTarget.bHasGraphBounds = Target.bHasGraphBounds;
		ExistingTarget.GraphPosition = Target.GraphPosition;
		ExistingTarget.GraphSize = Target.GraphSize;

		AtomicChange.LatestTransactionId = Input.TransactionId;
		AtomicChange.LatestTransactionIds.Reset();
		AtomicChange.LatestTransactionIds.Add(Input.TransactionId);
		AtomicChange.SourceTransactionIds.Add(Input.TransactionId);
		AtomicChange.ChangeKind = Input.ChangeKind;
		AtomicChange.GraphName = Target.GraphName;
		AtomicChange.LocationKey = Target.VisualGroupKey;
		AtomicChange.DisplayLabel = Target.DisplayLabel.IsEmpty() ? AtomicChange.DisplayLabel : Target.DisplayLabel;
		AtomicChange.AfterSummary = Input.AfterSummary;
		AtomicChange.ChangeId = Input.TransactionId;
		FBlueprintHelperReviewStoreTargetUtils::ApplyAssetLifecycleRootMetadata(AtomicChange);
	}
}

TArray<FBlueprintHelperReviewVisibleChange> FBlueprintHelperReviewStoreService::LoadPendingVisibleChanges(
	const FString& AssetPathFilter) const
{
	FBlueprintHelperReviewRecordQuery Query;
	Query.AssetPathFilter = AssetPathFilter;
	Query.bPendingOnly = true;

	TArray<FBlueprintHelperReviewVisibleChange> RecordChanges;
	const TArray<FBlueprintHelperReviewRecord> Records = QueryReviewRecords(Query);
	const bool bSkipMissingAssetRecords = AssetPathFilter.IsEmpty();
	for (const FBlueprintHelperReviewRecord& Record : Records)
	{
		if (bSkipMissingAssetRecords
			&& !FBlueprintHelperReviewStoreTargetUtils::DoesReviewAssetPackageExist(Record.AssetPath))
		{
			continue;
		}

		for (const FBlueprintHelperReviewVisibleChange& Change : Record.VisibleChanges)
		{
			if (bSkipMissingAssetRecords
				&& !Change.AssetPath.IsEmpty()
				&& Change.AssetPath != Record.AssetPath
				&& !FBlueprintHelperReviewStoreTargetUtils::DoesReviewAssetPackageExist(Change.AssetPath))
			{
				continue;
			}

			if (Change.Status == EBlueprintHelperReviewChangeStatus::Pending
				|| Change.Status == EBlueprintHelperReviewChangeStatus::NeedsAction
				|| Change.Status == EBlueprintHelperReviewChangeStatus::RejectFailed)
			{
				RecordChanges.Add(Change);
			}
		}
	}
	FBlueprintHelperReviewStoreMergeUtils::CollapseVisibleChangesLatestWins(RecordChanges);
	FBlueprintHelperReviewStoreTargetUtils::LinkPendingChildrenToLifecycleRoots(RecordChanges);
	FBlueprintHelperReviewStoreTargetUtils::SortVisibleChangesByReviewOrder(RecordChanges);
	return RecordChanges;
}

FDelegateHandle FBlueprintHelperReviewStoreService::AddPendingReviewChangedHandler(const FSimpleDelegate& Handler) const
{
	return PendingReviewChangedDelegate.Add(Handler);
}

void FBlueprintHelperReviewStoreService::RemovePendingReviewChangedHandler(FDelegateHandle Handle) const
{
	if (Handle.IsValid())
	{
		PendingReviewChangedDelegate.Remove(Handle);
	}
}

void FBlueprintHelperReviewStoreService::NotifyPendingReviewChanged() const
{
	PendingReviewChangedDelegate.Broadcast();
}

FBlueprintHelperReviewVisibleChange FBlueprintHelperReviewStoreService::MakeVisibleChange(
	const FBlueprintHelperReviewTransactionInput& Input,
	const FString& ChangeIdSuffix) const
{
	FBlueprintHelperReviewVisibleChange Change;
	Change.AssetPath = Input.AssetPath;
	Change.GraphName = Input.GraphName;
	Change.LocationKey = Input.LocationKey;
	Change.LatestTransactionId = Input.TransactionId;
	Change.LatestTransactionIds.Add(Input.TransactionId);
	Change.SourceTransactionIds.Add(Input.TransactionId);
	Change.ChangeKind = Input.ChangeKind;
	Change.DisplayLabel = Input.DisplayLabel.IsEmpty() ? Input.LocationKey : Input.DisplayLabel;
	Change.BeforeSummary = Input.BeforeSummary;
	Change.AfterSummary = Input.AfterSummary;
	Change.ChangeId = Input.TransactionId;
	if (!ChangeIdSuffix.IsEmpty())
	{
		Change.ChangeId += TEXT("_") + ChangeIdSuffix;
	}
	return Change;
}

void FBlueprintHelperReviewStoreService::GroupAtomicVisibleChange(
	const FBlueprintHelperReviewVisibleChange& AtomicChange,
	TMap<FString, int32>& GroupToIndex,
	TArray<FBlueprintHelperReviewVisibleChange>& OutChanges) const
{
	if (AtomicChange.AtomicTargets.Num() == 0)
	{
		return;
	}

	const FBlueprintHelperReviewAtomicTarget& Target = AtomicChange.AtomicTargets[0];
	const FString GroupKey = FString::Printf(
		TEXT("%s|%s"),
		*AtomicChange.AssetPath,
		*Target.VisualGroupKey);

	if (const int32* ExistingIndex = GroupToIndex.Find(GroupKey))
	{
		FBlueprintHelperReviewVisibleChange& Existing = OutChanges[*ExistingIndex];
		Existing.AtomicTargets.Add(Target);
		for (const FString& SourceTransactionId : AtomicChange.SourceTransactionIds)
		{
			Existing.SourceTransactionIds.AddUnique(SourceTransactionId);
		}
		for (const FString& LatestTransactionId : AtomicChange.LatestTransactionIds)
		{
			Existing.LatestTransactionIds.AddUnique(LatestTransactionId);
		}
		Existing.LatestTransactionId = AtomicChange.LatestTransactionId;
		Existing.ChangeKind = AtomicChange.ChangeKind;
		Existing.GraphName = AtomicChange.GraphName.IsEmpty() ? Existing.GraphName : AtomicChange.GraphName;
		Existing.DisplayLabel = AtomicChange.DisplayLabel.IsEmpty() ? Existing.DisplayLabel : AtomicChange.DisplayLabel;
		Existing.AfterSummary = AtomicChange.AfterSummary;
		Existing.ChangeId = AtomicChange.ChangeId;
		Existing.ExecutionOrder = AtomicChange.ExecutionOrder;
		Existing.TaskStepIndex = AtomicChange.TaskStepIndex;
		Existing.AtomicIndex = AtomicChange.AtomicIndex;
		FBlueprintHelperReviewStoreTargetUtils::ApplyAssetLifecycleRootMetadata(Existing);
		return;
	}

	const int32 NewIndex = OutChanges.Add(AtomicChange);
	GroupToIndex.Add(GroupKey, NewIndex);
}

void FBlueprintHelperReviewStoreService::AddEvidenceAtomicTargets(
	const FBlueprintHelperWriteReviewEvidence& Evidence,
	FBlueprintHelperReviewRecord& Record) const
{
	if (Evidence.AtomicTargets.Num() == 0)
	{
		FBlueprintHelperReviewVisibleChange Change;
		Change.ChangeId = Evidence.TransactionId;
		Change.AssetPath = Evidence.AssetPath;
		Change.LocationKey = Evidence.OperationKind;
		Change.LatestTransactionId = Evidence.TransactionId;
		Change.SourceTransactionIds.Add(Evidence.TransactionId);
		Change.ChangeKind = Evidence.ChangeKind;
		Change.Status = EBlueprintHelperReviewChangeStatus::NeedsAction;
		Change.DisplayLabel = Evidence.DisplayLabel.IsEmpty() ? Evidence.OperationKind : Evidence.DisplayLabel;
			Change.NeedsActionReason = TEXT("missing_atomic_targets");
			Record.VisibleChanges.Add(Change);
			return;
	}

	for (int32 Index = 0; Index < Evidence.AtomicTargets.Num(); ++Index)
	{
		FBlueprintHelperReviewAtomicTarget Target = Evidence.AtomicTargets[Index];
		Target.AssetPath = Target.AssetPath.IsEmpty() ? Evidence.AssetPath : Target.AssetPath;
		Target.TaskStepIndex = Evidence.TaskStepIndex;
		Target.AtomicIndex = Index;
		if (Target.ExecutionOrder == INDEX_NONE && Target.TaskStepIndex != INDEX_NONE)
		{
			Target.ExecutionOrder = Target.TaskStepIndex * 1000 + Index;
		}
		Target.ScopeIdentity = FBlueprintHelperReviewStoreTargetUtils::MakeReviewScopeIdentity(
			Target,
			FBlueprintHelperReviewStoreTargetUtils::MakeReviewInternalMissingAnchorKey(Evidence.TransactionId, Index));
		Target.FirstTransactionId = Target.FirstTransactionId.IsEmpty()
			? Evidence.TransactionId
			: Target.FirstTransactionId;
		Target.AfterSnapshotJson = Target.AfterSnapshotJson.IsEmpty()
			? Target.AnchorJson
			: Target.AfterSnapshotJson;
		Record.SourceTransactionSummary.AssetPaths.AddUnique(Target.AssetPath);
		Target.LatestTransactionId = Evidence.TransactionId;
		Target.SourceTransactionIds.AddUnique(Evidence.TransactionId);
		if (Target.Ownership.IsEmpty())
		{
			Target.Ownership = TEXT("unknown");
		}
		Target.Surface = BlueprintHelperReviewNormalizeSurfaceForTarget(
			Target.Surface,
			Target.TargetKind,
			Target.TargetKey,
			Target.VisualGroupKey,
			Evidence.OperationKind);
		FBlueprintHelperReviewStoreTargetUtils::ApplyGraphBodyAggregation(Target);
		NormalizeReviewTargetSemanticSnapshots(Evidence, Target);

		FString NeedsActionReason;
		if (!FBlueprintHelperReviewStoreTargetUtils::IsReviewEvidenceTargetComplete(Target, NeedsActionReason))
		{
			Target.Status = EBlueprintHelperReviewChangeStatus::NeedsAction;
		}

		const FString VisualGroupKey = Target.VisualGroupKey.IsEmpty()
			? FBlueprintHelperReviewStoreTargetUtils::MakeReviewInternalMissingGroupKey(Evidence.TransactionId, Index)
			: Target.VisualGroupKey;
		const FString ChangeAssetPath = Target.AssetPath.IsEmpty() ? Evidence.AssetPath : Target.AssetPath;
		const FString AtomicLookupKey = FBlueprintHelperReviewStoreTargetUtils::MakeReviewAtomicLookupKey(
			Target,
			FBlueprintHelperReviewStoreTargetUtils::MakeReviewInternalMissingAnchorKey(Evidence.TransactionId, Index));

		FBlueprintHelperReviewVisibleChange* Change = Record.VisibleChanges.FindByPredicate(
			[&VisualGroupKey, &ChangeAssetPath](const FBlueprintHelperReviewVisibleChange& Candidate)
			{
				return Candidate.LocationKey == VisualGroupKey
					&& Candidate.AssetPath == ChangeAssetPath;
			});
		if (!Change)
		{
			FBlueprintHelperReviewVisibleChange NewChange = FBlueprintHelperReviewStoreTargetUtils::MakeVisibleChangeFromEvidence(
				Evidence,
				Target,
				VisualGroupKey);
			NewChange.AtomicTargets.Add(Target);
			NewChange.ScopeIdentity = Target.ScopeIdentity;
			NewChange.BeforeHash = Target.BaselineHash;
			NewChange.AfterHash = Target.RecordedAfterHash;
			NewChange.BeforeSnapshotJson = Target.BeforeSnapshotJson;
			NewChange.AfterSnapshotJson = Target.AfterSnapshotJson;
			FBlueprintHelperReviewStoreTargetUtils::ApplyAssetLifecycleRootMetadata(NewChange);
			if (!NeedsActionReason.IsEmpty())
			{
				NewChange.Status = EBlueprintHelperReviewChangeStatus::NeedsAction;
				NewChange.NeedsActionReason = NeedsActionReason;
			}
			Record.VisibleChanges.Add(NewChange);
			continue;
		}

		FBlueprintHelperReviewAtomicTarget* ExistingTarget = Change->AtomicTargets.FindByPredicate(
			[&AtomicLookupKey](const FBlueprintHelperReviewAtomicTarget& Candidate)
			{
				return FBlueprintHelperReviewStoreTargetUtils::MakeReviewAtomicLookupKey(Candidate, FString()) == AtomicLookupKey;
			});
		if (!ExistingTarget)
		{
			Change->AtomicTargets.Add(Target);
		}
		else
		{
			TArray<FString> SourceTransactionIds = ExistingTarget->SourceTransactionIds;
			for (const FString& SourceTransactionId : Target.SourceTransactionIds)
			{
				SourceTransactionIds.AddUnique(SourceTransactionId);
			}

			FBlueprintHelperReviewAtomicTarget MergedTarget = Target;
			FBlueprintHelperReviewStoreTargetUtils::PreserveFirstBaselineFields(MergedTarget, *ExistingTarget, Target);
			MergedTarget.SourceTransactionIds = SourceTransactionIds;
			*ExistingTarget = MergedTarget;
		}

		const FString OriginalBeforeSummary = Change->BeforeSummary;
		const FString OriginalBeforeHash = Change->BeforeHash;
		const FString OriginalBeforeSnapshotJson = Change->BeforeSnapshotJson;
		Change->LatestTransactionId = Evidence.TransactionId;
		Change->LatestTransactionIds.Reset();
		Change->LatestTransactionIds.Add(Evidence.TransactionId);
		Change->SourceTransactionIds.AddUnique(Evidence.TransactionId);
		Change->ChangeId = FBlueprintHelperReviewStoreTargetUtils::MakeReviewVisibleChangeId(Evidence.TransactionId, VisualGroupKey);
		Change->ScopeIdentity = Change->ScopeIdentity.IsEmpty() ? Target.ScopeIdentity : Change->ScopeIdentity;
		Change->ChangeKind = Evidence.ChangeKind;
		Change->BeforeSummary = OriginalBeforeSummary.IsEmpty() ? Evidence.BeforeSummary : OriginalBeforeSummary;
		Change->AfterSummary = Evidence.AfterSummary;
		Change->BeforeHash = OriginalBeforeHash.IsEmpty() ? Target.BaselineHash : OriginalBeforeHash;
		Change->AfterHash = Target.RecordedAfterHash;
		Change->BeforeSnapshotJson = OriginalBeforeSnapshotJson.IsEmpty() ? Target.BeforeSnapshotJson : OriginalBeforeSnapshotJson;
		Change->AfterSnapshotJson = Target.AfterSnapshotJson;
		Change->ExecutionOrder = Target.ExecutionOrder;
		Change->TaskStepIndex = Target.TaskStepIndex;
		Change->AtomicIndex = Target.AtomicIndex;
		FBlueprintHelperReviewStoreTargetUtils::ApplyAssetLifecycleRootMetadata(*Change);
		if (!NeedsActionReason.IsEmpty())
		{
			Change->Status = EBlueprintHelperReviewChangeStatus::NeedsAction;
			Change->NeedsActionReason = NeedsActionReason;
		}
	}

	FBlueprintHelperReviewStoreMergeUtils::RemoveNetNoChangeVisibleChanges(Record.VisibleChanges);
}

TArray<FBlueprintHelperReviewAtomicTarget> FBlueprintHelperReviewStoreService::MakeAtomicTargetsForInput(
	const FBlueprintHelperReviewTransactionInput& Input) const
{
	if (Input.AtomicTargets.Num() > 0)
	{
		TArray<FBlueprintHelperReviewAtomicTarget> Targets = Input.AtomicTargets;
		for (FBlueprintHelperReviewAtomicTarget& Target : Targets)
		{
			Target.AssetPath = Target.AssetPath.IsEmpty() ? Input.AssetPath : Target.AssetPath;
			Target.GraphName = Target.GraphName.IsEmpty() ? Input.GraphName : Target.GraphName;
			Target.TargetKey = Target.TargetKey.IsEmpty() ? Input.LocationKey : Target.TargetKey;
			Target.VisualGroupKey = Target.VisualGroupKey.IsEmpty() ? Input.LocationKey : Target.VisualGroupKey;
			Target.DisplayLabel = Target.DisplayLabel.IsEmpty() ? Input.DisplayLabel : Target.DisplayLabel;
			Target.SourceTransactionIds.Add(Input.TransactionId);
			Target.Surface = BlueprintHelperReviewNormalizeSurfaceForTarget(
				Target.Surface,
				Target.TargetKind,
				Target.TargetKey,
				Target.VisualGroupKey,
				Input.LocationKey);
			FBlueprintHelperReviewStoreTargetUtils::ApplyGraphBodyAggregation(Target);
		}
		return Targets;
	}

	FBlueprintHelperReviewAtomicTarget Target;
	Target.AssetPath = Input.AssetPath;
	Target.GraphName = Input.GraphName;
	Target.TargetKey = Input.LocationKey.IsEmpty() ? Input.DisplayLabel : Input.LocationKey;
	Target.VisualGroupKey = Target.TargetKey;
	Target.DisplayLabel = Input.DisplayLabel;
	Target.SourceTransactionIds.Add(Input.TransactionId);

	FBlueprintHelperReviewVisibleChange TempChange;
	TempChange.LocationKey = Input.LocationKey;
	TempChange.GraphName = Input.GraphName;
	TempChange.DisplayLabel = Input.DisplayLabel;
	TempChange.ChangeKind = Input.ChangeKind;
	if (BlueprintHelperReviewShouldShowInComponents(TempChange))
	{
		Target.Surface = EBlueprintHelperReviewSurface::Components;
	}
	else if (BlueprintHelperReviewShouldShowInDetails(TempChange))
	{
		Target.Surface = EBlueprintHelperReviewSurface::Details;
	}
	else if (BlueprintHelperReviewShouldShowInGraph(TempChange))
	{
		Target.Surface = EBlueprintHelperReviewSurface::Graph;
	}
	else if (BlueprintHelperReviewShouldShowInMyBlueprint(TempChange))
	{
		Target.Surface = EBlueprintHelperReviewSurface::MyBlueprint;
	}
	else
	{
		Target.Surface = EBlueprintHelperReviewSurface::Graph;
	}
	Target.Surface = BlueprintHelperReviewNormalizeSurfaceForTarget(
		Target.Surface,
		Target.TargetKind,
		Target.TargetKey,
		Target.VisualGroupKey,
		Input.LocationKey);
	FBlueprintHelperReviewStoreTargetUtils::ApplyGraphBodyAggregation(Target);

	TArray<FBlueprintHelperReviewAtomicTarget> Targets;
	Targets.Add(Target);
	return Targets;
}
