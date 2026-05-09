// BlueprintHelper Debug DTOs.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/Paths.h"

enum class EBlueprintHelperDebugSeverity : uint8
{
	Info,
	Warning,
	Error
};

enum class EBlueprintHelperDebugEventStatus : uint8
{
	Captured,
	Archived
};

enum class EBlueprintHelperDebugCaseStatus : uint8
{
	Open,
	NeedsAction,
	Resolved,
	Archived
};

inline const TCHAR* BlueprintHelperDebugSeverityToString(EBlueprintHelperDebugSeverity Severity)
{
	switch (Severity)
	{
	case EBlueprintHelperDebugSeverity::Warning: return TEXT("warning");
	case EBlueprintHelperDebugSeverity::Error: return TEXT("error");
	default: return TEXT("info");
	}
}

inline EBlueprintHelperDebugSeverity BlueprintHelperDebugSeverityFromString(const FString& Value)
{
	const FString Lower = Value.ToLower();
	if (Lower == TEXT("warning")) return EBlueprintHelperDebugSeverity::Warning;
	if (Lower == TEXT("error")) return EBlueprintHelperDebugSeverity::Error;
	return EBlueprintHelperDebugSeverity::Info;
}

inline const TCHAR* BlueprintHelperDebugEventStatusToString(EBlueprintHelperDebugEventStatus Status)
{
	switch (Status)
	{
	case EBlueprintHelperDebugEventStatus::Archived: return TEXT("archived");
	default: return TEXT("captured");
	}
}

inline EBlueprintHelperDebugEventStatus BlueprintHelperDebugEventStatusFromString(const FString& Value)
{
	return Value.Equals(TEXT("archived"), ESearchCase::IgnoreCase)
		? EBlueprintHelperDebugEventStatus::Archived
		: EBlueprintHelperDebugEventStatus::Captured;
}

inline const TCHAR* BlueprintHelperDebugCaseStatusToString(EBlueprintHelperDebugCaseStatus Status)
{
	switch (Status)
	{
	case EBlueprintHelperDebugCaseStatus::NeedsAction: return TEXT("needs_action");
	case EBlueprintHelperDebugCaseStatus::Resolved: return TEXT("resolved");
	case EBlueprintHelperDebugCaseStatus::Archived: return TEXT("archived");
	default: return TEXT("open");
	}
}

inline EBlueprintHelperDebugCaseStatus BlueprintHelperDebugCaseStatusFromString(const FString& Value)
{
	if (Value.Equals(TEXT("needs_action"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperDebugCaseStatus::NeedsAction;
	}
	if (Value.Equals(TEXT("resolved"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperDebugCaseStatus::Resolved;
	}
	if (Value.Equals(TEXT("archived"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperDebugCaseStatus::Archived;
	}
	return EBlueprintHelperDebugCaseStatus::Open;
}

namespace BlueprintHelperDebugJson
{
	inline bool IsSensitiveFieldName(const FString& FieldName)
	{
		const FString Lower = FieldName.ToLower();
		return Lower.Contains(TEXT("raw_payload"))
			|| Lower.Contains(TEXT("raw_json"))
			|| Lower.Contains(TEXT("artifact"))
			|| Lower.Contains(TEXT("bundle_path"))
			|| Lower.Contains(TEXT("local_path"))
			|| Lower.Contains(TEXT("file_path"))
			|| Lower.Contains(TEXT("source_file"))
			|| Lower.Contains(TEXT("source_content"))
			|| Lower.Contains(TEXT("source_text"))
			|| Lower.Contains(TEXT("full_settings"))
			|| Lower.Contains(TEXT("settings_snapshot"))
			|| Lower.Contains(TEXT("token"))
			|| Lower.Contains(TEXT("secret"));
	}

	inline bool IsUnrealVirtualPath(const FString& Normalized)
	{
		return Normalized.StartsWith(TEXT("/Game/"))
			|| Normalized.StartsWith(TEXT("/Script/"))
			|| Normalized.StartsWith(TEXT("/Engine/"));
	}

	inline bool IsLocalAbsolutePath(const FString& Value)
	{
		FString Normalized = Value;
		FPaths::NormalizeFilename(Normalized);
		if (Normalized.Contains(TEXT("Saved/BlueprintHelper/Debug")))
		{
			return true;
		}
		if (Normalized.Len() >= 3 && FChar::IsAlpha(Normalized[0]) && Normalized[1] == TEXT(':') && Normalized[2] == TEXT('/'))
		{
			return true;
		}
		if (Normalized.StartsWith(TEXT("//")))
		{
			return true;
		}
		FString ProjectDir = FPaths::ProjectDir();
		FPaths::NormalizeDirectoryName(ProjectDir);
		if (!ProjectDir.IsEmpty() && Normalized.StartsWith(ProjectDir))
		{
			return true;
		}
		FString EngineDir = FPaths::EngineDir();
		FPaths::NormalizeDirectoryName(EngineDir);
		if (!EngineDir.IsEmpty() && Normalized.StartsWith(EngineDir))
		{
			return true;
		}
		return Normalized.StartsWith(TEXT("/")) && !IsUnrealVirtualPath(Normalized);
	}

	inline bool IsLocalDebugPath(const FString& Value)
	{
		return IsLocalAbsolutePath(Value);
	}

	inline bool LooksLikeToken(const FString& Value)
	{
		const FString Lower = Value.ToLower();
		return Lower.Contains(TEXT("bearer "))
			|| Lower.Contains(TEXT("auth_token"))
			|| Lower.Contains(TEXT("bridge_token"))
			|| Value.Contains(TEXT("sk-"))
			|| Value.Contains(TEXT("ghp_"))
			|| Value.Contains(TEXT("xoxb-"));
	}

	inline bool LooksLikeSourceContent(const FString& Value)
	{
		return Value.Contains(TEXT("\n"))
			&& (Value.Contains(TEXT("#include"))
				|| Value.Contains(TEXT("UCLASS("))
				|| Value.Contains(TEXT("UPROPERTY("))
				|| Value.Contains(TEXT("UFUNCTION("))
				|| Value.Contains(TEXT("class "))
				|| Value.Contains(TEXT("struct ")));
	}

	inline FString RedactString(const FString& Value)
	{
		if (LooksLikeToken(Value) || IsLocalAbsolutePath(Value) || LooksLikeSourceContent(Value))
		{
			return TEXT("[redacted]");
		}
		return Value;
	}

	TSharedPtr<FJsonValue> SanitizeValue(const TSharedPtr<FJsonValue>& Value);

	inline TSharedRef<FJsonObject> SanitizeObject(const TSharedPtr<FJsonObject>& Object)
	{
		TSharedRef<FJsonObject> Sanitized = MakeShared<FJsonObject>();
		if (!Object.IsValid())
		{
			return Sanitized;
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
		{
			if (IsSensitiveFieldName(Pair.Key))
			{
				continue;
			}
			TSharedPtr<FJsonValue> SanitizedValue = SanitizeValue(Pair.Value);
			if (SanitizedValue.IsValid())
			{
				Sanitized->SetField(Pair.Key, SanitizedValue);
			}
		}
		return Sanitized;
	}

	inline TSharedPtr<FJsonValue> SanitizeValue(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid() || Value->Type == EJson::Null)
		{
			return nullptr;
		}
		if (Value->Type == EJson::String)
		{
			const FString StringValue = Value->AsString();
			return MakeShared<FJsonValueString>(RedactString(StringValue));
		}
		if (Value->Type == EJson::Number)
		{
			return MakeShared<FJsonValueNumber>(Value->AsNumber());
		}
		if (Value->Type == EJson::Boolean)
		{
			return MakeShared<FJsonValueBoolean>(Value->AsBool());
		}
		if (Value->Type == EJson::Object)
		{
			return MakeShared<FJsonValueObject>(SanitizeObject(Value->AsObject()));
		}
		if (Value->Type == EJson::Array)
		{
			TArray<TSharedPtr<FJsonValue>> SanitizedArray;
			for (const TSharedPtr<FJsonValue>& Item : Value->AsArray())
			{
				TSharedPtr<FJsonValue> SanitizedItem = SanitizeValue(Item);
				if (SanitizedItem.IsValid())
				{
					SanitizedArray.Add(SanitizedItem);
				}
			}
			return MakeShared<FJsonValueArray>(SanitizedArray);
		}
		return nullptr;
	}

	inline TArray<TSharedPtr<FJsonValue>> StringArrayToJson(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> JsonValues;
		for (const FString& Value : Values)
		{
			if (!Value.IsEmpty())
			{
				JsonValues.Add(MakeShared<FJsonValueString>(RedactString(Value)));
			}
		}
		return JsonValues;
	}

	inline void ReadStringArray(const TSharedPtr<FJsonObject>& Json, const TCHAR* FieldName, TArray<FString>& OutValues)
	{
		OutValues.Reset();
		if (!Json.IsValid())
		{
			return;
		}
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Json->TryGetArrayField(FieldName, Values))
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			if (Value.IsValid() && Value->Type == EJson::String)
			{
				OutValues.Add(Value->AsString());
			}
		}
	}
}

struct FBlueprintHelperDebugError
{
	FString Code;
	FString Message;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		if (!Code.IsEmpty()) Json->SetStringField(TEXT("code"), Code);
		if (!Message.IsEmpty()) Json->SetStringField(TEXT("message"), BlueprintHelperDebugJson::RedactString(Message));
		return Json;
	}

	static FBlueprintHelperDebugError FromJson(const TSharedPtr<FJsonObject>& Json)
	{
		FBlueprintHelperDebugError Error;
		if (Json.IsValid())
		{
			Json->TryGetStringField(TEXT("code"), Error.Code);
			Json->TryGetStringField(TEXT("message"), Error.Message);
		}
		return Error;
	}
};

struct FBlueprintHelperDebugRedactionInfo
{
	FString Profile;
	bool bRequiresRedaction = true;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("profile"), Profile.IsEmpty() ? TEXT("standard") : Profile);
		Json->SetBoolField(TEXT("requires_redaction"), bRequiresRedaction);
		return Json;
	}
};

struct FBlueprintHelperDebugTransactionLink
{
	FString TransactionId;
	FString Role;
	FString Source;
	FString Summary;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		if (!TransactionId.IsEmpty()) Json->SetStringField(TEXT("transaction_id"), BlueprintHelperDebugJson::RedactString(TransactionId));
		if (!Role.IsEmpty()) Json->SetStringField(TEXT("role"), BlueprintHelperDebugJson::RedactString(Role));
		if (!Source.IsEmpty()) Json->SetStringField(TEXT("source"), BlueprintHelperDebugJson::RedactString(Source));
		if (!Summary.IsEmpty()) Json->SetStringField(TEXT("summary"), BlueprintHelperDebugJson::RedactString(Summary));
		return Json;
	}

	static FBlueprintHelperDebugTransactionLink FromJson(const TSharedPtr<FJsonObject>& Json)
	{
		FBlueprintHelperDebugTransactionLink Link;
		if (Json.IsValid())
		{
			Json->TryGetStringField(TEXT("transaction_id"), Link.TransactionId);
			Json->TryGetStringField(TEXT("role"), Link.Role);
			Json->TryGetStringField(TEXT("source"), Link.Source);
			Json->TryGetStringField(TEXT("summary"), Link.Summary);
		}
		return Link;
	}
};

struct FBlueprintHelperDebugEvent
{
	FString DebugEventId;
	FString DebugCaseId;
	FString CreatedAt;
	FString SourceLayer;
	FString Source;
	FString Operation;
	FString Stage;
	EBlueprintHelperDebugSeverity Severity = EBlueprintHelperDebugSeverity::Error;
	EBlueprintHelperDebugEventStatus Status = EBlueprintHelperDebugEventStatus::Captured;
	FString TraceId;
	FString TaskRunId;
	TArray<FString> AssetPaths;
	TArray<FString> ReviewRecordIds;
	TArray<FBlueprintHelperDebugTransactionLink> TransactionLinks;
	FBlueprintHelperDebugError Error;
	FBlueprintHelperDebugRedactionInfo Redaction;
	FString RecommendedNext;
	TSharedPtr<FJsonObject> ToolResultSummary;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.DebugEvent.v1"));
		if (!DebugEventId.IsEmpty()) Json->SetStringField(TEXT("debug_event_id"), DebugEventId);
		if (!DebugCaseId.IsEmpty()) Json->SetStringField(TEXT("debug_case_id"), DebugCaseId);
		if (!CreatedAt.IsEmpty()) Json->SetStringField(TEXT("created_at"), CreatedAt);
		if (!SourceLayer.IsEmpty()) Json->SetStringField(TEXT("source_layer"), SourceLayer);
		if (!Source.IsEmpty()) Json->SetStringField(TEXT("source"), Source);
		if (!Operation.IsEmpty()) Json->SetStringField(TEXT("operation"), Operation);
		if (!Stage.IsEmpty()) Json->SetStringField(TEXT("stage"), Stage);
		Json->SetStringField(TEXT("severity"), BlueprintHelperDebugSeverityToString(Severity));
		Json->SetStringField(TEXT("status"), BlueprintHelperDebugEventStatusToString(Status));
		if (!TraceId.IsEmpty()) Json->SetStringField(TEXT("trace_id"), TraceId);
		if (!TaskRunId.IsEmpty()) Json->SetStringField(TEXT("task_run_id"), TaskRunId);
		if (AssetPaths.Num() > 0) Json->SetArrayField(TEXT("asset_paths"), BlueprintHelperDebugJson::StringArrayToJson(AssetPaths));
		if (ReviewRecordIds.Num() > 0) Json->SetArrayField(TEXT("review_record_ids"), BlueprintHelperDebugJson::StringArrayToJson(ReviewRecordIds));
		if (TransactionLinks.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> TransactionValues;
			for (const FBlueprintHelperDebugTransactionLink& Link : TransactionLinks)
			{
				TransactionValues.Add(MakeShared<FJsonValueObject>(Link.ToJson()));
			}
			Json->SetArrayField(TEXT("transaction_links"), TransactionValues);
		}
		if (!Error.Code.IsEmpty() || !Error.Message.IsEmpty()) Json->SetObjectField(TEXT("error"), Error.ToJson());
		Json->SetObjectField(TEXT("redaction"), Redaction.ToJson());
		if (!RecommendedNext.IsEmpty()) Json->SetStringField(TEXT("recommended_next"), RecommendedNext);
		if (ToolResultSummary.IsValid())
		{
			Json->SetObjectField(TEXT("tool_result_summary"), BlueprintHelperDebugJson::SanitizeObject(ToolResultSummary));
		}
		return Json;
	}

	static FBlueprintHelperDebugEvent FromJson(const TSharedPtr<FJsonObject>& Json)
	{
		FBlueprintHelperDebugEvent Event;
		if (!Json.IsValid())
		{
			return Event;
		}
		Json->TryGetStringField(TEXT("debug_event_id"), Event.DebugEventId);
		Json->TryGetStringField(TEXT("debug_case_id"), Event.DebugCaseId);
		Json->TryGetStringField(TEXT("created_at"), Event.CreatedAt);
		Json->TryGetStringField(TEXT("source_layer"), Event.SourceLayer);
		Json->TryGetStringField(TEXT("source"), Event.Source);
		Json->TryGetStringField(TEXT("operation"), Event.Operation);
		Json->TryGetStringField(TEXT("stage"), Event.Stage);
		FString SeverityText;
		if (Json->TryGetStringField(TEXT("severity"), SeverityText)) Event.Severity = BlueprintHelperDebugSeverityFromString(SeverityText);
		FString StatusText;
		if (Json->TryGetStringField(TEXT("status"), StatusText)) Event.Status = BlueprintHelperDebugEventStatusFromString(StatusText);
		Json->TryGetStringField(TEXT("trace_id"), Event.TraceId);
		Json->TryGetStringField(TEXT("task_run_id"), Event.TaskRunId);
		BlueprintHelperDebugJson::ReadStringArray(Json, TEXT("asset_paths"), Event.AssetPaths);
		BlueprintHelperDebugJson::ReadStringArray(Json, TEXT("review_record_ids"), Event.ReviewRecordIds);
		const TArray<TSharedPtr<FJsonValue>>* TransactionValues = nullptr;
		if (Json->TryGetArrayField(TEXT("transaction_links"), TransactionValues))
		{
			for (const TSharedPtr<FJsonValue>& Value : *TransactionValues)
			{
				if (Value.IsValid() && Value->Type == EJson::Object)
				{
					Event.TransactionLinks.Add(FBlueprintHelperDebugTransactionLink::FromJson(Value->AsObject()));
				}
			}
		}
		const TSharedPtr<FJsonObject>* ErrorObject = nullptr;
		if (Json->TryGetObjectField(TEXT("error"), ErrorObject))
		{
			Event.Error = FBlueprintHelperDebugError::FromJson(*ErrorObject);
		}
		Json->TryGetStringField(TEXT("recommended_next"), Event.RecommendedNext);
		const TSharedPtr<FJsonObject>* ToolSummaryObject = nullptr;
		if (Json->TryGetObjectField(TEXT("tool_result_summary"), ToolSummaryObject))
		{
			Event.ToolResultSummary = *ToolSummaryObject;
		}
		return Event;
	}
};

struct FBlueprintHelperDebugCase
{
	FString DebugCaseId;
	FString CreatedAt;
	FString UpdatedAt;
	FString Source;
	EBlueprintHelperDebugSeverity Severity = EBlueprintHelperDebugSeverity::Error;
	EBlueprintHelperDebugCaseStatus Status = EBlueprintHelperDebugCaseStatus::Open;
	FString Operation;
	FString Stage;
	TArray<FString> TraceIds;
	FString TaskRunId;
	TArray<FString> AssetPaths;
	TArray<FString> ReviewRecordIds;
	TArray<FBlueprintHelperDebugTransactionLink> TransactionLinks;
	FBlueprintHelperDebugError Error;
	FString RecommendedNext;
	TArray<FBlueprintHelperDebugEvent> Events;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.DebugCase.v1"));
		if (!DebugCaseId.IsEmpty()) Json->SetStringField(TEXT("debug_case_id"), DebugCaseId);
		if (!CreatedAt.IsEmpty()) Json->SetStringField(TEXT("created_at"), CreatedAt);
		if (!UpdatedAt.IsEmpty()) Json->SetStringField(TEXT("updated_at"), UpdatedAt);
		if (!Source.IsEmpty()) Json->SetStringField(TEXT("source"), Source);
		Json->SetStringField(TEXT("severity"), BlueprintHelperDebugSeverityToString(Severity));
		Json->SetStringField(TEXT("status"), BlueprintHelperDebugCaseStatusToString(Status));
		if (!Operation.IsEmpty()) Json->SetStringField(TEXT("operation"), Operation);
		if (!Stage.IsEmpty()) Json->SetStringField(TEXT("stage"), Stage);
		if (TraceIds.Num() > 0) Json->SetArrayField(TEXT("trace_ids"), BlueprintHelperDebugJson::StringArrayToJson(TraceIds));
		if (!TaskRunId.IsEmpty()) Json->SetStringField(TEXT("task_run_id"), TaskRunId);
		if (AssetPaths.Num() > 0) Json->SetArrayField(TEXT("asset_paths"), BlueprintHelperDebugJson::StringArrayToJson(AssetPaths));
		if (ReviewRecordIds.Num() > 0) Json->SetArrayField(TEXT("review_record_ids"), BlueprintHelperDebugJson::StringArrayToJson(ReviewRecordIds));
		if (TransactionLinks.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> TransactionValues;
			for (const FBlueprintHelperDebugTransactionLink& Link : TransactionLinks)
			{
				TransactionValues.Add(MakeShared<FJsonValueObject>(Link.ToJson()));
			}
			Json->SetArrayField(TEXT("transaction_links"), TransactionValues);
		}
		if (!Error.Code.IsEmpty() || !Error.Message.IsEmpty()) Json->SetObjectField(TEXT("error"), Error.ToJson());
		if (!RecommendedNext.IsEmpty()) Json->SetStringField(TEXT("recommended_next"), RecommendedNext);
		if (Events.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> EventValues;
			for (const FBlueprintHelperDebugEvent& Event : Events)
			{
				EventValues.Add(MakeShared<FJsonValueObject>(Event.ToJson()));
			}
			Json->SetArrayField(TEXT("events"), EventValues);
		}
		return Json;
	}

	static FBlueprintHelperDebugCase FromJson(const TSharedPtr<FJsonObject>& Json)
	{
		FBlueprintHelperDebugCase DebugCase;
		if (!Json.IsValid())
		{
			return DebugCase;
		}
		Json->TryGetStringField(TEXT("debug_case_id"), DebugCase.DebugCaseId);
		Json->TryGetStringField(TEXT("created_at"), DebugCase.CreatedAt);
		Json->TryGetStringField(TEXT("updated_at"), DebugCase.UpdatedAt);
		Json->TryGetStringField(TEXT("source"), DebugCase.Source);
		FString SeverityText;
		if (Json->TryGetStringField(TEXT("severity"), SeverityText)) DebugCase.Severity = BlueprintHelperDebugSeverityFromString(SeverityText);
		FString StatusText;
		if (Json->TryGetStringField(TEXT("status"), StatusText)) DebugCase.Status = BlueprintHelperDebugCaseStatusFromString(StatusText);
		Json->TryGetStringField(TEXT("operation"), DebugCase.Operation);
		Json->TryGetStringField(TEXT("stage"), DebugCase.Stage);
		BlueprintHelperDebugJson::ReadStringArray(Json, TEXT("trace_ids"), DebugCase.TraceIds);
		Json->TryGetStringField(TEXT("task_run_id"), DebugCase.TaskRunId);
		BlueprintHelperDebugJson::ReadStringArray(Json, TEXT("asset_paths"), DebugCase.AssetPaths);
		BlueprintHelperDebugJson::ReadStringArray(Json, TEXT("review_record_ids"), DebugCase.ReviewRecordIds);
		const TArray<TSharedPtr<FJsonValue>>* TransactionValues = nullptr;
		if (Json->TryGetArrayField(TEXT("transaction_links"), TransactionValues))
		{
			for (const TSharedPtr<FJsonValue>& Value : *TransactionValues)
			{
				if (Value.IsValid() && Value->Type == EJson::Object)
				{
					DebugCase.TransactionLinks.Add(FBlueprintHelperDebugTransactionLink::FromJson(Value->AsObject()));
				}
			}
		}
		const TSharedPtr<FJsonObject>* ErrorObject = nullptr;
		if (Json->TryGetObjectField(TEXT("error"), ErrorObject))
		{
			DebugCase.Error = FBlueprintHelperDebugError::FromJson(*ErrorObject);
		}
		Json->TryGetStringField(TEXT("recommended_next"), DebugCase.RecommendedNext);
		const TArray<TSharedPtr<FJsonValue>>* EventValues = nullptr;
		if (Json->TryGetArrayField(TEXT("events"), EventValues))
		{
			for (const TSharedPtr<FJsonValue>& Value : *EventValues)
			{
				if (Value.IsValid() && Value->Type == EJson::Object)
				{
					DebugCase.Events.Add(FBlueprintHelperDebugEvent::FromJson(Value->AsObject()));
				}
			}
		}
		return DebugCase;
	}
};

struct FBlueprintHelperDebugCaseSummary
{
	FString DebugCaseId;
	FString CreatedAt;
	FString UpdatedAt;
	FString Source;
	EBlueprintHelperDebugSeverity Severity = EBlueprintHelperDebugSeverity::Error;
	EBlueprintHelperDebugCaseStatus Status = EBlueprintHelperDebugCaseStatus::Open;
	FString Operation;
	FString Stage;
	TArray<FString> TraceIds;
	FString TaskRunId;
	TArray<FString> AssetPaths;
	TArray<FString> ReviewRecordIds;
	TArray<FBlueprintHelperDebugTransactionLink> TransactionLinks;
	FBlueprintHelperDebugError Error;
	FString RecommendedNext;
	int32 EventCount = 0;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.DebugCaseSummary.v1"));
		if (!DebugCaseId.IsEmpty()) Json->SetStringField(TEXT("debug_case_id"), DebugCaseId);
		if (!CreatedAt.IsEmpty()) Json->SetStringField(TEXT("created_at"), CreatedAt);
		if (!UpdatedAt.IsEmpty()) Json->SetStringField(TEXT("updated_at"), UpdatedAt);
		if (!Source.IsEmpty()) Json->SetStringField(TEXT("source"), Source);
		Json->SetStringField(TEXT("severity"), BlueprintHelperDebugSeverityToString(Severity));
		Json->SetStringField(TEXT("status"), BlueprintHelperDebugCaseStatusToString(Status));
		if (!Operation.IsEmpty()) Json->SetStringField(TEXT("operation"), Operation);
		if (!Stage.IsEmpty()) Json->SetStringField(TEXT("stage"), Stage);
		if (TraceIds.Num() > 0) Json->SetArrayField(TEXT("trace_ids"), BlueprintHelperDebugJson::StringArrayToJson(TraceIds));
		if (!TaskRunId.IsEmpty()) Json->SetStringField(TEXT("task_run_id"), TaskRunId);
		if (AssetPaths.Num() > 0) Json->SetArrayField(TEXT("asset_paths"), BlueprintHelperDebugJson::StringArrayToJson(AssetPaths));
		if (ReviewRecordIds.Num() > 0) Json->SetArrayField(TEXT("review_record_ids"), BlueprintHelperDebugJson::StringArrayToJson(ReviewRecordIds));
		if (TransactionLinks.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> TransactionValues;
			for (const FBlueprintHelperDebugTransactionLink& Link : TransactionLinks)
			{
				TransactionValues.Add(MakeShared<FJsonValueObject>(Link.ToJson()));
			}
			Json->SetArrayField(TEXT("transaction_links"), TransactionValues);
		}
		if (!Error.Code.IsEmpty() || !Error.Message.IsEmpty()) Json->SetObjectField(TEXT("error"), Error.ToJson());
		if (!RecommendedNext.IsEmpty()) Json->SetStringField(TEXT("recommended_next"), RecommendedNext);
		Json->SetNumberField(TEXT("event_count"), EventCount);
		return Json;
	}
};

struct FBlueprintHelperDebugSkippedArtifact
{
	FString Artifact;
	FString Reason;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		if (!Artifact.IsEmpty()) Json->SetStringField(TEXT("artifact"), Artifact);
		if (!Reason.IsEmpty()) Json->SetStringField(TEXT("reason"), Reason);
		return Json;
	}
};

struct FBlueprintHelperDebugBundleManifest
{
	FString BundleId;
	FString DebugCaseId;
	FString CreatedAt;
	FString SummaryRef;
	TArray<FString> Contents;
	TArray<FString> ReviewSummaryRefs;
	TArray<FBlueprintHelperDebugSkippedArtifact> SkippedArtifacts;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.DebugBundleManifest.v1"));
		if (!BundleId.IsEmpty()) Json->SetStringField(TEXT("bundle_id"), BundleId);
		if (!DebugCaseId.IsEmpty()) Json->SetStringField(TEXT("debug_case_id"), DebugCaseId);
		Json->SetStringField(TEXT("format"), TEXT("directory"));
		if (!CreatedAt.IsEmpty()) Json->SetStringField(TEXT("created_at"), CreatedAt);
		Json->SetNumberField(TEXT("manifest_version"), 1);
		if (!SummaryRef.IsEmpty() && FPaths::IsRelative(SummaryRef)) Json->SetStringField(TEXT("summary_ref"), SummaryRef);
		TArray<TSharedPtr<FJsonValue>> SafeContents;
		for (const FString& Content : Contents)
		{
			if (!Content.IsEmpty() && FPaths::IsRelative(Content) && !BlueprintHelperDebugJson::IsLocalDebugPath(Content))
			{
				SafeContents.Add(MakeShared<FJsonValueString>(Content));
			}
		}
		Json->SetArrayField(TEXT("contents"), SafeContents);
		TArray<TSharedPtr<FJsonValue>> SafeReviewRefs;
		for (const FString& ReviewSummaryRef : ReviewSummaryRefs)
		{
			if (!ReviewSummaryRef.IsEmpty() && FPaths::IsRelative(ReviewSummaryRef) && !BlueprintHelperDebugJson::IsLocalDebugPath(ReviewSummaryRef))
			{
				SafeReviewRefs.Add(MakeShared<FJsonValueString>(ReviewSummaryRef));
			}
		}
		if (SafeReviewRefs.Num() > 0)
		{
			Json->SetArrayField(TEXT("review_summary_refs"), SafeReviewRefs);
		}
		if (SkippedArtifacts.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> SkippedValues;
			for (const FBlueprintHelperDebugSkippedArtifact& SkippedArtifact : SkippedArtifacts)
			{
				SkippedValues.Add(MakeShared<FJsonValueObject>(SkippedArtifact.ToJson()));
			}
			Json->SetArrayField(TEXT("skipped_artifacts"), SkippedValues);
		}
		TSharedRef<FJsonObject> Privacy = MakeShared<FJsonObject>();
		Privacy->SetStringField(TEXT("profile"), TEXT("standard"));
		Privacy->SetBoolField(TEXT("summary_only"), true);
		Privacy->SetBoolField(TEXT("redacted"), true);
		Privacy->SetBoolField(TEXT("contains_tokens"), false);
		Privacy->SetBoolField(TEXT("contains_full_settings"), false);
		Privacy->SetBoolField(TEXT("contains_local_absolute_paths"), false);
		Privacy->SetBoolField(TEXT("contains_full_asset_raw_json"), false);
		Privacy->SetBoolField(TEXT("contains_source_files"), false);
		TArray<TSharedPtr<FJsonValue>> Redactions;
		Redactions.Add(MakeShared<FJsonValueString>(TEXT("tokens")));
		Redactions.Add(MakeShared<FJsonValueString>(TEXT("env_values")));
		Redactions.Add(MakeShared<FJsonValueString>(TEXT("settings_full")));
		Redactions.Add(MakeShared<FJsonValueString>(TEXT("local_absolute_paths")));
		Redactions.Add(MakeShared<FJsonValueString>(TEXT("full_raw_json")));
		Redactions.Add(MakeShared<FJsonValueString>(TEXT("source_content")));
		Privacy->SetArrayField(TEXT("redactions"), Redactions);
		Json->SetObjectField(TEXT("privacy"), Privacy);
		return Json;
	}
};
