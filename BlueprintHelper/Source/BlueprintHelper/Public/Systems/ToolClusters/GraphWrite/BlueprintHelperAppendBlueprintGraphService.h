// BlueprintHelper Service Layer 。AppendBlueprintGraph 核心服务

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperServiceTypes.h"
#include "Shared/GraphWrite/BlueprintHelperAppendGraphTypes.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FBlueprintHelperGraphResolver;
class FBlueprintHelperAgentImportService;
class FBlueprintHelperBlockIdService;
class FBlueprintHelperOwnershipService;
class FBlueprintHelperTransactionJournalService;
class UEdGraph;
class UBlueprint;
class FJsonObject;

/**
 * AppendBlueprintGraph 核心服务。 * 支持创建。EG_{FeatureName} 图表或向空图表追加独。BlueprintHelper-owned block。 */
class BLUEPRINTHELPER_API FBlueprintHelperAppendBlueprintGraphService
{
public:
	FBlueprintHelperAppendBlueprintGraphService(
		const FBlueprintHelperGraphResolver& InResolver,
		const FBlueprintHelperAgentImportService& InAgentImportService,
		const FBlueprintHelperBlockIdService& InBlockIdService,
		const FBlueprintHelperOwnershipService& InOwnershipService,
		const FBlueprintHelperTransactionJournalService& InJournalService);

	/** 执行 Append 操作，返回统一 ToolResultBase。*/
	FBlueprintHelperToolResultBase Execute(const TSharedPtr<FJsonObject>& Payload) const;

private:
	// ─── 内部请求/预检结构 ───

	struct FAppendRequest
	{
		FString AssetPath;
		FString GraphName;
		FString FeatureName;
		bool bDryRun = false;
		bool bReuseExistingEntries = false;
		TArray<TSharedPtr<FJsonValue>> Nodes;
		TArray<TSharedPtr<FJsonValue>> Links;
	};

	struct FAppendPreflightResult
	{
		bool bPassed = true;
		TArray<FString> BlockedBy;
		TArray<FBlueprintHelperDryRunIssue> Conflicts;
		TArray<FBlueprintHelperDryRunIssue> Errors;
	};

	// ─── 解析 ───

	FAppendRequest ParseRequest(const TSharedPtr<FJsonObject>& Payload) const;

	// ─── Preflight ───

	FAppendPreflightResult Preflight(const FAppendRequest& Request) const;
	bool PreflightBlueprint(const FString& AssetPath, UBlueprint*& OutBlueprint, FAppendPreflightResult& OutResult) const;
	bool PreflightGraphTarget(UBlueprint* Blueprint, const FAppendRequest& Request, UEdGraph*& OutGraph, FAppendPreflightResult& OutResult) const;
	bool PreflightNodePayload(const FAppendRequest& Request, UEdGraph* Graph, FAppendPreflightResult& OutResult) const;

	// ─── 图表操作 ───

	UEdGraph* FindOrCreateAppendGraph(UBlueprint* Blueprint, const FString& GraphName, FString& OutError) const;
	bool IsEventGraph(UEdGraph* Graph) const;

	// ─── 执行 ───

	FBlueprintHelperToolResultBase ExecuteDryRun(const FAppendRequest& Request) const;
	FBlueprintHelperToolResultBase ExecuteWrite(const FAppendRequest& Request) const;

	// ─── 构建 AgentImport 兼容 JSON ───

	FString BuildAgentImportPayload(const FAppendRequest& Request) const;

	// ─── Helpers ───

	bool IsForbiddenEventKind(const FString& Kind, const FString& EventName) const;
	TArray<FString> ExtractCustomEventNames(const FAppendRequest& Request) const;

	const FBlueprintHelperGraphResolver& Resolver;
	const FBlueprintHelperAgentImportService& AgentImportService;
	const FBlueprintHelperBlockIdService& BlockIdService;
	const FBlueprintHelperOwnershipService& OwnershipService;
	const FBlueprintHelperTransactionJournalService& JournalService;
};
