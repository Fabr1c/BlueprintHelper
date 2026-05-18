// BlueprintHelper Review FBlueprintHelperReviewGraphRollbackService declarations.

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

class FBlueprintHelperReviewGraphRollbackService
{
public:
	static bool ExtractRollbackTransactionId(const FString& RollbackDataRef, FString& OutTransactionId);
	static bool LoadJournalRecordForReviewRollback(
				const FString& TransactionId,
				TSharedPtr<FJsonObject>& OutRecord,
				FString& OutError);
	static FString ExtractReviewTargetTail(const FString& TargetKey, const FString& Marker);
	static FString NormalizeReviewGuidCandidate(const FString& Candidate);
	static bool DoesReviewNodeMatchStableId(const UEdGraphNode* Node, const FString& Candidate);
	static UEdGraphPin* FindFirstExecPin(UEdGraphNode* Node, EEdGraphPinDirection Direction);
	static bool NodeMatchesEntryName(UEdGraphNode* Node, const FString& EntryName);
	static bool HasInboundExecLinkFromImportedNode(UEdGraphPin* ExecInputPin, const TSet<UEdGraphNode*>& ImportedNodes);
	static UEdGraphNode* FindFirstExecutableBodyNode(const TSet<UEdGraphNode*>& ImportedNodes);
	static bool PinsHaveSingleConnectionToEachOther(UEdGraphPin* FirstPin, UEdGraphPin* SecondPin);
	static void BreakAllPinLinksWithModify(UEdGraphPin* Pin);
	static bool TryGetRollbackDataObject(
				const TSharedPtr<FJsonObject>& JournalRecord,
				TSharedPtr<FJsonObject>& OutRollbackData,
				FString& OutError);
	static FString ResolveConversionTransactionId(
				const FBlueprintHelperReviewConvertOwnerBlockRequest& Request,
				const FBlueprintHelperTransactionJournalService& JournalService);
	static bool ExecuteBhToUserOwnerBlockConversion(
				const FBlueprintHelperReviewAtomicTarget& Target,
				const FBlueprintHelperReviewConvertOwnerBlockRequest& Request,
				const FString& ConversionTransactionId,
				FString& OutError);
	static UEdGraphNode* FindReviewNodeByAnchor(UEdGraph* Graph, const FString& Anchor);
	static bool ExecuteUserToBhOwnerBlockConversion(
				const FBlueprintHelperReviewAtomicTarget& Target,
				const FBlueprintHelperReviewConvertOwnerBlockRequest& Request,
				const FString& ConversionTransactionId,
				FString& OutError);
	static UEdGraph* FindReviewRollbackGraph(UBlueprint* Blueprint, const FString& GraphName);
	static void CollectRollbackNodesForTarget(
				UEdGraph* Graph,
				const FBlueprintHelperReviewAtomicTarget& Target,
				TArray<UEdGraphNode*>& OutNodes);
	static bool ExecuteGraphAppendRollback(
				const FBlueprintHelperReviewAtomicTarget& Target,
				const TSharedPtr<FJsonObject>& JournalRecord,
				FString& OutError);
};
