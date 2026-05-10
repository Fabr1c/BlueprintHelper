// Facade for best-effort DebugCase event recording.

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Shared/Debug/BlueprintHelperDebugTypes.h"

class FBlueprintHelperDebugCaseStoreService;
class FBlueprintHelperReviewStoreService;

struct FBlueprintHelperDebugEntryEventInput
{
	FString SourceLayer;
	FString Source;
	FString Operation;
	FString Stage;
	EBlueprintHelperDebugSeverity Severity = EBlueprintHelperDebugSeverity::Error;
	FString TraceId;
	FString TaskRunId;
	TArray<FString> AssetPaths;
	TArray<FString> ReviewRecordIds;
	TArray<FBlueprintHelperDebugTransactionLink> TransactionLinks;
	FBlueprintHelperDebugError Error;
	FString RecommendedNext;
	TSharedPtr<FJsonObject> ToolResultSummary;
};

struct FBlueprintHelperDebugEntryRecordResult
{
	bool bRecorded = false;
	FString DebugCaseId;
	FString DebugEventId;
	FString ErrorMessage;
};

class BLUEPRINTHELPER_API FBlueprintHelperDebugEntryService
{
public:
	explicit FBlueprintHelperDebugEntryService(
		const FBlueprintHelperDebugCaseStoreService& InStore,
		const FBlueprintHelperReviewStoreService* InReviewStore = nullptr);

	FBlueprintHelperDebugEntryRecordResult RecordEventBestEffort(const FBlueprintHelperDebugEntryEventInput& Input) const;
	void AttachDebugCaseToFailureBestEffort(
		FBlueprintHelperToolResultBase& Result,
		const FBlueprintHelperDebugEntryEventInput& Input) const;
	FBlueprintHelperToolResultBase GetDebugCaseSummaryResult(const TSharedPtr<FJsonObject>& Payload) const;
	FBlueprintHelperToolResultBase GetDebugCaseListResult(const TSharedPtr<FJsonObject>& Payload) const;
	FBlueprintHelperToolResultBase ExportDebugBundleSummaryResult(const TSharedPtr<FJsonObject>& Payload) const;
	FBlueprintHelperToolResultBase CleanupDebugCasesResult(const TSharedPtr<FJsonObject>& Payload) const;

private:
	const FBlueprintHelperDebugCaseStoreService& Store;
	const FBlueprintHelperReviewStoreService* ReviewStore = nullptr;

	static FString NewDebugCaseId();
	static FString NewDebugEventId();
	static FString UtcTimestamp();
};
