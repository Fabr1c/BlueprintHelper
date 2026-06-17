// BlueprintHelper task run journal persistent store service implementation.

#include "Runtime/TaskRuntime/BlueprintHelperTaskRunJournalStoreService.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

struct FBlueprintHelperTaskRunJournalStoreFileCandidate
{
	FString Path;
	FDateTime ModifiedTime;
};

FString FBlueprintHelperTaskRunJournalStoreService::GetTaskRunJournalDirectory()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("BlueprintHelper"),
		TEXT("TaskRuntime"),
		TEXT("TaskRunJournals"));
}

FString FBlueprintHelperTaskRunJournalStoreService::MakeTaskRunJournalPath(const FString& TaskRunId)
{
	return FPaths::Combine(
		GetTaskRunJournalDirectory(),
		MakeSafeTaskRunFileName(TaskRunId));
}

bool FBlueprintHelperTaskRunJournalStoreService::SaveTaskRunJournal(
	const FString& TaskRunId,
	const TSharedPtr<FJsonObject>& Journal,
	FString& OutError) const
{
	OutError.Reset();
	if (TaskRunId.IsEmpty())
	{
		OutError = TEXT("TaskRunId is empty.");
		return false;
	}
	if (!Journal.IsValid())
	{
		OutError = TEXT("TaskRunJournal is invalid.");
		return false;
	}

	const FString Directory = GetTaskRunJournalDirectory();
	if (!IFileManager::Get().MakeDirectory(*Directory, true))
	{
		OutError = FString::Printf(TEXT("Failed to create TaskRunJournal directory: %s"), *Directory);
		return false;
	}

	FString JsonText;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonText);
	if (!FJsonSerializer::Serialize(Journal.ToSharedRef(), Writer))
	{
		OutError = TEXT("Failed to serialize TaskRunJournal.");
		return false;
	}

	const FString Path = MakeTaskRunJournalPath(TaskRunId);
	if (!FFileHelper::SaveStringToFile(
		JsonText,
		*Path,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("Failed to save TaskRunJournal: %s"), *Path);
		return false;
	}

	return true;
}

bool FBlueprintHelperTaskRunJournalStoreService::LoadTaskRunJournal(
	const FString& TaskRunId,
	TSharedPtr<FJsonObject>& OutJournal,
	FString& OutError) const
{
	OutJournal.Reset();
	OutError.Reset();
	if (TaskRunId.IsEmpty())
	{
		OutError = TEXT("TaskRunId is empty.");
		return false;
	}

	const FString Path = MakeTaskRunJournalPath(TaskRunId);
	if (!FPaths::FileExists(Path))
	{
		OutError = FString::Printf(TEXT("TaskRunJournal file does not exist: %s"), *Path);
		return false;
	}

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *Path))
	{
		OutError = FString::Printf(TEXT("Failed to load TaskRunJournal: %s"), *Path);
		return false;
	}

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, OutJournal) || !OutJournal.IsValid())
	{
		OutError = FString::Printf(TEXT("Failed to parse TaskRunJournal: %s"), *Path);
		OutJournal.Reset();
		return false;
	}

	return true;
}

TArray<TSharedPtr<FJsonObject>> FBlueprintHelperTaskRunJournalStoreService::QueryTaskRunJournalsForTargetAssets(
	const TArray<FString>& TargetAssets,
	int32 MaxResults) const
{
	TArray<TSharedPtr<FJsonObject>> Results;
	if (TargetAssets.Num() == 0 || MaxResults <= 0)
	{
		return Results;
	}

	const FString Directory = GetTaskRunJournalDirectory();
	TArray<FString> FileNames;
	IFileManager::Get().FindFiles(
		FileNames,
		*(Directory / TEXT("*.json")),
		true,
		false);
	if (FileNames.Num() == 0)
	{
		return Results;
	}

	TArray<FBlueprintHelperTaskRunJournalStoreFileCandidate> Candidates;
	for (const FString& FileName : FileNames)
	{
		FBlueprintHelperTaskRunJournalStoreFileCandidate Candidate;
		Candidate.Path = Directory / FileName;
		Candidate.ModifiedTime = IFileManager::Get().GetTimeStamp(*Candidate.Path);
		Candidates.Add(Candidate);
	}
	Candidates.Sort(
		[](const FBlueprintHelperTaskRunJournalStoreFileCandidate& Left,
			const FBlueprintHelperTaskRunJournalStoreFileCandidate& Right)
		{
			return Left.ModifiedTime > Right.ModifiedTime;
		});

	for (const FBlueprintHelperTaskRunJournalStoreFileCandidate& Candidate : Candidates)
	{
		FString JsonText;
		if (!FFileHelper::LoadFileToString(JsonText, *Candidate.Path))
		{
			continue;
		}

		TSharedPtr<FJsonObject> Journal;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, Journal) || !Journal.IsValid())
		{
			continue;
		}

		TArray<FString> JournalTargetAssets;
		ReadStringArrayField(Journal, TEXT("target_assets"), JournalTargetAssets);
		if (!HasAnySharedString(TargetAssets, JournalTargetAssets))
		{
			continue;
		}

		Results.Add(Journal);
		if (Results.Num() >= MaxResults)
		{
			return Results;
		}
	}

	return Results;
}

bool FBlueprintHelperTaskRunJournalStoreService::DeleteTaskRunJournal(
	const FString& TaskRunId,
	FString& OutError) const
{
	OutError.Reset();
	if (TaskRunId.IsEmpty())
	{
		OutError = TEXT("TaskRunId is empty.");
		return false;
	}

	const FString Path = MakeTaskRunJournalPath(TaskRunId);
	if (!FPaths::FileExists(Path))
	{
		return true;
	}

	if (!IFileManager::Get().Delete(*Path, false, true))
	{
		OutError = FString::Printf(TEXT("Failed to delete TaskRunJournal: %s"), *Path);
		return false;
	}

	return true;
}

FString FBlueprintHelperTaskRunJournalStoreService::MakeSafeTaskRunFileName(const FString& TaskRunId)
{
	FString FileName = FPaths::MakeValidFileName(TaskRunId);
	if (FileName.IsEmpty())
	{
		FileName = TEXT("unknown_task_run");
	}
	return FileName + TEXT(".json");
}

bool FBlueprintHelperTaskRunJournalStoreService::ReadStringArrayField(
	const TSharedPtr<FJsonObject>& Json,
	const FString& FieldName,
	TArray<FString>& OutValues)
{
	OutValues.Reset();
	const TArray<TSharedPtr<FJsonValue>>* RawValues = nullptr;
	if (!Json.IsValid() ||
		!Json->TryGetArrayField(FieldName, RawValues) ||
		!RawValues)
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& RawValue : *RawValues)
	{
		if (!RawValue.IsValid() || RawValue->Type != EJson::String)
		{
			continue;
		}
		OutValues.AddUnique(RawValue->AsString());
	}
	return OutValues.Num() > 0;
}

bool FBlueprintHelperTaskRunJournalStoreService::HasAnySharedString(
	const TArray<FString>& Left,
	const TArray<FString>& Right)
{
	for (const FString& Value : Left)
	{
		if (Right.Contains(Value))
		{
			return true;
		}
	}
	return false;
}
