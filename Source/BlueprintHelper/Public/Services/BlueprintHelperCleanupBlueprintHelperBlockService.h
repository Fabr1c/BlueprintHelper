// BlueprintHelper Service Layer — CleanupBlueprintHelperBlock 核心服务

#pragma once

#include "CoreMinimal.h"
#include "Structure/BlueprintHelperCleanupBlockTypes.h"
#include "Structure/BlueprintHelperToolResultTypes.h"

class FBlueprintHelperGraphResolver;
class FBlueprintHelperTransactionJournalService;
class UEdGraph;
class UBlueprint;
class UEdGraphNode;
class FJsonObject;

class BLUEPRINTHELPER_API FBlueprintHelperCleanupBlueprintHelperBlockService
{
public:
	FBlueprintHelperCleanupBlueprintHelperBlockService(
		const FBlueprintHelperGraphResolver& InResolver,
		const FBlueprintHelperTransactionJournalService& InJournalService);

	FBlueprintHelperToolResultBase Execute(const TSharedPtr<FJsonObject>& Payload) const;

private:
	struct FCleanupRequest
	{
		FString AssetPath, GraphName, BlockRef, BlockId;
		EBlueprintHelperMissingPolicy MissingPolicy = EBlueprintHelperMissingPolicy::Error;
		bool bDryRun = false;

		FString GetEffectiveBlockId() const
		{
			if (!BlockId.IsEmpty()) return BlockId;
			if (!GraphName.IsEmpty() && !BlockRef.IsEmpty())
				return FString::Printf(TEXT("%s_%s"), *GraphName, *BlockRef);
			return FString();
		}
	};

	struct FResolvedBlockTarget
	{
		UBlueprint* Blueprint = nullptr;
		UEdGraph* Graph = nullptr;
		FString BlockId, BlockRef, GraphName;
		TArray<UEdGraphNode*> OwnedNodes;
		TArray<UEdGraphNode*> ConflictingNodes;
		bool bFound = false, bOwned = true;
		bool bHasExternalDependents = false;
	};

	struct FCleanupPreflightResult
	{
		bool bPassed = true;
		TArray<FString> BlockedBy;
		TArray<FBlueprintHelperDryRunIssue> Conflicts;
		TArray<FBlueprintHelperDryRunIssue> Errors;
	};

	FCleanupRequest ParseRequest(const TSharedPtr<FJsonObject>& Payload) const;
	FCleanupPreflightResult Preflight(const FCleanupRequest& Request, FResolvedBlockTarget& OutTarget) const;
	FBlueprintHelperToolResultBase ExecuteDryRun(const FCleanupRequest& Request) const;
	FBlueprintHelperToolResultBase ExecuteWrite(const FCleanupRequest& Request) const;

	bool ResolveBlock(const FCleanupRequest& Request, FResolvedBlockTarget& OutTarget, FString& OutError) const;
	bool CheckOwnership(const FResolvedBlockTarget& Target, FCleanupPreflightResult& OutResult) const;
	bool CheckDependencies(const FResolvedBlockTarget& Target, FCleanupPreflightResult& OutResult) const;
	bool DeleteOwnedNodes(UBlueprint* BP, UEdGraph* Graph, const TArray<UEdGraphNode*>& Nodes, FString& OutError) const;
	void MakeCleanupTargetJson(const FCleanupRequest& Req, const FResolvedBlockTarget& Tgt, FBlueprintHelperToolResultBase& Out) const;

	const FBlueprintHelperGraphResolver& Resolver;
	const FBlueprintHelperTransactionJournalService& JournalService;
};
