// BlueprintHelper Service Layer — ReplaceBlueprintGraph 核心服务

#pragma once

#include "CoreMinimal.h"
#include "Structure/BlueprintHelperServiceTypes.h"
#include "Structure/BlueprintHelperReplaceGraphTypes.h"
#include "Structure/BlueprintHelperToolResultTypes.h"

class FBlueprintHelperGraphResolver;
class FBlueprintHelperAgentImportService;
class FBlueprintHelperBlockIdService;
class FBlueprintHelperOwnershipService;
class FBlueprintHelperTransactionJournalService;
class FBlueprintHelperGraphSnapshotService;
class UEdGraph;
class UBlueprint;
class FJsonObject;

/**
 * ReplaceBlueprintGraph 核心服务。
 * 替换指定目标的完整实现：owned block / function body / event body / custom event body。
 */
class BLUEPRINTHELPER_API FBlueprintHelperReplaceBlueprintGraphService
{
public:
	FBlueprintHelperReplaceBlueprintGraphService(
		const FBlueprintHelperGraphResolver& InResolver,
		const FBlueprintHelperAgentImportService& InAgentImportService,
		const FBlueprintHelperBlockIdService& InBlockIdService,
		const FBlueprintHelperOwnershipService& InOwnershipService,
		const FBlueprintHelperTransactionJournalService& InJournalService,
		const FBlueprintHelperGraphSnapshotService& InSnapshotService);

	/** 执行 Replace 操作。 */
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

		TArray<TSharedPtr<FJsonValue>> Nodes;
		TArray<TSharedPtr<FJsonValue>> Links;
		bool bDryRun = false;
		bool bStrict = true;
		bool bPreserveLayout = false;
	};

	struct FReplacePreflightResult
	{
		bool bPassed = true;
		TArray<FString> BlockedBy;
		TArray<FBlueprintHelperGraphWriteIssue> Conflicts;
		TArray<FBlueprintHelperGraphWriteIssue> Errors;
	};

	// ─── 解析 ───

	FReplaceRequest ParseRequest(const TSharedPtr<FJsonObject>& Payload) const;

	// ─── Preflight ───

	FReplacePreflightResult Preflight(const FReplaceRequest& Request) const;
	bool PreflightBlueprint(const FString& AssetPath, UBlueprint*& OutBlueprint, FReplacePreflightResult& OutResult) const;
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

	// ─── 构建 AgentImport 兼容 payload ───

	FString BuildAgentImportPayload(const FReplaceRequest& Request) const;

	// ─── 删除旧实现 ───

	bool DeleteOldImplementation(UBlueprint* Blueprint, UEdGraph* Graph, const TArray<UEdGraphNode*>& NodesToDelete) const;

	const FBlueprintHelperGraphResolver& Resolver;
	const FBlueprintHelperAgentImportService& AgentImportService;
	const FBlueprintHelperBlockIdService& BlockIdService;
	const FBlueprintHelperOwnershipService& OwnershipService;
	const FBlueprintHelperTransactionJournalService& JournalService;
	const FBlueprintHelperGraphSnapshotService& SnapshotService;
};
