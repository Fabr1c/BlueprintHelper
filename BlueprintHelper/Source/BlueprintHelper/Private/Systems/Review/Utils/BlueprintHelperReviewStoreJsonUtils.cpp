// BlueprintHelper Review BlueprintHelperReviewStoreJsonUtils implementation.

#include "Systems/Review/Utils/BlueprintHelperReviewStoreJsonUtils.h"

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
#include "Systems/Review/Utils/BlueprintHelperReviewStoreTargetUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentEvidence.h"

EBlueprintHelperReviewChangeStatus FBlueprintHelperReviewStoreJsonUtils::ParseReviewChangeStatus(const FString& Status)
	{
		return FBlueprintHelperReviewEnumUtils::ParseChangeStatus(Status);
	}
EBlueprintHelperReviewChangeKind FBlueprintHelperReviewStoreJsonUtils::ParseReviewChangeKind(const FString& ChangeKind)
	{
		return FBlueprintHelperReviewEnumUtils::ParseChangeKind(ChangeKind);
	}
EBlueprintHelperReviewSurface FBlueprintHelperReviewStoreJsonUtils::ParseReviewSurface(const FString& Surface)
	{
		return FBlueprintHelperReviewEnumUtils::ParseSurface(Surface);
	}
void FBlueprintHelperReviewStoreJsonUtils::ReadReviewStringArray(
		const TSharedPtr<FJsonObject>& Json,
		const TCHAR* FieldName,
		TArray<FString>& OutValues)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Json.IsValid() || !Json->TryGetArrayField(FieldName, Values) || !Values)
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			if (!Value.IsValid())
			{
				continue;
			}
			FString StringValue;
			if (Value->TryGetString(StringValue))
			{
				OutValues.AddUnique(StringValue);
			}
		}
	}
TArray<TSharedPtr<FJsonValue>> FBlueprintHelperReviewStoreJsonUtils::MakeReviewJsonStringArray(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> JsonValues;
		for (const FString& Value : Values)
		{
			JsonValues.Add(MakeShared<FJsonValueString>(Value));
		}
		return JsonValues;
	}
TSharedRef<FJsonObject> FBlueprintHelperReviewStoreJsonUtils::MakeReviewJsonVector2D(const FVector2D& Value)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("x"), Value.X);
		Json->SetNumberField(TEXT("y"), Value.Y);
		return Json;
	}
bool FBlueprintHelperReviewStoreJsonUtils::ReadReviewJsonVector2D(
		const TSharedPtr<FJsonObject>& Json,
		const TCHAR* FieldName,
		FVector2D& OutValue)
	{
		const TSharedPtr<FJsonObject>* VectorJson = nullptr;
		if (!Json.IsValid() ||
			!Json->TryGetObjectField(FieldName, VectorJson) ||
			!VectorJson ||
			!VectorJson->IsValid())
		{
			return false;
		}

		double X = 0.0;
		double Y = 0.0;
		if (!(*VectorJson)->TryGetNumberField(TEXT("x"), X) ||
			!(*VectorJson)->TryGetNumberField(TEXT("y"), Y))
		{
			return false;
		}

		OutValue = FVector2D(X, Y);
		return true;
	}
TSharedRef<FJsonObject> FBlueprintHelperReviewStoreJsonUtils::ReviewAtomicTargetToJson(const FBlueprintHelperReviewAtomicTarget& Target)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("asset_path"), Target.AssetPath);
		Json->SetStringField(TEXT("surface"), BlueprintHelperReviewSurfaceToString(Target.Surface));
		if (!Target.GraphName.IsEmpty()) Json->SetStringField(TEXT("graph_name"), Target.GraphName);
		Json->SetStringField(TEXT("target_key"), Target.TargetKey);
		if (!Target.TargetKind.IsEmpty()) Json->SetStringField(TEXT("target_kind"), Target.TargetKind);
		if (!Target.TargetSubKind.IsEmpty()) Json->SetStringField(TEXT("target_subkind"), Target.TargetSubKind);
		if (!Target.SignatureRole.IsEmpty()) Json->SetStringField(TEXT("signature_role"), Target.SignatureRole);
		if (!Target.SignatureEvidenceId.IsEmpty()) Json->SetStringField(TEXT("signature_evidence_id"), Target.SignatureEvidenceId);
		if (!Target.DependencyOwnerStepId.IsEmpty()) Json->SetStringField(TEXT("dependency_owner_step_id"), Target.DependencyOwnerStepId);
		if (!Target.DependentStepId.IsEmpty()) Json->SetStringField(TEXT("dependent_step_id"), Target.DependentStepId);
		if (!Target.VisualGroupKey.IsEmpty()) Json->SetStringField(TEXT("visual_group_key"), Target.VisualGroupKey);
		if (!Target.DisplayLabel.IsEmpty()) Json->SetStringField(TEXT("display_label"), Target.DisplayLabel);
		if (!Target.FirstEvidenceId.IsEmpty()) Json->SetStringField(TEXT("first_evidence_id"), Target.FirstEvidenceId);
		if (!Target.LatestEvidenceId.IsEmpty()) Json->SetStringField(TEXT("latest_evidence_id"), Target.LatestEvidenceId);
		Json->SetArrayField(TEXT("source_evidence_ids"), MakeReviewJsonStringArray(Target.SourceEvidenceIds));
		if (!Target.Ownership.IsEmpty()) Json->SetStringField(TEXT("ownership"), Target.Ownership);
		if (!Target.NodeGuid.IsEmpty()) Json->SetStringField(TEXT("node_guid"), Target.NodeGuid);
		if (!Target.PinPath.IsEmpty()) Json->SetStringField(TEXT("pin_path"), Target.PinPath);
		if (!Target.PropertyPath.IsEmpty()) Json->SetStringField(TEXT("property_path"), Target.PropertyPath);
		if (!Target.ComponentPath.IsEmpty()) Json->SetStringField(TEXT("component_path"), Target.ComponentPath);
		if (!Target.LifecycleObjectKey.IsEmpty()) Json->SetStringField(TEXT("lifecycle_object_key"), Target.LifecycleObjectKey);
		if (!Target.LifecycleParentKey.IsEmpty()) Json->SetStringField(TEXT("lifecycle_parent_key"), Target.LifecycleParentKey);
		if (!Target.AnchorJson.IsEmpty()) Json->SetStringField(TEXT("anchor"), Target.AnchorJson);
		if (!Target.RecordedAfterHash.IsEmpty()) Json->SetStringField(TEXT("recorded_after_hash"), Target.RecordedAfterHash);
		if (!Target.BaselineHash.IsEmpty()) Json->SetStringField(TEXT("baseline_hash"), Target.BaselineHash);
		if (!Target.BeforeSnapshotJson.IsEmpty()) Json->SetStringField(TEXT("before_snapshot_json"), Target.BeforeSnapshotJson);
		if (!Target.AfterSnapshotJson.IsEmpty()) Json->SetStringField(TEXT("after_snapshot_json"), Target.AfterSnapshotJson);
		if (Target.ExecutionOrder != INDEX_NONE) Json->SetNumberField(TEXT("execution_order"), Target.ExecutionOrder);
		if (Target.TaskStepIndex != INDEX_NONE) Json->SetNumberField(TEXT("task_step_index"), Target.TaskStepIndex);
		if (Target.AtomicIndex != INDEX_NONE) Json->SetNumberField(TEXT("atomic_index"), Target.AtomicIndex);
		if (Target.bHasGraphBounds)
		{
			Json->SetBoolField(TEXT("has_graph_bounds"), true);
			Json->SetObjectField(TEXT("graph_position"), MakeReviewJsonVector2D(Target.GraphPosition));
			Json->SetObjectField(TEXT("graph_size"), MakeReviewJsonVector2D(Target.GraphSize));
		}
		Json->SetStringField(TEXT("status"), BlueprintHelperReviewChangeStatusToString(Target.Status));
		return Json;
	}
TSharedRef<FJsonObject> FBlueprintHelperReviewStoreJsonUtils::ReviewVisibleChangeToJson(const FBlueprintHelperReviewVisibleChange& Change)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("change_id"), Change.ChangeId);
		Json->SetStringField(TEXT("asset_path"), Change.AssetPath);
		if (!Change.GraphName.IsEmpty()) Json->SetStringField(TEXT("graph_name"), Change.GraphName);
		Json->SetStringField(TEXT("visual_group_key"), Change.LocationKey);
		Json->SetStringField(TEXT("latest_evidence_id"), Change.LatestEvidenceId);
		Json->SetArrayField(TEXT("latest_evidence_ids"), MakeReviewJsonStringArray(Change.LatestEvidenceIds));
		Json->SetArrayField(TEXT("source_evidence_ids"), MakeReviewJsonStringArray(Change.SourceEvidenceIds));
		Json->SetStringField(TEXT("change_kind"), BlueprintHelperReviewChangeKindToString(Change.ChangeKind));
		Json->SetStringField(TEXT("status"), BlueprintHelperReviewChangeStatusToString(Change.Status));
		if (!Change.DisplayLabel.IsEmpty()) Json->SetStringField(TEXT("display_label"), Change.DisplayLabel);
		if (!Change.BeforeSummary.IsEmpty()) Json->SetStringField(TEXT("before_summary"), Change.BeforeSummary);
		if (!Change.BeforeHash.IsEmpty()) Json->SetStringField(TEXT("before_hash"), Change.BeforeHash);
		if (!Change.BeforeSnapshotJson.IsEmpty()) Json->SetStringField(TEXT("before_snapshot_json"), Change.BeforeSnapshotJson);
		if (!Change.AfterSummary.IsEmpty()) Json->SetStringField(TEXT("after_summary"), Change.AfterSummary);
		if (!Change.AfterHash.IsEmpty()) Json->SetStringField(TEXT("after_hash"), Change.AfterHash);
		if (!Change.AfterSnapshotJson.IsEmpty()) Json->SetStringField(TEXT("after_snapshot_json"), Change.AfterSnapshotJson);
		if (!Change.NeedsActionReason.IsEmpty()) Json->SetStringField(TEXT("needs_action_reason"), Change.NeedsActionReason);
		if (!Change.ParentChangeId.IsEmpty()) Json->SetStringField(TEXT("parent_change_id"), Change.ParentChangeId);
		if (Change.ExecutionOrder != INDEX_NONE) Json->SetNumberField(TEXT("execution_order"), Change.ExecutionOrder);
		if (Change.TaskStepIndex != INDEX_NONE) Json->SetNumberField(TEXT("task_step_index"), Change.TaskStepIndex);
		if (Change.AtomicIndex != INDEX_NONE) Json->SetNumberField(TEXT("atomic_index"), Change.AtomicIndex);
		if (Change.bIsAssetLifecycleRoot) Json->SetBoolField(TEXT("is_asset_lifecycle_root"), true);
		if (Change.bRejectRemovesChildren) Json->SetBoolField(TEXT("reject_removes_children"), true);

		TArray<TSharedPtr<FJsonValue>> Targets;
		for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			Targets.Add(MakeShared<FJsonValueObject>(ReviewAtomicTargetToJson(Target)));
		}
		Json->SetArrayField(TEXT("atomic_targets"), Targets);
		return Json;
	}
TSharedRef<FJsonObject> FBlueprintHelperReviewStoreJsonUtils::ReviewActionRecordToJson(const FBlueprintHelperReviewActionRecord& Action)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("action"), Action.Action);
		Json->SetArrayField(TEXT("target_keys"), MakeReviewJsonStringArray(Action.TargetKeys));
		if (!Action.OwnershipPolicy.IsEmpty()) Json->SetStringField(TEXT("ownership_policy"), Action.OwnershipPolicy);
		if (!Action.CreatedAt.IsEmpty()) Json->SetStringField(TEXT("created_at"), Action.CreatedAt);
		if (!Action.SourceEvidenceId.IsEmpty()) Json->SetStringField(TEXT("source_evidence_id"), Action.SourceEvidenceId);
		if (!Action.Message.IsEmpty()) Json->SetStringField(TEXT("message"), Action.Message);
		return Json;
	}
TSharedRef<FJsonObject> FBlueprintHelperReviewStoreJsonUtils::ReviewRecordToJson(const FBlueprintHelperReviewRecord& Record)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), Record.Schema);
		Json->SetStringField(TEXT("review_record_id"), Record.ReviewRecordId);
		Json->SetStringField(TEXT("archive_session_id"), Record.ArchiveSessionId);
		Json->SetStringField(TEXT("asset_path"), Record.AssetPath);
		Json->SetArrayField(TEXT("source_task_run_ids"), MakeReviewJsonStringArray(Record.SourceTaskRunIds));
		Json->SetStringField(TEXT("status"), BlueprintHelperReviewChangeStatusToString(Record.Status));
		Json->SetStringField(TEXT("storage_status"), BlueprintHelperReviewStorageStatusToString(Record.StorageStatus));

		TArray<TSharedPtr<FJsonValue>> Changes;
		for (const FBlueprintHelperReviewVisibleChange& Change : Record.VisibleChanges)
		{
			Changes.Add(MakeShared<FJsonValueObject>(ReviewVisibleChangeToJson(Change)));
		}
		Json->SetArrayField(TEXT("visible_changes"), Changes);

		TArray<TSharedPtr<FJsonValue>> Actions;
		for (const FBlueprintHelperReviewActionRecord& Action : Record.ReviewActions)
		{
			Actions.Add(MakeShared<FJsonValueObject>(ReviewActionRecordToJson(Action)));
		}
		Json->SetArrayField(TEXT("review_actions"), Actions);

		TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
		Summary->SetNumberField(TEXT("evidence_count"), Record.SourceReviewSummary.EvidenceCount);
		Summary->SetArrayField(TEXT("task_run_ids"), MakeReviewJsonStringArray(Record.SourceReviewSummary.TaskRunIds));
		Summary->SetArrayField(TEXT("operation_kinds"), MakeReviewJsonStringArray(Record.SourceReviewSummary.OperationKinds));
		Summary->SetArrayField(TEXT("asset_paths"), MakeReviewJsonStringArray(Record.SourceReviewSummary.AssetPaths));
		Summary->SetArrayField(TEXT("evidence_ids"), MakeReviewJsonStringArray(Record.SourceReviewSummary.EvidenceIds));
		if (!Record.SourceReviewSummary.CreatedAtFirst.IsEmpty())
		{
			Summary->SetStringField(TEXT("created_at_first"), Record.SourceReviewSummary.CreatedAtFirst);
		}
		if (!Record.SourceReviewSummary.CreatedAtLast.IsEmpty())
		{
			Summary->SetStringField(TEXT("created_at_last"), Record.SourceReviewSummary.CreatedAtLast);
		}
		Summary->SetStringField(TEXT("final_review_status"),
			BlueprintHelperReviewChangeStatusToString(Record.SourceReviewSummary.FinalReviewStatus));
		Json->SetObjectField(TEXT("source_review_summary"), Summary);

		TSharedRef<FJsonObject> Diagnostics = MakeShared<FJsonObject>();
		Diagnostics->SetArrayField(TEXT("debug_case_ids"), MakeReviewJsonStringArray(Record.DebugCaseIds));
		Json->SetObjectField(TEXT("diagnostics"), Diagnostics);
		return Json;
	}
TSharedRef<FJsonObject> FBlueprintHelperReviewStoreJsonUtils::ReviewArchiveSessionToJson(const FBlueprintHelperReviewArchiveSession& ArchiveSession)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), ArchiveSession.Schema);
		Json->SetStringField(TEXT("archive_session_id"), ArchiveSession.ArchiveSessionId);
		Json->SetStringField(TEXT("task_run_id"), ArchiveSession.TaskRunId);
		Json->SetArrayField(TEXT("allowed_target_assets"), MakeReviewJsonStringArray(ArchiveSession.AllowedTargetAssets));
		Json->SetArrayField(TEXT("baseline_snapshot_refs"), MakeReviewJsonStringArray(ArchiveSession.BaselineSnapshotRefs));
		Json->SetArrayField(TEXT("baseline_semantic_snapshot_refs"), MakeReviewJsonStringArray(ArchiveSession.BaselineSemanticSnapshotRefs));
		TSharedRef<FJsonObject> Baseline = MakeShared<FJsonObject>();
		if (!ArchiveSession.BaselineDirtyAssetPolicy.IsEmpty())
		{
			Baseline->SetStringField(TEXT("dirty_asset_policy"), ArchiveSession.BaselineDirtyAssetPolicy);
		}
		if (!ArchiveSession.BaselineSnapshotTrust.IsEmpty())
		{
			Baseline->SetStringField(TEXT("snapshot_trust"), ArchiveSession.BaselineSnapshotTrust);
		}
		Baseline->SetArrayField(TEXT("dirty_target_assets"), MakeReviewJsonStringArray(ArchiveSession.DirtyTargetAssets));
		Baseline->SetArrayField(TEXT("warnings"), MakeReviewJsonStringArray(ArchiveSession.BaselineWarnings));
		Baseline->SetArrayField(TEXT("disk_snapshot_refs"), MakeReviewJsonStringArray(ArchiveSession.BaselineSnapshotRefs));
		Baseline->SetArrayField(TEXT("semantic_snapshot_refs"), MakeReviewJsonStringArray(ArchiveSession.BaselineSemanticSnapshotRefs));
		Json->SetObjectField(TEXT("baseline"), Baseline);
		if (!ArchiveSession.CreatedAt.IsEmpty())
		{
			Json->SetStringField(TEXT("created_at"), ArchiveSession.CreatedAt);
		}
		return Json;
	}
bool FBlueprintHelperReviewStoreJsonUtils::ReadReviewArchiveSessionFromJson(
		const TSharedPtr<FJsonObject>& Json,
		FBlueprintHelperReviewArchiveSession& OutArchiveSession)
	{
		if (!Json.IsValid())
		{
			return false;
		}

		FString Schema;
		Json->TryGetStringField(TEXT("schema"), Schema);
		if (Schema != TEXT("BlueprintHelper.ArchiveSession.v2"))
		{
			return false;
		}

		OutArchiveSession = FBlueprintHelperReviewArchiveSession();
		OutArchiveSession.Schema = Schema;
		Json->TryGetStringField(TEXT("archive_session_id"), OutArchiveSession.ArchiveSessionId);
		Json->TryGetStringField(TEXT("task_run_id"), OutArchiveSession.TaskRunId);
		Json->TryGetStringField(TEXT("created_at"), OutArchiveSession.CreatedAt);
		ReadReviewStringArray(Json, TEXT("allowed_target_assets"), OutArchiveSession.AllowedTargetAssets);
		ReadReviewStringArray(Json, TEXT("baseline_snapshot_refs"), OutArchiveSession.BaselineSnapshotRefs);
		ReadReviewStringArray(Json, TEXT("baseline_semantic_snapshot_refs"), OutArchiveSession.BaselineSemanticSnapshotRefs);

		const TSharedPtr<FJsonObject>* BaselineJson = nullptr;
		if (Json->TryGetObjectField(TEXT("baseline"), BaselineJson) && BaselineJson && BaselineJson->IsValid())
		{
			(*BaselineJson)->TryGetStringField(TEXT("dirty_asset_policy"), OutArchiveSession.BaselineDirtyAssetPolicy);
			(*BaselineJson)->TryGetStringField(TEXT("snapshot_trust"), OutArchiveSession.BaselineSnapshotTrust);
			ReadReviewStringArray(*BaselineJson, TEXT("dirty_target_assets"), OutArchiveSession.DirtyTargetAssets);
			ReadReviewStringArray(*BaselineJson, TEXT("warnings"), OutArchiveSession.BaselineWarnings);
		}
		return !OutArchiveSession.ArchiveSessionId.IsEmpty();
	}
bool FBlueprintHelperReviewStoreJsonUtils::ReadReviewRecordFromJson(const TSharedPtr<FJsonObject>& Json, FBlueprintHelperReviewRecord& OutRecord)
	{
		if (!Json.IsValid())
		{
			return false;
		}

		FString Schema;
		Json->TryGetStringField(TEXT("schema"), Schema);
		if (Schema != TEXT("BlueprintHelper.ReviewRecord.v2"))
		{
			return false;
		}

		OutRecord = FBlueprintHelperReviewRecord();
		OutRecord.Schema = Schema;
		Json->TryGetStringField(TEXT("review_record_id"), OutRecord.ReviewRecordId);
		Json->TryGetStringField(TEXT("archive_session_id"), OutRecord.ArchiveSessionId);
		Json->TryGetStringField(TEXT("asset_path"), OutRecord.AssetPath);
		ReadReviewStringArray(Json, TEXT("source_task_run_ids"), OutRecord.SourceTaskRunIds);
		FString Status;
		Json->TryGetStringField(TEXT("status"), Status);
		OutRecord.Status = ParseReviewChangeStatus(Status);

		FString StorageStatus;
		Json->TryGetStringField(TEXT("storage_status"), StorageStatus);
		OutRecord.StorageStatus = FBlueprintHelperReviewEnumUtils::ParseStorageStatus(StorageStatus);

		const TArray<TSharedPtr<FJsonValue>>* Changes = nullptr;
		if (Json->TryGetArrayField(TEXT("visible_changes"), Changes) && Changes)
		{
			for (const TSharedPtr<FJsonValue>& ChangeValue : *Changes)
			{
				const TSharedPtr<FJsonObject> ChangeJson = ChangeValue.IsValid()
					? ChangeValue->AsObject()
					: nullptr;
				if (!ChangeJson.IsValid())
				{
					continue;
				}

				FBlueprintHelperReviewVisibleChange Change;
				ChangeJson->TryGetStringField(TEXT("change_id"), Change.ChangeId);
				ChangeJson->TryGetStringField(TEXT("asset_path"), Change.AssetPath);
				ChangeJson->TryGetStringField(TEXT("graph_name"), Change.GraphName);
				ChangeJson->TryGetStringField(TEXT("visual_group_key"), Change.LocationKey);
				ChangeJson->TryGetStringField(TEXT("latest_evidence_id"), Change.LatestEvidenceId);
				ReadReviewStringArray(ChangeJson, TEXT("latest_evidence_ids"), Change.LatestEvidenceIds);
				ReadReviewStringArray(ChangeJson, TEXT("source_evidence_ids"), Change.SourceEvidenceIds);
				FString ChangeStatus;
				ChangeJson->TryGetStringField(TEXT("status"), ChangeStatus);
				Change.Status = ParseReviewChangeStatus(ChangeStatus);
				FString ChangeKind;
				ChangeJson->TryGetStringField(TEXT("change_kind"), ChangeKind);
				Change.ChangeKind = ParseReviewChangeKind(ChangeKind);
				ChangeJson->TryGetStringField(TEXT("display_label"), Change.DisplayLabel);
				ChangeJson->TryGetStringField(TEXT("before_summary"), Change.BeforeSummary);
				ChangeJson->TryGetStringField(TEXT("before_hash"), Change.BeforeHash);
				ChangeJson->TryGetStringField(TEXT("before_snapshot_json"), Change.BeforeSnapshotJson);
				ChangeJson->TryGetStringField(TEXT("after_summary"), Change.AfterSummary);
				ChangeJson->TryGetStringField(TEXT("after_hash"), Change.AfterHash);
				ChangeJson->TryGetStringField(TEXT("after_snapshot_json"), Change.AfterSnapshotJson);
				ChangeJson->TryGetStringField(TEXT("needs_action_reason"), Change.NeedsActionReason);
				ChangeJson->TryGetStringField(TEXT("parent_change_id"), Change.ParentChangeId);
				double OrderValue = 0.0;
				if (ChangeJson->TryGetNumberField(TEXT("execution_order"), OrderValue))
				{
					Change.ExecutionOrder = static_cast<int32>(OrderValue);
				}
				if (ChangeJson->TryGetNumberField(TEXT("task_step_index"), OrderValue))
				{
					Change.TaskStepIndex = static_cast<int32>(OrderValue);
				}
				if (ChangeJson->TryGetNumberField(TEXT("atomic_index"), OrderValue))
				{
					Change.AtomicIndex = static_cast<int32>(OrderValue);
				}
				ChangeJson->TryGetBoolField(TEXT("is_asset_lifecycle_root"), Change.bIsAssetLifecycleRoot);
				ChangeJson->TryGetBoolField(TEXT("reject_removes_children"), Change.bRejectRemovesChildren);

				const TArray<TSharedPtr<FJsonValue>>* Targets = nullptr;
				if (ChangeJson->TryGetArrayField(TEXT("atomic_targets"), Targets) && Targets)
				{
					for (const TSharedPtr<FJsonValue>& TargetValue : *Targets)
					{
						const TSharedPtr<FJsonObject> TargetJson = TargetValue.IsValid()
							? TargetValue->AsObject()
							: nullptr;
						if (!TargetJson.IsValid())
						{
							continue;
						}

						FBlueprintHelperReviewAtomicTarget Target;
						TargetJson->TryGetStringField(TEXT("asset_path"), Target.AssetPath);
						FString Surface;
						TargetJson->TryGetStringField(TEXT("surface"), Surface);
						Target.Surface = ParseReviewSurface(Surface);
						TargetJson->TryGetStringField(TEXT("graph_name"), Target.GraphName);
						TargetJson->TryGetStringField(TEXT("target_key"), Target.TargetKey);
						TargetJson->TryGetStringField(TEXT("target_kind"), Target.TargetKind);
						TargetJson->TryGetStringField(TEXT("target_subkind"), Target.TargetSubKind);
						TargetJson->TryGetStringField(TEXT("signature_role"), Target.SignatureRole);
						TargetJson->TryGetStringField(TEXT("signature_evidence_id"), Target.SignatureEvidenceId);
						TargetJson->TryGetStringField(TEXT("dependency_owner_step_id"), Target.DependencyOwnerStepId);
						TargetJson->TryGetStringField(TEXT("dependent_step_id"), Target.DependentStepId);
						TargetJson->TryGetStringField(TEXT("visual_group_key"), Target.VisualGroupKey);
						TargetJson->TryGetStringField(TEXT("display_label"), Target.DisplayLabel);
						TargetJson->TryGetStringField(TEXT("first_evidence_id"), Target.FirstEvidenceId);
						TargetJson->TryGetStringField(TEXT("latest_evidence_id"), Target.LatestEvidenceId);
						ReadReviewStringArray(TargetJson, TEXT("source_evidence_ids"), Target.SourceEvidenceIds);
						TargetJson->TryGetStringField(TEXT("ownership"), Target.Ownership);
						TargetJson->TryGetStringField(TEXT("node_guid"), Target.NodeGuid);
						TargetJson->TryGetStringField(TEXT("pin_path"), Target.PinPath);
						TargetJson->TryGetStringField(TEXT("property_path"), Target.PropertyPath);
						TargetJson->TryGetStringField(TEXT("component_path"), Target.ComponentPath);
						TargetJson->TryGetStringField(TEXT("lifecycle_object_key"), Target.LifecycleObjectKey);
						TargetJson->TryGetStringField(TEXT("lifecycle_parent_key"), Target.LifecycleParentKey);
						TargetJson->TryGetStringField(TEXT("anchor"), Target.AnchorJson);
						TargetJson->TryGetStringField(TEXT("recorded_after_hash"), Target.RecordedAfterHash);
						TargetJson->TryGetStringField(TEXT("baseline_hash"), Target.BaselineHash);
						TargetJson->TryGetStringField(TEXT("before_snapshot_json"), Target.BeforeSnapshotJson);
						TargetJson->TryGetStringField(TEXT("after_snapshot_json"), Target.AfterSnapshotJson);
						double TargetOrderValue = 0.0;
						if (TargetJson->TryGetNumberField(TEXT("execution_order"), TargetOrderValue))
						{
							Target.ExecutionOrder = static_cast<int32>(TargetOrderValue);
						}
						if (TargetJson->TryGetNumberField(TEXT("task_step_index"), TargetOrderValue))
						{
							Target.TaskStepIndex = static_cast<int32>(TargetOrderValue);
						}
						if (TargetJson->TryGetNumberField(TEXT("atomic_index"), TargetOrderValue))
						{
							Target.AtomicIndex = static_cast<int32>(TargetOrderValue);
						}
						TargetJson->TryGetBoolField(TEXT("has_graph_bounds"), Target.bHasGraphBounds);
						ReadReviewJsonVector2D(TargetJson, TEXT("graph_position"), Target.GraphPosition);
						ReadReviewJsonVector2D(TargetJson, TEXT("graph_size"), Target.GraphSize);
						FString TargetStatus;
						TargetJson->TryGetStringField(TEXT("status"), TargetStatus);
						Target.Status = ParseReviewChangeStatus(TargetStatus);
						Target.Surface = BlueprintHelperReviewNormalizeSurfaceForTarget(
							Target.Surface,
							Target.TargetKind,
							Target.TargetKey,
							Target.VisualGroupKey,
							Change.LocationKey);
						Change.AtomicTargets.Add(Target);
					}
				}

				FBlueprintHelperReviewStoreTargetUtils::ApplyAssetLifecycleRootMetadata(Change);
				OutRecord.VisibleChanges.Add(Change);
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* Actions = nullptr;
		if (Json->TryGetArrayField(TEXT("review_actions"), Actions) && Actions)
		{
			for (const TSharedPtr<FJsonValue>& ActionValue : *Actions)
			{
				const TSharedPtr<FJsonObject> ActionJson = ActionValue.IsValid()
					? ActionValue->AsObject()
					: nullptr;
				if (!ActionJson.IsValid())
				{
					continue;
				}

				FBlueprintHelperReviewActionRecord Action;
				ActionJson->TryGetStringField(TEXT("action"), Action.Action);
				ReadReviewStringArray(ActionJson, TEXT("target_keys"), Action.TargetKeys);
				ActionJson->TryGetStringField(TEXT("ownership_policy"), Action.OwnershipPolicy);
				ActionJson->TryGetStringField(TEXT("created_at"), Action.CreatedAt);
				ActionJson->TryGetStringField(TEXT("source_evidence_id"), Action.SourceEvidenceId);
				ActionJson->TryGetStringField(TEXT("message"), Action.Message);
				OutRecord.ReviewActions.Add(Action);
			}
		}

		const TSharedPtr<FJsonObject>* Summary = nullptr;
		if (Json->TryGetObjectField(TEXT("source_review_summary"), Summary) && Summary)
		{
			OutRecord.SourceReviewSummary.EvidenceCount =
				static_cast<int32>((*Summary)->GetNumberField(TEXT("evidence_count")));
			ReadReviewStringArray(*Summary, TEXT("task_run_ids"), OutRecord.SourceReviewSummary.TaskRunIds);
			ReadReviewStringArray(*Summary, TEXT("operation_kinds"), OutRecord.SourceReviewSummary.OperationKinds);
			ReadReviewStringArray(*Summary, TEXT("asset_paths"), OutRecord.SourceReviewSummary.AssetPaths);
			ReadReviewStringArray(*Summary, TEXT("evidence_ids"), OutRecord.SourceReviewSummary.EvidenceIds);
			(*Summary)->TryGetStringField(TEXT("created_at_first"), OutRecord.SourceReviewSummary.CreatedAtFirst);
			(*Summary)->TryGetStringField(TEXT("created_at_last"), OutRecord.SourceReviewSummary.CreatedAtLast);
			FString FinalReviewStatus;
			(*Summary)->TryGetStringField(TEXT("final_review_status"), FinalReviewStatus);
			OutRecord.SourceReviewSummary.FinalReviewStatus = ParseReviewChangeStatus(FinalReviewStatus);
		}

		const TSharedPtr<FJsonObject>* Diagnostics = nullptr;
		if (Json->TryGetObjectField(TEXT("diagnostics"), Diagnostics) && Diagnostics)
		{
			ReadReviewStringArray(*Diagnostics, TEXT("debug_case_ids"), OutRecord.DebugCaseIds);
		}

		return !OutRecord.ReviewRecordId.IsEmpty();
	}

