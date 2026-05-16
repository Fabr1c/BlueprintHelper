// BlueprintHelper Review FBlueprintHelperReviewActionRecordUtils declarations.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/DataTable.h"
#include "Systems/Review/BlueprintHelperReviewActionService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Systems/Transactions/BlueprintHelperTransactionJournalService.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"

class FBlueprintHelperReviewActionRecordUtils
{
public:
	static FBlueprintHelperReviewActionRecord MakeReviewActionRecord(
				const FString& Action,
				const TArray<FString>& TargetKeys,
				const FString& OwnershipPolicy,
				const FString& SourceTransactionId,
				const FString& Message);
	static bool DeleteDebugCasesForReviewRecord(
				const FString& ReviewRecordId,
				const TArray<FString>& ExplicitDebugCaseIds,
				FString& OutError);
	static bool DeleteReviewRecordAndLinkedDebugCases(
				FBlueprintHelperReviewStoreService& Store,
				const FString& ReviewRecordId,
				FString& OutError);
	static bool HasInjectedRejectOptions(const FBlueprintHelperReviewRejectOptions& Options);
	static TSharedRef<FJsonObject> BuildJournalRecordFromPreparedRollbackJournal(
				const FBlueprintHelperReviewPreparedRollbackJournal& Prepared);
	static FBlueprintHelperReviewActionResult MakeRejectFailureResult(
				const FBlueprintHelperReviewVisibleChange& Change,
				EBlueprintHelperReviewChangeStatus Status,
				const FString& Message);
};
