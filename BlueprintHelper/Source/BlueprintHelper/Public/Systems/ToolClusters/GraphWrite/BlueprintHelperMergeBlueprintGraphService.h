// BlueprintHelper Service Layer 。MergeBlueprintGraph 核心服务

#pragma once

#include "CoreMinimal.h"
#include "Shared/GraphWrite/BlueprintHelperMergeGraphTypes.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FBlueprintHelperGraphResolver;
class FBlueprintHelperLogicJsonPathService;
class FBlueprintHelperTransactionJournalService;
class UEdGraph;
class UBlueprint;
class UEdGraphNode;
class UEdGraphPin;
class FJsonObject;

class BLUEPRINTHELPER_API FBlueprintHelperMergeBlueprintGraphService
{
public:
	FBlueprintHelperMergeBlueprintGraphService(
		const FBlueprintHelperGraphResolver& InResolver,
		const FBlueprintHelperLogicJsonPathService& InPathService,
		const FBlueprintHelperTransactionJournalService& InJournalService);

	FBlueprintHelperToolResultBase Execute(const TSharedPtr<FJsonObject>& Payload) const;

private:
	struct FMergeRequest
	{
		FString AssetPath, GraphName;
		EBlueprintHelperMergeScope MergeScope;
		EBlueprintHelperInsertStrategy InsertStrategy;
		FString AnchorBlockId, AnchorGroupEntryNodePath;
		FString AnchorNodeRef, AnchorPinRef, AnchorNodePath, AnchorPinPath;
		FString InsertedBlockId, InsertedBlockRef, InsertedFunctionName, InsertedCustomEventName;
		TSharedPtr<FJsonObject> LogicSpec;
		TArray<FString> SequenceOrder;
		bool bAllowCompileBeforeCall = false;
		bool bDryRun = false;
	};

	struct FMergeContext
	{
		UBlueprint* Blueprint = nullptr;
		UEdGraph* Graph = nullptr;
		UEdGraphNode* AnchorNode = nullptr;
		UEdGraphPin* AnchorPin = nullptr;
		TArray<UEdGraphPin*> Successors;
		UEdGraphPin* OriginalSuccessorPin = nullptr;
		FString InsertedRef;
		UEdGraphNode* InsertedNode = nullptr;
		UEdGraphNode* SequenceNode = nullptr;
	};

	struct FMergePreflightResult
	{
		bool bPassed = true;
		TArray<FString> BlockedBy;
		TArray<FBlueprintHelperDryRunIssue> Conflicts;
		TArray<FBlueprintHelperDryRunIssue> Errors;
		TSharedPtr<FJsonObject> FragmentDebugData;
	};

	FMergeRequest ParseRequest(const TSharedPtr<FJsonObject>& Payload) const;
	FMergePreflightResult Preflight(
		const FMergeRequest& Request,
		FMergeContext& Context,
		bool bAllowInsertedLogicRequiresCompile) const;
	bool PreflightLogicSpec(const FMergeRequest& Request, UBlueprint* Blueprint, FMergePreflightResult& OutResult) const;
	FBlueprintHelperToolResultBase ExecuteDryRun(const FMergeRequest& Request) const;
	FBlueprintHelperToolResultBase ExecuteWrite(const FMergeRequest& Request) const;

	bool ResolveAnchor(const FMergeRequest& Request, FMergeContext& Context, FString& OutError) const;
	bool ResolveInsertedLogic(const FMergeRequest& Request, FMergeContext& Context, FString& OutErrorCode, FString& OutError) const;
	bool CheckSuccessorCount(const FMergeRequest& Request, const FMergeContext& Context, FMergePreflightResult& OutResult) const;
	bool ApplyAppendAfter(UBlueprint* BP, UEdGraph* Graph, const FMergeRequest& Request, FMergeContext& Context, FString& OutError) const;
	bool ApplyInsertBetween(UBlueprint* BP, UEdGraph* Graph, const FMergeRequest& Request, FMergeContext& Context, FString& OutError) const;
	bool ApplyBranchFork(UBlueprint* BP, UEdGraph* Graph, const FMergeRequest& Request, FMergeContext& Context, FString& OutError) const;

	const FBlueprintHelperGraphResolver& Resolver;
	const FBlueprintHelperLogicJsonPathService& PathService;
	const FBlueprintHelperTransactionJournalService& JournalService;
};
