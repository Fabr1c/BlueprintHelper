// BlueprintHelper Review FBlueprintHelperReviewRejectService declarations.

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

class FBlueprintHelperReviewRejectService
{
public:
	static FBlueprintHelperReviewActionResult RejectVisibleChangeWithDefaultDispatcher(
				const FBlueprintHelperReviewVisibleChange& Change,
				const FBlueprintHelperReviewRejectOptions* Options);
	static FBlueprintHelperReviewCascadeActionResult CascadeRejectLifecycleChildrenAfterRootResult(
				const FBlueprintHelperReviewVisibleChange& Root,
				const TArray<FBlueprintHelperReviewVisibleChange>& PendingChanges,
				const FBlueprintHelperReviewActionResult& RootResult,
				const FString& ResolvedReviewRecordId);
};
