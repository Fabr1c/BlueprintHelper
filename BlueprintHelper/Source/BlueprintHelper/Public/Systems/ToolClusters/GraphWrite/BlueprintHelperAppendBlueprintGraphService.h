// BlueprintHelper Service Layer 。AppendBlueprintGraph 核心服务

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperServiceTypes.h"
#include "Shared/GraphWrite/BlueprintHelperAppendGraphTypes.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FBlueprintHelperGraphResolver;
class FBlueprintHelperBlockIdService;
class FBlueprintHelperOwnershipService;
class UEdGraph;
class UBlueprint;
class FJsonObject;
struct FBlueprintGraphWriteConnectivityValidationInput;

/**
 * AppendBlueprintGraph 核心服务。 * 支持创建。EG_{FeatureName} 图表或向空图表追加独。BlueprintHelper-owned block。 */
class BLUEPRINTHELPER_API FBlueprintHelperAppendBlueprintGraphService
{
public:
	FBlueprintHelperAppendBlueprintGraphService(
		const FBlueprintHelperGraphResolver& InResolver,
		const FBlueprintHelperBlockIdService& InBlockIdService,
		const FBlueprintHelperOwnershipService& InOwnershipService);
	/** 执行 Append 操作，返回统一 ToolResultBase。*/
	FBlueprintHelperToolResultBase Execute(const TSharedPtr<FJsonObject>& Payload) const;

#if WITH_DEV_AUTOMATION_TESTS
	static void SetAutomationOwnershipWriteFailure(bool bFail, const FString& ErrorMessage);
#endif

private:
	// ─── 内部请求/预检结构 ───

	struct FAppendRequest
	{
		FString AssetPath;
		FString GraphName;
		FString FeatureName;
		bool bDryRun = false;
		bool bReuseExistingEntries = false;
		bool bAllowExistingGraph = false;
		bool bIncludeTiming = false;
		TSharedPtr<FJsonObject> LogicSpec;
	};

	struct FAppendPreflightResult
	{
		bool bPassed = true;
		TArray<FString> BlockedBy;
		TArray<FBlueprintHelperDryRunIssue> Conflicts;
		TArray<FBlueprintHelperDryRunIssue> Errors;
		TSharedPtr<FJsonObject> FragmentDebugData;
	};

	// ─── 解析 ───

	FAppendRequest ParseRequest(const TSharedPtr<FJsonObject>& Payload) const;

	// ─── Preflight ───

	FAppendPreflightResult Preflight(const FAppendRequest& Request) const;
	bool PreflightBlueprint(const FString& AssetPath, UBlueprint*& OutBlueprint, FAppendPreflightResult& OutResult) const;
	bool PreflightGraphTarget(UBlueprint* Blueprint, const FAppendRequest& Request, UEdGraph*& OutGraph, FAppendPreflightResult& OutResult) const;
	bool PreflightNodePayload(const FAppendRequest& Request, UBlueprint* Blueprint, UEdGraph* Graph, FAppendPreflightResult& OutResult) const;

	// ─── 图表操作 ───

	UEdGraph* FindOrCreateAppendGraph(UBlueprint* Blueprint, const FString& GraphName, FString& OutError) const;
	bool IsEventGraph(UEdGraph* Graph) const;

	// ─── 执行 ───

	FBlueprintHelperToolResultBase ExecuteDryRun(const FAppendRequest& Request) const;
	FBlueprintHelperToolResultBase ExecuteWrite(const FAppendRequest& Request) const;

	// ─── 构建 AgentImport 兼容 JSON ───

	FString BuildSemanticGraphWritePayload(const FAppendRequest& Request) const;
	FBlueprintGraphWriteConnectivityValidationInput BuildAppendConnectivityInput(
		UBlueprint* Blueprint,
		UEdGraph* TargetGraph,
		const FAppendRequest& Request) const;

	// ─── Helpers ───

	TArray<FString> ExtractCustomEventNames(const FAppendRequest& Request) const;

	const FBlueprintHelperGraphResolver& Resolver;
	const FBlueprintHelperBlockIdService& BlockIdService;
	const FBlueprintHelperOwnershipService& OwnershipService;
};
