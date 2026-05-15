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

namespace
{
static FCriticalSection GReviewPanelDebugBundleWriteCriticalSection;
static FCriticalSection GReviewPanelDebugBundleTaskCriticalSection;
static TArray<TFuture<void>> GReviewPanelDebugBundleWriteTasks;
static bool bReviewPanelDebugBundleAsyncShutdown = false;

static bool IsReviewPanelDebugBundleAsyncShutdownRequested()
{
	FScopeLock Lock(&GReviewPanelDebugBundleTaskCriticalSection);
	return bReviewPanelDebugBundleAsyncShutdown;
}

static void TrackReviewPanelDebugBundleWriteTask(TFuture<void>&& Future)
{
	bool bWaitImmediately = false;
	{
		FScopeLock Lock(&GReviewPanelDebugBundleTaskCriticalSection);
		for (int32 Index = GReviewPanelDebugBundleWriteTasks.Num() - 1; Index >= 0; --Index)
		{
			if (GReviewPanelDebugBundleWriteTasks[Index].IsReady())
			{
				GReviewPanelDebugBundleWriteTasks.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			}
		}
		if (bReviewPanelDebugBundleAsyncShutdown)
		{
			bWaitImmediately = true;
		}
		else
		{
			GReviewPanelDebugBundleWriteTasks.Add(MoveTemp(Future));
			return;
		}
	}
	if (bWaitImmediately)
	{
		Future.Wait();
	}
}

static void FlushReviewPanelDebugBundleWriteTasksInternal(bool bShutdown)
{
	TArray<TFuture<void>> Tasks;
	{
		FScopeLock Lock(&GReviewPanelDebugBundleTaskCriticalSection);
		bReviewPanelDebugBundleAsyncShutdown = bReviewPanelDebugBundleAsyncShutdown || bShutdown;
		Tasks = MoveTemp(GReviewPanelDebugBundleWriteTasks);
	}
	for (TFuture<void>& Task : Tasks)
	{
		Task.Wait();
	}
}

static FString SanitizeReviewPanelDebugBundleText(FString Text)
{
	Text.ReplaceInline(TEXT("\r\n"), TEXT("\\n"), ESearchCase::CaseSensitive);
	Text.ReplaceInline(TEXT("\r"), TEXT("\\n"), ESearchCase::CaseSensitive);
	Text.ReplaceInline(TEXT("\n"), TEXT("\\n"), ESearchCase::CaseSensitive);
	return Text;
}
}

FString FBlueprintHelperReviewDebugBundleService::GetDebugRootDir()
{
	return FPaths::ProjectSavedDir() / TEXT("BlueprintHelper") / TEXT("Debug");
}

FString FBlueprintHelperReviewDebugBundleService::GetReviewPanelBundleDir()
{
	return GetDebugRootDir() / TEXT("ReviewPanelBundles");
}

FString FBlueprintHelperReviewDebugBundleService::MakeDefaultBundlePath()
{
	return GetReviewPanelBundleDir() / FString::Printf(
		TEXT("review_panel_%s.json"),
		*FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")));
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
	FString NormalizedRoot = GetDebugRootDir();
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
	Json->SetStringField(TEXT("latest_transaction_id"), Change->LatestTransactionId);
	Json->SetStringField(TEXT("change_kind"), BlueprintHelperReviewChangeKindToString(Change->ChangeKind));
	Json->SetStringField(TEXT("status"), BlueprintHelperReviewChangeStatusToString(Change->Status));
	Json->SetStringField(TEXT("scope_identity"), Change->ScopeIdentity);
	Json->SetStringField(TEXT("before_hash"), Change->BeforeHash);
	Json->SetStringField(TEXT("after_hash"), Change->AfterHash);
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
		TargetJson->SetStringField(TEXT("scope_identity"), Target.ScopeIdentity);
		TargetJson->SetStringField(TEXT("first_transaction_id"), Target.FirstTransactionId);
		TargetJson->SetStringField(TEXT("latest_transaction_id"), Target.LatestTransactionId);
		TargetJson->SetStringField(TEXT("baseline_hash"), Target.BaselineHash);
		TargetJson->SetStringField(TEXT("recorded_after_hash"), Target.RecordedAfterHash);
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
	Event->SetStringField(TEXT("message"), SanitizeReviewPanelDebugBundleText(Message));
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
	Event->SetStringField(TEXT("reason"), SanitizeReviewPanelDebugBundleText(Reason));
	Event->SetObjectField(TEXT("change"), BuildChangeSummary(Change));
	return Event;
}

TSharedRef<FJsonObject> FBlueprintHelperReviewDebugBundleService::CreateEmptyBundle(const FString& SessionId)
{
	TSharedRef<FJsonObject> Bundle = MakeShared<FJsonObject>();
	Bundle->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.ReviewPanelDebugBundle.v1"));
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
	if (IsReviewPanelDebugBundleAsyncShutdownRequested())
	{
		SetError(OutError, TEXT("debug bundle async writer is shutting down"));
		return false;
	}

	TFuture<void> WriteTask = Async(EAsyncExecution::ThreadPool, [NormalizedPath, SessionIdCopy, EventCopy]()
	{
		if (IsReviewPanelDebugBundleAsyncShutdownRequested())
		{
			return;
		}
		FScopeLock Lock(&GReviewPanelDebugBundleWriteCriticalSection);
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
	TrackReviewPanelDebugBundleWriteTask(MoveTemp(WriteTask));

	SetError(OutError, FString());
	return true;
}

void FBlueprintHelperReviewDebugBundleService::FlushAsyncWrites()
{
	FlushReviewPanelDebugBundleWriteTasksInternal(false);
}

void FBlueprintHelperReviewDebugBundleService::ShutdownAsyncWrites()
{
	FlushReviewPanelDebugBundleWriteTasksInternal(true);
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
