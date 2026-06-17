// BlueprintHelper task run journal persistent store service.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;

class BLUEPRINTHELPER_API FBlueprintHelperTaskRunJournalStoreService
{
public:
	static FString GetTaskRunJournalDirectory();
	static FString MakeTaskRunJournalPath(const FString& TaskRunId);

	bool SaveTaskRunJournal(
		const FString& TaskRunId,
		const TSharedPtr<FJsonObject>& Journal,
		FString& OutError) const;

	bool LoadTaskRunJournal(
		const FString& TaskRunId,
		TSharedPtr<FJsonObject>& OutJournal,
		FString& OutError) const;

	TArray<TSharedPtr<FJsonObject>> QueryTaskRunJournalsForTargetAssets(
		const TArray<FString>& TargetAssets,
		int32 MaxResults) const;

	bool DeleteTaskRunJournal(
		const FString& TaskRunId,
		FString& OutError) const;

private:
	static FString MakeSafeTaskRunFileName(const FString& TaskRunId);
	static bool ReadStringArrayField(
		const TSharedPtr<FJsonObject>& Json,
		const FString& FieldName,
		TArray<FString>& OutValues);
	static bool HasAnySharedString(
		const TArray<FString>& Left,
		const TArray<FString>& Right);
};
