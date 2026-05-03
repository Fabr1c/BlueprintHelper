// BlueprintHelper Service Layer — RollbackCleanupTransaction 核心服务

#pragma once

#include "CoreMinimal.h"
#include "Services/BlueprintHelperRollbackCleanupTypes.h"
#include "Services/BlueprintHelperToolResultTypes.h"

class FBlueprintHelperGraphResolver;
class FBlueprintHelperTransactionJournalService;
class FJsonObject;

class BLUEPRINTHELPER_API FBlueprintHelperRollbackCleanupTransactionService
{
public:
	FBlueprintHelperRollbackCleanupTransactionService(
		const FBlueprintHelperGraphResolver& InResolver,
		const FBlueprintHelperTransactionJournalService& InJournalService);

	FBlueprintHelperToolResultBase Execute(const TSharedPtr<FJsonObject>& Payload) const;

private:
	struct FRollbackRequest
	{
		FString TransactionId, AssetPath;
		EBlueprintHelperRollbackScope RollbackScope = EBlueprintHelperRollbackScope::CleanupTransaction;
		EBlueprintHelperAlreadyRolledBackPolicy AlreadyRolledBackPolicy = EBlueprintHelperAlreadyRolledBackPolicy::Error;
		bool bDryRun = true;
	};

	struct FRollbackPreflightResult
	{
		bool bPassed = true;
		TArray<FString> BlockedBy;
		TArray<FBlueprintHelperDryRunIssue> Conflicts;
		TArray<FBlueprintHelperDryRunIssue> Errors;
		FBlueprintHelperRollbackSummary Summary;
		bool bAlreadyRolledBack = false;
		FString SourceAssetPath;
	};

	FRollbackRequest ParseRequest(const TSharedPtr<FJsonObject>& Payload) const;
	FRollbackPreflightResult Preflight(const FRollbackRequest& Request) const;
	FBlueprintHelperToolResultBase ExecuteDryRun(const FRollbackRequest& Request) const;
	FBlueprintHelperToolResultBase ExecuteWrite(const FRollbackRequest& Request) const;

	bool LoadJournalRecord(const FString& TransactionId, TSharedPtr<FJsonObject>& OutRecord, FString& OutError) const;
	bool ValidateCleanupTransaction(const TSharedPtr<FJsonObject>& Record, FRollbackPreflightResult& OutResult) const;
	bool CheckRollbackData(const TSharedPtr<FJsonObject>& Record, FRollbackPreflightResult& OutResult) const;

	const FBlueprintHelperGraphResolver& Resolver;
	const FBlueprintHelperTransactionJournalService& JournalService;
};
