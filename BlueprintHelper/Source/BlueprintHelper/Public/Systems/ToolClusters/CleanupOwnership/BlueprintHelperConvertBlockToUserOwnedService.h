// BlueprintHelper Service Layer 。ConvertBlockToUserOwned 核心服务

#pragma once

#include "CoreMinimal.h"
#include "Shared/CleanupOwnership/BlueprintHelperConvertBlockToUserOwnedTypes.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FBlueprintHelperGraphResolver;
class FBlueprintHelperOwnershipService;
class FBlueprintHelperTransactionJournalService;
class UEdGraph;
class UBlueprint;
class UEdGraphNode;
class FJsonObject;

class BLUEPRINTHELPER_API FBlueprintHelperConvertBlockToUserOwnedService
{
public:
	FBlueprintHelperConvertBlockToUserOwnedService(
		const FBlueprintHelperGraphResolver& InResolver,
		const FBlueprintHelperOwnershipService& InOwnershipService,
		const FBlueprintHelperTransactionJournalService& InJournalService);

	FBlueprintHelperToolResultBase Execute(const TSharedPtr<FJsonObject>& Payload) const;

private:
	struct FConvertRequest
	{
		FString AssetPath, GraphName, GraphId, BlockRef, BlockId, TransactionId;
		EBlueprintHelperOwnershipScope OwnershipScope = EBlueprintHelperOwnershipScope::Block;
		EBlueprintHelperAlreadyUserOwnedPolicy AlreadyUserOwnedPolicy = EBlueprintHelperAlreadyUserOwnedPolicy::Error;
		bool bDryRun = false;

		FString GetEffectiveBlockId() const
		{
			if (!BlockId.IsEmpty()) return BlockId;
			const FString GId = GraphId.IsEmpty() ? GraphName : GraphId;
			if (!GId.IsEmpty() && !BlockRef.IsEmpty())
				return FString::Printf(TEXT("%s_%s"), *GId, *BlockRef);
			return FString();
		}
	};

	struct FResolvedBlock
	{
		UBlueprint* Blueprint = nullptr;
		UEdGraph* Graph = nullptr;
		FString BlockId, GraphName;
		TArray<UEdGraphNode*> BlockNodes;
		bool bFound = false, bIsOwned = false, bAlreadyUserOwned = false;
	};

	struct FConvertPreflightResult
	{
		bool bPassed = true;
		TArray<FString> BlockedBy;
		TArray<FBlueprintHelperDryRunIssue> Conflicts;
		TArray<FBlueprintHelperDryRunIssue> Errors;
	};

	FConvertRequest ParseRequest(const TSharedPtr<FJsonObject>& Payload) const;
	FConvertPreflightResult Preflight(const FConvertRequest& Request, FResolvedBlock& OutTarget) const;
	FBlueprintHelperToolResultBase ExecuteDryRun(const FConvertRequest& Request) const;
	FBlueprintHelperToolResultBase ExecuteWrite(const FConvertRequest& Request) const;

	bool ResolveBlock(const FConvertRequest& Request, FResolvedBlock& OutTarget, FString& OutError) const;
	bool ConvertOwnershipMetadata(const TArray<UEdGraphNode*>& Nodes, const FString& BlockId, FString& OutError) const;
	void StripManagedNodeComment(UEdGraphNode* Node) const;
	void MakeConvertTargetJson(const FConvertRequest& Req, FBlueprintHelperToolResultBase& Out) const;

	const FBlueprintHelperGraphResolver& Resolver;
	const FBlueprintHelperOwnershipService& OwnershipService;
	const FBlueprintHelperTransactionJournalService& JournalService;
};
