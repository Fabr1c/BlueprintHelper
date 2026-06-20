// BlueprintHelper execution receipt lookup store.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;

class FBlueprintHelperExecutionReceiptStore
{
public:
	bool FindReceiptById(
		const FString& ReceiptId,
		const TMap<FString, TSharedPtr<FJsonObject>>& InMemoryJournals,
		TSharedPtr<FJsonObject>& OutReceipt,
		FString& OutTaskRunId,
		FString& OutError) const;

private:
	static bool TryReadReceiptFromJournal(
		const TSharedPtr<FJsonObject>& Journal,
		const FString& ReceiptId,
		TSharedPtr<FJsonObject>& OutReceipt,
		FString& OutTaskRunId);
};
