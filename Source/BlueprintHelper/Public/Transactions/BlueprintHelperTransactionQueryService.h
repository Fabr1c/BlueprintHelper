// BlueprintHelper Service Layer — Transaction Query 服务

#pragma once

#include "CoreMinimal.h"
#include "Structure/BlueprintHelperTransactionQueryTypes.h"
#include "Structure/BlueprintHelperToolResultTypes.h"

class FJsonObject;

class BLUEPRINTHELPER_API FBlueprintHelperTransactionQueryService
{
public:
	FBlueprintHelperTransactionQueryService() = default;

	FBlueprintHelperToolResultBase List(const TSharedPtr<FJsonObject>& Payload) const;
	FBlueprintHelperToolResultBase Read(const TSharedPtr<FJsonObject>& Payload) const;

private:
	struct FListRequest { FString AssetPath, QueryScope; int32 Limit = 20; };

	struct FReadRequest { FString TransactionId; FString DetailLevel; };

	FListRequest ParseListRequest(const TSharedPtr<FJsonObject>& Payload) const;
	FReadRequest ParseReadRequest(const TSharedPtr<FJsonObject>& Payload) const;
	bool LoadJournalIndex(TArray<FBlueprintHelperTransactionListItem>& OutItems) const;
	bool ReadJournalFile(const FString& TransactionId, TSharedPtr<FJsonObject>& OutRecord) const;
	void BuildReadSummary(const TSharedPtr<FJsonObject>& Record, FBlueprintHelperTransactionSummaryRecord& Out) const;
};
