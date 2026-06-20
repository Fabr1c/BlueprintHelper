// BlueprintHelper execution receipt lookup store.

#include "Runtime/TaskRuntime/Receipt/BlueprintHelperExecutionReceiptStore.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRunJournalStoreService.h"
#include "Runtime/TaskRuntime/Receipt/BlueprintHelperExecutionReceiptService.h"
#include "Runtime/TaskRuntime/Receipt/BlueprintHelperExecutionReceiptTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool FBlueprintHelperExecutionReceiptStore::FindReceiptById(
	const FString& ReceiptId,
	const TMap<FString, TSharedPtr<FJsonObject>>& InMemoryJournals,
	TSharedPtr<FJsonObject>& OutReceipt,
	FString& OutTaskRunId,
	FString& OutError) const
{
	OutReceipt.Reset();
	OutTaskRunId.Reset();
	OutError.Reset();
	if (ReceiptId.IsEmpty())
	{
		OutError = TEXT("receipt_id is empty.");
		return false;
	}

	for (const TPair<FString, TSharedPtr<FJsonObject>>& Pair : InMemoryJournals)
	{
		if (TryReadReceiptFromJournal(Pair.Value, ReceiptId, OutReceipt, OutTaskRunId))
		{
			return true;
		}
	}

	const FString Directory = FBlueprintHelperTaskRunJournalStoreService::GetTaskRunJournalDirectory();
	TArray<FString> FileNames;
	IFileManager::Get().FindFiles(FileNames, *(Directory / TEXT("*.json")), true, false);
	for (const FString& FileName : FileNames)
	{
		FString JsonText;
		if (!FFileHelper::LoadFileToString(JsonText, *(Directory / FileName)))
		{
			continue;
		}

		TSharedPtr<FJsonObject> Journal;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, Journal) || !Journal.IsValid())
		{
			continue;
		}
		if (TryReadReceiptFromJournal(Journal, ReceiptId, OutReceipt, OutTaskRunId))
		{
			return true;
		}
	}

	OutError = FString::Printf(TEXT("ExecutionReceipt not found: %s."), *ReceiptId);
	return false;
}

bool FBlueprintHelperExecutionReceiptStore::TryReadReceiptFromJournal(
	const TSharedPtr<FJsonObject>& Journal,
	const FString& ReceiptId,
	TSharedPtr<FJsonObject>& OutReceipt,
	FString& OutTaskRunId)
{
	const TSharedPtr<FJsonObject> Receipt = FBlueprintHelperExecutionReceiptService::ReadReceiptFromJournal(Journal);
	if (!Receipt.IsValid())
	{
		return false;
	}

	FString FoundReceiptId;
	if (!Receipt->TryGetStringField(FBlueprintHelperExecutionReceiptFields::ReceiptId(), FoundReceiptId) ||
		FoundReceiptId != ReceiptId)
	{
		return false;
	}

	Journal->TryGetStringField(FBlueprintHelperExecutionReceiptFields::TaskRunId(), OutTaskRunId);
	if (OutTaskRunId.IsEmpty())
	{
		Receipt->TryGetStringField(FBlueprintHelperExecutionReceiptFields::TaskRunId(), OutTaskRunId);
	}
	OutReceipt = Receipt;
	return true;
}
