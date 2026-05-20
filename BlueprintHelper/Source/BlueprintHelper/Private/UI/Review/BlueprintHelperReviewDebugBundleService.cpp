// BlueprintHelper ReviewPanel debug bundle service.

#include "UI/Review/BlueprintHelperReviewDebugBundleService.h"

#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "HAL/CriticalSection.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Systems/Review/BlueprintHelperReviewConfigResolver.h"
#include "UI/Review/Utils/BlueprintHelperReviewDebugBundleUtils.h"

FString FBlueprintHelperReviewDebugBundleService::GetDebugRootDir()
{
	return FBlueprintHelperReviewConfigResolver::Load().DebugBundle.RootDir;
}

FString FBlueprintHelperReviewDebugBundleService::GetReviewPanelBundleDir()
{
	return FBlueprintHelperReviewConfigResolver::Load().DebugBundle.GetBundleDir();
}

FString FBlueprintHelperReviewDebugBundleService::MakeDefaultBundlePath()
{
	const FBlueprintHelperReviewDebugBundleConfig BundleConfig =
		FBlueprintHelperReviewConfigResolver::Load().DebugBundle;
	FString Filename = FDateTime::UtcNow().ToString(*BundleConfig.FilenamePattern);
	if (!Filename.EndsWith(TEXT(".json"), ESearchCase::IgnoreCase))
	{
		Filename += TEXT(".json");
	}
	return GetReviewPanelBundleDir() / Filename;
}

FString FBlueprintHelperReviewDebugBundleService::NormalizeBundlePath(const FString& InPath)
{
	FString Path = InPath;
	Path.TrimStartAndEndInline();
	if (Path.IsEmpty())
	{
		Path = MakeDefaultBundlePath();
	}
	else if (FPaths::IsRelative(Path))
	{
		Path = GetReviewPanelBundleDir() / Path;
	}

	FPaths::NormalizeFilename(Path);
	FPaths::CollapseRelativeDirectories(Path);
	return Path;
}

bool FBlueprintHelperReviewDebugBundleService::IsPathInsideDebugRoot(const FString& Path)
{
	FString NormalizedPath = NormalizeBundlePath(Path);
	const FBlueprintHelperReviewDebugBundleConfig BundleConfig =
		FBlueprintHelperReviewConfigResolver::Load().DebugBundle;
	if (!BundleConfig.bEnforceRootPath)
	{
		return true;
	}
	FString NormalizedRoot = BundleConfig.RootDir;
	FPaths::NormalizeDirectoryName(NormalizedRoot);
	FPaths::CollapseRelativeDirectories(NormalizedRoot);

	if (NormalizedPath.Equals(NormalizedRoot, ESearchCase::IgnoreCase))
	{
		return true;
	}

	const FString RootPrefix = NormalizedRoot.EndsWith(TEXT("/"))
		? NormalizedRoot
		: NormalizedRoot + TEXT("/");
	return NormalizedPath.StartsWith(RootPrefix, ESearchCase::IgnoreCase);
}

TSharedRef<FJsonObject> FBlueprintHelperReviewDebugBundleService::BuildChangeSummary(
	const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Change)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!Change.IsValid())
	{
		Json->SetBoolField(TEXT("valid"), false);
		return Json;
	}

	Json->SetBoolField(TEXT("valid"), true);
	Json->SetStringField(TEXT("change_id"), Change->ChangeId);
	Json->SetStringField(TEXT("asset_path"), Change->AssetPath);
	Json->SetStringField(TEXT("graph_name"), Change->GraphName);
	Json->SetStringField(TEXT("location_key"), Change->LocationKey);
	Json->SetStringField(TEXT("display_label"), Change->DisplayLabel);
	Json->SetStringField(TEXT("latest_evidence_id"), Change->LatestEvidenceId);
	Json->SetStringField(TEXT("change_kind"), BlueprintHelperReviewChangeKindToString(Change->ChangeKind));
	Json->SetStringField(TEXT("status"), BlueprintHelperReviewChangeStatusToString(Change->Status));
	Json->SetStringField(TEXT("before_hash"), Change->BeforeHash);
	Json->SetStringField(TEXT("after_hash"), Change->AfterHash);
	const FBlueprintHelperReviewDebugBundleConfig BundleConfig =
		FBlueprintHelperReviewConfigResolver::Load().DebugBundle;
	Json->SetStringField(TEXT("hash_source"), BundleConfig.HashSource);
	Json->SetStringField(TEXT("snapshot_schema"), BundleConfig.SchemaSnapshot);
	Json->SetStringField(TEXT("retention_mode"), BundleConfig.Retention);
	Json->SetBoolField(TEXT("has_before_snapshot"), !Change->BeforeSnapshotJson.IsEmpty());
	Json->SetBoolField(TEXT("has_after_snapshot"), !Change->AfterSnapshotJson.IsEmpty());
	Json->SetNumberField(TEXT("atomic_target_count"), Change->AtomicTargets.Num());

	TArray<TSharedPtr<FJsonValue>> Targets;
	for (const FBlueprintHelperReviewAtomicTarget& Target : Change->AtomicTargets)
	{
		TSharedRef<FJsonObject> TargetJson = MakeShared<FJsonObject>();
		TargetJson->SetStringField(TEXT("surface"), BlueprintHelperReviewSurfaceToString(Target.Surface));
		TargetJson->SetStringField(TEXT("target_kind"), Target.TargetKind);
		TargetJson->SetStringField(TEXT("target_key"), Target.TargetKey);
		TargetJson->SetStringField(TEXT("display_label"), Target.DisplayLabel);
		TargetJson->SetStringField(TEXT("property_path"), Target.PropertyPath);
		TargetJson->SetStringField(TEXT("component_path"), Target.ComponentPath);
		TargetJson->SetStringField(TEXT("first_evidence_id"), Target.FirstEvidenceId);
		TargetJson->SetStringField(TEXT("latest_evidence_id"), Target.LatestEvidenceId);
		TargetJson->SetStringField(TEXT("baseline_hash"), Target.BaselineHash);
		TargetJson->SetStringField(TEXT("recorded_after_hash"), Target.RecordedAfterHash);
		TargetJson->SetStringField(TEXT("hash_source"), BundleConfig.HashSource);
		TargetJson->SetStringField(TEXT("snapshot_schema"), BundleConfig.SchemaSnapshot);
		TargetJson->SetBoolField(TEXT("has_before_snapshot"), !Target.BeforeSnapshotJson.IsEmpty());
		TargetJson->SetBoolField(TEXT("has_after_snapshot"), !Target.AfterSnapshotJson.IsEmpty());
		Targets.Add(MakeShared<FJsonValueObject>(TargetJson));
	}
	Json->SetArrayField(TEXT("atomic_targets"), Targets);
	return Json;
}

TSharedRef<FJsonObject> FBlueprintHelperReviewDebugBundleService::BuildLogEvent(
	const FString& SessionId,
	const FString& Message,
	const TSharedPtr<FBlueprintHelperReviewVisibleChange>& SelectedChange,
	const FString& AssetPath)
{
	TSharedRef<FJsonObject> Event = MakeShared<FJsonObject>();
	Event->SetStringField(TEXT("event_type"), TEXT("debug_log"));
	Event->SetStringField(TEXT("session_id"), SessionId);
	Event->SetStringField(TEXT("created_at"), FDateTime::UtcNow().ToIso8601());
	Event->SetStringField(TEXT("asset_path"), AssetPath);
	Event->SetStringField(TEXT("message"), FBlueprintHelperReviewDebugBundleUtils::SanitizeText(Message));
	Event->SetObjectField(TEXT("selected_change"), BuildChangeSummary(SelectedChange));
	return Event;
}

TSharedRef<FJsonObject> FBlueprintHelperReviewDebugBundleService::BuildFocusEvent(
	const FString& SessionId,
	const FString& Phase,
	int32 Index,
	int32 Count,
	const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Change,
	const FString& AssetPath,
	const FString& Reason)
{
	TSharedRef<FJsonObject> Event = MakeShared<FJsonObject>();
	Event->SetStringField(TEXT("event_type"), TEXT("focus_traversal"));
	Event->SetStringField(TEXT("session_id"), SessionId);
	Event->SetStringField(TEXT("created_at"), FDateTime::UtcNow().ToIso8601());
	Event->SetStringField(TEXT("phase"), Phase);
	Event->SetStringField(TEXT("asset_path"), AssetPath);
	Event->SetNumberField(TEXT("index"), Index);
	Event->SetNumberField(TEXT("count"), Count);
		Event->SetStringField(TEXT("reason"), FBlueprintHelperReviewDebugBundleUtils::SanitizeText(Reason));
	Event->SetObjectField(TEXT("change"), BuildChangeSummary(Change));
	return Event;
}

TSharedRef<FJsonObject> FBlueprintHelperReviewDebugBundleService::BuildActionHashGuardEvent(
	const FString& SessionId,
	const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Change,
	const FString& AssetPath,
	const FString& TargetKey,
	const FString& ExpectedHash,
	const FString& CurrentHash,
	const FString& CurrentSnapshotJson,
	const FString& RecordedAfterSnapshotJson)
{
	TSharedRef<FJsonObject> Event = BuildLogEvent(
		SessionId,
		FString::Printf(
			TEXT("Reject hash guard target=%s expected=%s current=%s"),
			*TargetKey,
			*ExpectedHash,
			*CurrentHash),
		Change,
		AssetPath);
	Event->SetStringField(TEXT("event_type"), TEXT("review_action_hash_guard"));
	Event->SetStringField(TEXT("target_key"), TargetKey);
	Event->SetStringField(TEXT("expected_hash"), ExpectedHash);
	Event->SetStringField(TEXT("current_hash"), CurrentHash);
	Event->SetStringField(TEXT("current_snapshot_json"), CurrentSnapshotJson);
	Event->SetStringField(TEXT("recorded_after_snapshot_json"), RecordedAfterSnapshotJson);
	return Event;
}

TSharedRef<FJsonObject> FBlueprintHelperReviewDebugBundleService::CreateEmptyBundle(const FString& SessionId)
{
	TSharedRef<FJsonObject> Bundle = MakeShared<FJsonObject>();
	Bundle->SetStringField(TEXT("schema"), FBlueprintHelperReviewConfigResolver::Load().DebugBundle.SchemaReviewPanel);
	Bundle->SetStringField(TEXT("session_id"), SessionId);
	Bundle->SetStringField(TEXT("created_at"), FDateTime::UtcNow().ToIso8601());
	Bundle->SetStringField(TEXT("updated_at"), FDateTime::UtcNow().ToIso8601());
	Bundle->SetArrayField(TEXT("events"), TArray<TSharedPtr<FJsonValue>>());
	return Bundle;
}

bool FBlueprintHelperReviewDebugBundleService::LoadOrCreateBundle(
	const FString& BundlePath,
	const FString& SessionId,
	TSharedPtr<FJsonObject>& OutBundle,
	FString* OutError)
{
	const FString NormalizedPath = NormalizeBundlePath(BundlePath);
	if (!IsPathInsideDebugRoot(NormalizedPath))
	{
		SetError(OutError, TEXT("debug bundle path must stay inside Saved/BlueprintHelper/Debug"));
		return false;
	}

	if (!IFileManager::Get().FileExists(*NormalizedPath))
	{
		OutBundle = CreateEmptyBundle(SessionId);
		SetError(OutError, FString());
		return true;
	}

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *NormalizedPath))
	{
		SetError(OutError, TEXT("failed to read debug bundle"));
		return false;
	}

	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, OutBundle) || !OutBundle.IsValid())
	{
		SetError(OutError, TEXT("debug bundle json is invalid"));
		return false;
	}

	SetError(OutError, FString());
	return true;
}

bool FBlueprintHelperReviewDebugBundleService::SaveBundle(
	const FString& BundlePath,
	const TSharedRef<FJsonObject>& Bundle,
	FString* OutError)
{
	const FString NormalizedPath = NormalizeBundlePath(BundlePath);
	if (!IsPathInsideDebugRoot(NormalizedPath))
	{
		SetError(OutError, TEXT("debug bundle path must stay inside Saved/BlueprintHelper/Debug"));
		return false;
	}

	const FString Dir = FPaths::GetPath(NormalizedPath);
	if (!IFileManager::Get().DirectoryExists(*Dir)
		&& !IFileManager::Get().MakeDirectory(*Dir, true))
	{
		SetError(OutError, TEXT("failed to create debug bundle directory"));
		return false;
	}

	Bundle->SetStringField(TEXT("updated_at"), FDateTime::UtcNow().ToIso8601());

	FString JsonText;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	if (!FJsonSerializer::Serialize(Bundle, Writer))
	{
		SetError(OutError, TEXT("failed to serialize debug bundle"));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(
		JsonText,
		*NormalizedPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		SetError(OutError, TEXT("failed to write debug bundle"));
		return false;
	}

	SetError(OutError, FString());
	return true;
}

bool FBlueprintHelperReviewDebugBundleService::AppendEvent(
	const FString& BundlePath,
	const FString& SessionId,
	const TSharedRef<FJsonObject>& Event,
	FString* OutError)
{
	const FString NormalizedPath = NormalizeBundlePath(BundlePath);
	if (!IsPathInsideDebugRoot(NormalizedPath))
	{
		SetError(OutError, TEXT("debug bundle path must stay inside Saved/BlueprintHelper/Debug"));
		return false;
	}

	const FString SessionIdCopy = SessionId;
	TSharedRef<FJsonObject> EventCopy = Event;
	if (FBlueprintHelperReviewDebugBundleUtils::IsShutdownRequested())
	{
		SetError(OutError, TEXT("debug bundle async writer is shutting down"));
		return false;
	}

	TFuture<void> WriteTask = Async(EAsyncExecution::ThreadPool, [NormalizedPath, SessionIdCopy, EventCopy]()
	{
		if (FBlueprintHelperReviewDebugBundleUtils::IsShutdownRequested())
		{
			return;
		}
		FScopeLock Lock(&FBlueprintHelperReviewDebugBundleUtils::GetWriteCriticalSection());
		FString IgnoredError;
		TSharedPtr<FJsonObject> Bundle;
		if (!FBlueprintHelperReviewDebugBundleService::LoadOrCreateBundle(
			NormalizedPath,
			SessionIdCopy,
			Bundle,
			&IgnoredError) || !Bundle.IsValid())
		{
			return;
		}

		TArray<TSharedPtr<FJsonValue>> Events;
		const TArray<TSharedPtr<FJsonValue>>* ExistingEvents = nullptr;
		if (Bundle->TryGetArrayField(TEXT("events"), ExistingEvents) && ExistingEvents)
		{
			Events = *ExistingEvents;
		}
		Events.Add(MakeShared<FJsonValueObject>(EventCopy));
		Bundle->SetArrayField(TEXT("events"), Events);
		FBlueprintHelperReviewDebugBundleService::SaveBundle(
			NormalizedPath,
			Bundle.ToSharedRef(),
			&IgnoredError);
	});
	FBlueprintHelperReviewDebugBundleUtils::TrackWriteTask(MoveTemp(WriteTask));

	SetError(OutError, FString());
	return true;
}

void FBlueprintHelperReviewDebugBundleService::FlushAsyncWrites()
{
	FBlueprintHelperReviewDebugBundleUtils::FlushWriteTasks();
}

void FBlueprintHelperReviewDebugBundleService::ShutdownAsyncWrites()
{
	FBlueprintHelperReviewDebugBundleUtils::ShutdownWriteTasks();
}

bool FBlueprintHelperReviewDebugBundleService::LoadBundleText(
	const FString& BundlePath,
	FString& OutText,
	FString* OutError)
{
	const FString NormalizedPath = NormalizeBundlePath(BundlePath);
	if (!IsPathInsideDebugRoot(NormalizedPath))
	{
		SetError(OutError, TEXT("debug bundle path must stay inside Saved/BlueprintHelper/Debug"));
		return false;
	}

	if (!FFileHelper::LoadFileToString(OutText, *NormalizedPath))
	{
		SetError(OutError, TEXT("failed to read debug bundle"));
		return false;
	}

	SetError(OutError, FString());
	return true;
}

bool FBlueprintHelperReviewDebugBundleService::LoadBundleSummaryText(
	const FString& BundlePath,
	FString& OutSummaryText,
	FString* OutError)
{
	FString BundleText;
	if (!LoadBundleText(BundlePath, BundleText, OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Bundle;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BundleText);
	if (!FJsonSerializer::Deserialize(Reader, Bundle) || !Bundle.IsValid())
	{
		SetError(OutError, TEXT("debug bundle json is invalid"));
		return false;
	}

	FString Schema;
	FString SessionId;
	FString CreatedAt;
	FString UpdatedAt;
	Bundle->TryGetStringField(TEXT("schema"), Schema);
	Bundle->TryGetStringField(TEXT("session_id"), SessionId);
	Bundle->TryGetStringField(TEXT("created_at"), CreatedAt);
	Bundle->TryGetStringField(TEXT("updated_at"), UpdatedAt);

	const TArray<TSharedPtr<FJsonValue>>* Events = nullptr;
	const int32 EventCount = Bundle->TryGetArrayField(TEXT("events"), Events) && Events ? Events->Num() : 0;
	TMap<FString, int32> TypeCounts;
	FString FirstEventAt;
	FString LastEventAt;
	if (Events)
	{
		for (const TSharedPtr<FJsonValue>& EventValue : *Events)
		{
			const TSharedPtr<FJsonObject> Event = EventValue.IsValid() ? EventValue->AsObject() : nullptr;
			if (!Event.IsValid())
			{
				continue;
			}

			FString EventType;
			Event->TryGetStringField(TEXT("event_type"), EventType);
			if (EventType.IsEmpty())
			{
				EventType = TEXT("unknown");
			}
			TypeCounts.FindOrAdd(EventType)++;

			FString EventCreatedAt;
			if (Event->TryGetStringField(TEXT("created_at"), EventCreatedAt) && !EventCreatedAt.IsEmpty())
			{
				if (FirstEventAt.IsEmpty())
				{
					FirstEventAt = EventCreatedAt;
				}
				LastEventAt = EventCreatedAt;
			}
		}
	}

	TArray<FString> TypeParts;
	for (const TPair<FString, int32>& Pair : TypeCounts)
	{
		TypeParts.Add(FString::Printf(TEXT("%s=%d"), *Pair.Key, Pair.Value));
	}
	TypeParts.Sort();

	OutSummaryText = FString::Printf(
		TEXT("DebugBundle Summary\nschema: %s\nsession_id: %s\ncreated_at: %s\nupdated_at: %s\nevents: %d\nevent_types: %s\nfirst_event_at: %s\nlast_event_at: %s"),
		*Schema,
		*SessionId,
		*CreatedAt,
		*UpdatedAt,
		EventCount,
		*FString::Join(TypeParts, TEXT(", ")),
		*FirstEventAt,
		*LastEventAt);
	SetError(OutError, FString());
	return true;
}

void FBlueprintHelperReviewDebugBundleService::SetError(FString* OutError, const FString& Error)
{
	if (OutError)
	{
		*OutError = Error;
	}
}

