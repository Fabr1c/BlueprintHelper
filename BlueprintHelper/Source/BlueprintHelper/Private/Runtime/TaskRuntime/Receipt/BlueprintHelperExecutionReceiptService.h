// BlueprintHelper execution receipt JSON service.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;

class FBlueprintHelperExecutionReceiptService
{
public:
	static TSharedPtr<FJsonObject> CloneJsonObject(const TSharedPtr<FJsonObject>& Source);

	static TSharedPtr<FJsonObject> ReadReceiptFromPayload(const TSharedPtr<FJsonObject>& Payload);
	static TSharedPtr<FJsonObject> ReadReceiptFromJournal(const TSharedPtr<FJsonObject>& Journal);

	static TSharedPtr<FJsonObject> BuildPreviewReceipt(
		const TSharedPtr<FJsonObject>& PreviewTokenRequest,
		const TSharedPtr<FJsonObject>& TaskPlan,
		bool bPassed);

	static TSharedPtr<FJsonObject> BuildExecuteReceipt(
		const TSharedPtr<FJsonObject>& Payload,
		const FString& TaskRunId,
		bool bOk,
		const FString& JournalRef);

	static void AttachReceiptToResultData(
		TSharedPtr<FJsonObject>& Data,
		const TSharedPtr<FJsonObject>& Receipt);

	static void AttachReceiptToJournal(
		const TSharedPtr<FJsonObject>& Journal,
		const TSharedPtr<FJsonObject>& Receipt);
};
