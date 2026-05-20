// BlueprintHelper Service Layer 。ReplaceBlueprintGraph 核心服务

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperServiceTypes.h"
#include "Shared/GraphWrite/BlueprintHelperReplaceGraphTypes.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FBlueprintHelperGraphResolver;
class FBlueprintHelperBlockIdService;
class FBlueprintHelperOwnershipService;
class FBlueprintHelperGraphSnapshotService;
class UEdGraph;
class UBlueprint;
class FJsonObject;

/**
 * ReplaceBlueprintGraph 核心服务。 * 替换指定目标的完整实现：owned block / function body / event body / custom event body。 */
class BLUEPRINTHELPER_API FBlueprintHelperReplaceBlueprintGraphService
{
public:
	FBlueprintHelperReplaceBlueprintGraphService(
		const FBlueprintHelperGraphResolver& InResolver,
		const FBlueprintHelperBlockIdService& InBlockIdService,
		const FBlueprintHelperOwnershipService& InOwnershipService,
		const FBlueprintHelperGraphSnapshotService& InSnapshotService);

	/** 执行 Replace 操作。*/
	FBlueprintHelperToolResultBase Execute(const TSharedPtr<FJsonObject>& Payload) const;

private:
	// ─── 内部请求模型 ───

	struct FReplaceRequest
	{
		FString AssetPath;
		FString GraphName;
		EBlueprintHelperReplaceScope Scope = EBlueprintHelperReplaceScope::BlockImplementation;

		FString BlockId;
		FString TargetRef;
		FString EntryName;
		FString NodePath;

		TSharedPtr<FJsonObject> LogicSpec;
		bool bDryRun = false;
		bool bStrict = true;
		// DEPRECATED_LAYOUT: preserve_layout is legacy GraphWrite behavior. GraphLayout owns final placement.
		bool bPreserveLayout = false;
	};

	struct FReplacePreflightResult
	{
		bool bPassed = true;
		TArray<FString> BlockedBy;
		TArray<FBlueprintHelperGraphWriteIssue> Conflicts;
		TArray<FBlueprintHelperGraphWriteIssue> Errors;
		TSharedPtr<FJsonObject> FragmentDebugData;
	};

	// ─── 解析 ───

	FReplaceRequest ParseRequest(const TSharedPtr<FJsonObject>& Payload) const;

	// ─── Preflight ───

	FReplacePreflightResult Preflight(const FReplaceRequest& Request) const;
	bool PreflightBlueprint(const FString& AssetPath, UBlueprint*& OutBlueprint, FReplacePreflightResult& OutResult) const;
	bool PreflightLogicSpec(const FReplaceRequest& Request, UBlueprint* Blueprint, FReplacePreflightResult& OutResult) const;
	bool PreflightGraphTarget(UBlueprint* Blueprint, const FString& GraphName, EBlueprintHelperReplaceScope Scope, UEdGraph*& OutGraph, FReplacePreflightResult& OutResult) const;
	bool PreflightReplaceScope(EBlueprintHelperReplaceScope Scope, FReplacePreflightResult& OutResult) const;

	// ─── DryRun ───

	FBlueprintHelperToolResultBase ExecuteDryRun(const FReplaceRequest& Request) const;

	// ─── 正式写入 ───

	FBlueprintHelperToolResultBase ExecuteWrite(const FReplaceRequest& Request) const;

	// ─── 目标解析 ───

	struct FResolvedReplaceTarget
	{
		UBlueprint* Blueprint = nullptr;
		UEdGraph* Graph = nullptr;
		EBlueprintHelperReplaceScope Scope;

		FString AssetPath;
		FString GraphName;
		FString GraphId;
		FString TargetRef;

		FString OriginalBlockId;
		FString OriginalBlockRef;
		bool bIsBlueprintHelperOwned = false;

		TArray<UEdGraphNode*> NodesToDelete;
		TArray<UEdGraphNode*> NodesToPreserve;
		TArray<UEdGraphNode*> ExistingOwnedNodes;
		TArray<UEdGraphNode*> ExistingUserNodes;

		bool bExternalDependentsMayBreak = false;
	};

	bool ResolveReplaceTarget(const FReplaceRequest& Request, UBlueprint* Blueprint, FResolvedReplaceTarget& OutTarget, FString& OutError) const;
	bool ResolveBlockImplementation(UEdGraph* Graph, const FReplaceRequest& Request, FResolvedReplaceTarget& OutTarget, FString& OutError) const;

	// GraphWrite SemanticIR payload

	FString BuildSemanticGraphWritePayload(const FReplaceRequest& Request) const;

	// ─── 删除旧实。───

	bool DeleteOldImplementation(UBlueprint* Blueprint, UEdGraph* Graph, const TArray<UEdGraphNode*>& NodesToDelete) const;
	bool ReconnectPreservedEntryToNewBody(
		const FReplaceRequest& Request,
		const FResolvedReplaceTarget& Resolved,
		const TSet<UEdGraphNode*>& NodesBeforeImport,
		FString& OutError) const;

	const FBlueprintHelperGraphResolver& Resolver;
	const FBlueprintHelperBlockIdService& BlockIdService;
	const FBlueprintHelperOwnershipService& OwnershipService;
	const FBlueprintHelperGraphSnapshotService& SnapshotService;
};
