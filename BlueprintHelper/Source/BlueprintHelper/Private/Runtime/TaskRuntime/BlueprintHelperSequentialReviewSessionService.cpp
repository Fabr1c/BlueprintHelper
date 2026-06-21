// BlueprintHelper Runtime-owned sequential Review session service implementation.

#include "Runtime/TaskRuntime/BlueprintHelperSequentialReviewSessionService.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Systems/Review/BlueprintHelperReviewConfigResolver.h"

class FBlueprintHelperSequentialReviewSessionJsonLocal
{
public:
	static TArray<TSharedPtr<FJsonValue>> MakeStringArray(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> JsonValues;
		for (const FString& Value : Values)
		{
			if (!Value.IsEmpty())
			{
				JsonValues.Add(MakeShared<FJsonValueString>(Value));
			}
		}
		return JsonValues;
	}

	static void ReadStringArray(
		const TSharedPtr<FJsonObject>& Json,
		const FString& FieldName,
		TArray<FString>& OutValues)
	{
		OutValues.Reset();
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Json.IsValid() || !Json->TryGetArrayField(FieldName, Values) || !Values)
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			if (!Value.IsValid() || Value->Type != EJson::String)
			{
				continue;
			}
			const FString StringValue = Value->AsString();
			if (!StringValue.IsEmpty())
			{
				OutValues.AddUnique(StringValue);
			}
		}
	}

	static TSharedRef<FJsonObject> ToJson(
		const FBlueprintHelperSequentialReviewSession& Session)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), Session.Schema);
		Json->SetStringField(TEXT("sequential_review_session_id"), Session.SequentialReviewSessionId);
		Json->SetArrayField(TEXT("target_assets"), MakeStringArray(Session.TargetAssets));
		Json->SetArrayField(TEXT("archive_session_ids"), MakeStringArray(Session.ArchiveSessionIds));
		Json->SetArrayField(TEXT("review_record_ids"), MakeStringArray(Session.ReviewRecordIds));
		Json->SetArrayField(TEXT("source_task_run_ids"), MakeStringArray(Session.SourceTaskRunIds));
		Json->SetStringField(TEXT("session_start_archive_session_id"), Session.SessionStartArchiveSessionId);
		Json->SetStringField(TEXT("last_good_archive_session_id"), Session.LastGoodArchiveSessionId);
		Json->SetStringField(TEXT("last_good_task_run_id"), Session.LastGoodTaskRunId);
		Json->SetStringField(TEXT("last_failure_task_run_id"), Session.LastFailureTaskRunId);
		Json->SetStringField(
			TEXT("status"),
			BlueprintHelperSequentialReviewSessionStatusToString(Session.Status));
		Json->SetBoolField(TEXT("has_last_good_snapshot"), Session.bHasLastGoodSnapshot);
		Json->SetBoolField(TEXT("has_unresolved_failed_execute"), Session.bHasUnresolvedFailedExecute);
		Json->SetBoolField(TEXT("has_external_conflict"), Session.bHasExternalConflict);
		Json->SetStringField(TEXT("created_at"), Session.CreatedAt);
		Json->SetStringField(TEXT("updated_at"), Session.UpdatedAt);
		return Json;
	}

	static bool FromJson(
		const TSharedPtr<FJsonObject>& Json,
		FBlueprintHelperSequentialReviewSession& OutSession)
	{
		if (!Json.IsValid())
		{
			return false;
		}

		OutSession = FBlueprintHelperSequentialReviewSession();
		Json->TryGetStringField(TEXT("schema"), OutSession.Schema);
		Json->TryGetStringField(TEXT("sequential_review_session_id"), OutSession.SequentialReviewSessionId);
		ReadStringArray(Json, TEXT("target_assets"), OutSession.TargetAssets);
		ReadStringArray(Json, TEXT("archive_session_ids"), OutSession.ArchiveSessionIds);
		ReadStringArray(Json, TEXT("review_record_ids"), OutSession.ReviewRecordIds);
		ReadStringArray(Json, TEXT("source_task_run_ids"), OutSession.SourceTaskRunIds);
		Json->TryGetStringField(
			TEXT("session_start_archive_session_id"),
			OutSession.SessionStartArchiveSessionId);
		Json->TryGetStringField(
			TEXT("last_good_archive_session_id"),
			OutSession.LastGoodArchiveSessionId);
		Json->TryGetStringField(TEXT("last_good_task_run_id"), OutSession.LastGoodTaskRunId);
		Json->TryGetStringField(TEXT("last_failure_task_run_id"), OutSession.LastFailureTaskRunId);
		FString Status;
		Json->TryGetStringField(TEXT("status"), Status);
		OutSession.Status = BlueprintHelperSequentialReviewSessionStatusFromString(Status);
		Json->TryGetBoolField(TEXT("has_last_good_snapshot"), OutSession.bHasLastGoodSnapshot);
		Json->TryGetBoolField(
			TEXT("has_unresolved_failed_execute"),
			OutSession.bHasUnresolvedFailedExecute);
		Json->TryGetBoolField(TEXT("has_external_conflict"), OutSession.bHasExternalConflict);
		Json->TryGetStringField(TEXT("created_at"), OutSession.CreatedAt);
		Json->TryGetStringField(TEXT("updated_at"), OutSession.UpdatedAt);
		return !OutSession.SequentialReviewSessionId.IsEmpty();
	}
};

const TCHAR* BlueprintHelperSequentialReviewSessionStatusToString(
	EBlueprintHelperSequentialReviewSessionStatus Status)
{
	switch (Status)
	{
	case EBlueprintHelperSequentialReviewSessionStatus::Active:
		return TEXT("active");
	case EBlueprintHelperSequentialReviewSessionStatus::NeedsRecovery:
		return TEXT("needs_recovery");
	case EBlueprintHelperSequentialReviewSessionStatus::Accepted:
		return TEXT("accepted");
	case EBlueprintHelperSequentialReviewSessionStatus::Rejected:
		return TEXT("rejected");
	default:
		return TEXT("active");
	}
}

EBlueprintHelperSequentialReviewSessionStatus BlueprintHelperSequentialReviewSessionStatusFromString(
	const FString& Value)
{
	if (Value.Equals(TEXT("needs_recovery"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperSequentialReviewSessionStatus::NeedsRecovery;
	}
	if (Value.Equals(TEXT("accepted"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperSequentialReviewSessionStatus::Accepted;
	}
	if (Value.Equals(TEXT("rejected"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperSequentialReviewSessionStatus::Rejected;
	}
	return EBlueprintHelperSequentialReviewSessionStatus::Active;
}

FString FBlueprintHelperSequentialReviewSessionService::MakeSequentialReviewSessionId()
{
	return FString::Printf(
		TEXT("seq_review_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

FBlueprintHelperSequentialReviewSessionLookup
FBlueprintHelperSequentialReviewSessionService::FindOpenSessionForTargetAssets(
	const TArray<FString>& TargetAssets) const
{
	FBlueprintHelperSequentialReviewSessionLookup Lookup;
	if (TargetAssets.Num() == 0)
	{
		return Lookup;
	}

	for (const FBlueprintHelperSequentialReviewSession& Session : QuerySessions())
	{
		const bool bOpen =
			Session.Status == EBlueprintHelperSequentialReviewSessionStatus::Active ||
			Session.Status == EBlueprintHelperSequentialReviewSessionStatus::NeedsRecovery;
		if (bOpen && HasSameTargetSet(TargetAssets, Session.TargetAssets))
		{
			Lookup.bFound = true;
			Lookup.Session = Session;
			return Lookup;
		}
	}
	return Lookup;
}

bool FBlueprintHelperSequentialReviewSessionService::RecordExecuteUpdate(
	const FBlueprintHelperSequentialReviewSessionExecuteUpdate& Update,
	FBlueprintHelperSequentialReviewSession& OutSession,
	FString& OutError) const
{
	OutSession = FBlueprintHelperSequentialReviewSession();
	if (Update.SequentialReviewSessionId.IsEmpty())
	{
		OutError = TEXT("sequential_review_session_id is required");
		return false;
	}

	FBlueprintHelperSequentialReviewSession Session;
	if (Update.bContinuation)
	{
		if (!LoadSession(Update.SequentialReviewSessionId, Session, OutError))
		{
			return false;
		}
	}
	else
	{
		Session.SequentialReviewSessionId = Update.SequentialReviewSessionId;
		Session.SessionStartArchiveSessionId = Update.ArchiveSessionId;
		Session.CreatedAt = FDateTime::UtcNow().ToIso8601();
	}

	for (const FString& TargetAsset : Update.TargetAssets)
	{
		if (!TargetAsset.IsEmpty())
		{
			Session.TargetAssets.AddUnique(TargetAsset);
		}
	}
	if (!Update.TaskRunId.IsEmpty())
	{
		Session.SourceTaskRunIds.AddUnique(Update.TaskRunId);
	}
	if (!Update.ArchiveSessionId.IsEmpty())
	{
		Session.ArchiveSessionIds.AddUnique(Update.ArchiveSessionId);
		if (Session.SessionStartArchiveSessionId.IsEmpty())
		{
			Session.SessionStartArchiveSessionId = Update.ArchiveSessionId;
		}
	}
	for (const FString& ReviewRecordId : Update.ReviewRecordIds)
	{
		if (!ReviewRecordId.IsEmpty())
		{
			Session.ReviewRecordIds.AddUnique(ReviewRecordId);
		}
	}

	if (Update.bSucceeded)
	{
		Session.LastGoodArchiveSessionId = Update.ArchiveSessionId;
		Session.LastGoodTaskRunId = Update.TaskRunId;
		Session.LastFailureTaskRunId.Reset();
		Session.bHasLastGoodSnapshot = true;
		Session.bHasUnresolvedFailedExecute = false;
		Session.Status = EBlueprintHelperSequentialReviewSessionStatus::Active;
	}
	else if (Update.bFailed)
	{
		Session.LastFailureTaskRunId = Update.TaskRunId;
		Session.bHasUnresolvedFailedExecute = true;
		Session.Status = EBlueprintHelperSequentialReviewSessionStatus::NeedsRecovery;
	}

	Session.UpdatedAt = FDateTime::UtcNow().ToIso8601();
	if (Session.CreatedAt.IsEmpty())
	{
		Session.CreatedAt = Session.UpdatedAt;
	}

	if (!SaveSession(Session, OutError))
	{
		return false;
	}

	OutSession = Session;
	return true;
}

bool FBlueprintHelperSequentialReviewSessionService::CloseSessionsForReviewRecord(
	const FString& ReviewRecordId,
	EBlueprintHelperSequentialReviewSessionStatus FinalStatus,
	FString& OutError) const
{
	if (ReviewRecordId.IsEmpty())
	{
		OutError = TEXT("review_record_id is required");
		return false;
	}

	bool bMatched = false;
	for (FBlueprintHelperSequentialReviewSession Session : QuerySessions())
	{
		if (!Session.ReviewRecordIds.Contains(ReviewRecordId))
		{
			continue;
		}

		Session.Status = FinalStatus;
		Session.bHasUnresolvedFailedExecute = false;
		Session.UpdatedAt = FDateTime::UtcNow().ToIso8601();
		if (!SaveSession(Session, OutError))
		{
			return false;
		}
		bMatched = true;
	}

	if (!bMatched)
	{
		OutError.Reset();
	}
	return true;
}

TArray<FBlueprintHelperSequentialReviewSession>
FBlueprintHelperSequentialReviewSessionService::QuerySessions() const
{
	TArray<FBlueprintHelperSequentialReviewSession> Sessions;
	const FString SessionsDir = GetSessionsDir();
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(SessionsDir / TEXT("*.json")), true, false);
	for (const FString& File : Files)
	{
		const FString Path = SessionsDir / File;
		FString Content;
		if (!FFileHelper::LoadFileToString(Content, *Path))
		{
			continue;
		}

		TSharedPtr<FJsonObject> Json;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
		FBlueprintHelperSequentialReviewSession Session;
		if (FJsonSerializer::Deserialize(Reader, Json) &&
			FBlueprintHelperSequentialReviewSessionJsonLocal::FromJson(Json, Session))
		{
			Sessions.Add(Session);
		}
	}
	return Sessions;
}

bool FBlueprintHelperSequentialReviewSessionService::LoadSession(
	const FString& SequentialReviewSessionId,
	FBlueprintHelperSequentialReviewSession& OutSession,
	FString& OutError) const
{
	OutSession = FBlueprintHelperSequentialReviewSession();
	if (SequentialReviewSessionId.IsEmpty())
	{
		OutError = TEXT("sequential_review_session_id is required");
		return false;
	}

	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *MakeSessionPath(SequentialReviewSessionId)))
	{
		OutError = FString::Printf(
			TEXT("sequential review session not found: %s"),
			*SequentialReviewSessionId);
		return false;
	}

	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
	if (!FJsonSerializer::Deserialize(Reader, Json) ||
		!FBlueprintHelperSequentialReviewSessionJsonLocal::FromJson(Json, OutSession))
	{
		OutError = FString::Printf(
			TEXT("failed to parse sequential review session: %s"),
			*SequentialReviewSessionId);
		return false;
	}
	return true;
}

bool FBlueprintHelperSequentialReviewSessionService::SaveSession(
	const FBlueprintHelperSequentialReviewSession& Session,
	FString& OutError) const
{
	if (Session.SequentialReviewSessionId.IsEmpty())
	{
		OutError = TEXT("sequential_review_session_id is required");
		return false;
	}

	IFileManager::Get().MakeDirectory(*GetSessionsDir(), true);
	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	if (!FJsonSerializer::Serialize(
		FBlueprintHelperSequentialReviewSessionJsonLocal::ToJson(Session),
		Writer))
	{
		OutError = TEXT("failed to serialize sequential review session");
		return false;
	}

	if (!FFileHelper::SaveStringToFile(Output, *MakeSessionPath(Session.SequentialReviewSessionId)))
	{
		OutError = FString::Printf(
			TEXT("failed to save sequential review session: %s"),
			*Session.SequentialReviewSessionId);
		return false;
	}

	OutError.Reset();
	return true;
}

FString FBlueprintHelperSequentialReviewSessionService::GetSessionsDir()
{
	return FBlueprintHelperReviewConfigResolver::Load().GetReviewRootDir() / TEXT("SequentialSessions");
}

FString FBlueprintHelperSequentialReviewSessionService::MakeSessionPath(
	const FString& SequentialReviewSessionId)
{
	FString SafeId = SequentialReviewSessionId;
	SafeId.ReplaceInline(TEXT("/"), TEXT("_"));
	SafeId.ReplaceInline(TEXT("\\"), TEXT("_"));
	SafeId.ReplaceInline(TEXT(":"), TEXT("_"));
	SafeId.ReplaceInline(TEXT(" "), TEXT("_"));
	return GetSessionsDir() / FString::Printf(TEXT("%s.json"), *SafeId);
}

bool FBlueprintHelperSequentialReviewSessionService::HasSameTargetSet(
	const TArray<FString>& Left,
	const TArray<FString>& Right)
{
	TArray<FString> NormalizedLeft;
	for (const FString& Value : Left)
	{
		if (!Value.IsEmpty())
		{
			NormalizedLeft.AddUnique(Value);
		}
	}
	TArray<FString> NormalizedRight;
	for (const FString& Value : Right)
	{
		if (!Value.IsEmpty())
		{
			NormalizedRight.AddUnique(Value);
		}
	}

	NormalizedLeft.Sort();
	NormalizedRight.Sort();
	return NormalizedLeft == NormalizedRight;
}
