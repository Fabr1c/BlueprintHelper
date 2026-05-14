// BlueprintHelper Service Layer 。PatchBlueprintGraph 核心服务

#pragma once

#include "CoreMinimal.h"
#include "Shared/GraphWrite/BlueprintHelperPatchGraphTypes.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonPathService.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FBlueprintHelperGraphResolver;
class FBlueprintHelperLogicJsonPathService;
class FBlueprintHelperTransactionJournalService;
class UEdGraph;
class UBlueprint;
class UEdGraphNode;
class UEdGraphPin;
class FJsonObject;

struct FBlueprintHelperResolvedPatchTarget
{
	UEdGraphNode* Node = nullptr;
	UEdGraphPin* Pin = nullptr;
	FBlueprintHelperResolvedLink Link;
	FBlueprintHelperPatchedRef PatchedRef;
};

class BLUEPRINTHELPER_API FBlueprintHelperPatchBlueprintGraphService
{
public:
	FBlueprintHelperPatchBlueprintGraphService(
		const FBlueprintHelperGraphResolver& InResolver,
		const FBlueprintHelperLogicJsonPathService& InPathService,
		const FBlueprintHelperTransactionJournalService& InJournalService);

	FBlueprintHelperToolResultBase Execute(const TSharedPtr<FJsonObject>& Payload) const;

private:
	struct FPatchRequest
	{
		FString AssetPath;
		FString GraphName;
		EBlueprintHelperPatchScope PatchScope;
		EBlueprintHelperPatchType PatchType;
		bool bDryRun = false;
		FString BlockId, GroupEntryNodePath;
		FString NodeRef, PinRef, LinkRef;
		FString NodePath, PinPath, LinkPath;
		TSharedPtr<FJsonObject> PatchPayload;
		TSharedPtr<FJsonObject> LogicSpec;
		FString ExpectedOldValue;
		bool bExpectedOldStateProvided = false;
	};

	struct FPatchPreflightResult
	{
		bool bPassed = true;
		TArray<FString> BlockedBy;
		TArray<FBlueprintHelperGraphWriteIssue> Conflicts;
		TArray<FBlueprintHelperGraphWriteIssue> Errors;
		FString BeforeValue;
		TSharedPtr<FJsonObject> FragmentDebugData;
	};

	FPatchRequest ParseRequest(const TSharedPtr<FJsonObject>& Payload) const;
	FPatchPreflightResult Preflight(const FPatchRequest& Request, UBlueprint* Blueprint, UEdGraph* Graph, FBlueprintHelperResolvedPatchTarget& OutTarget) const;
	bool PreflightLogicSpec(const FPatchRequest& Request, UBlueprint* Blueprint, FPatchPreflightResult& OutResult) const;
	FBlueprintHelperToolResultBase ExecuteDryRun(const FPatchRequest& Request) const;
	FBlueprintHelperToolResultBase ExecuteWrite(const FPatchRequest& Request) const;

	bool ApplyPatch(
		UBlueprint* Blueprint, UEdGraph* Graph,
		const FPatchRequest& Request, const FBlueprintHelperResolvedPatchTarget& Target,
		bool& bOutChanged, FString& OutError) const;

	// 。patch_type 实现
	bool ApplySetPinDefault(UEdGraph* Graph, UEdGraphPin* Pin, const FString& NewValue, bool& bOutChanged, FString& OutError) const;
	bool ApplySetNodeComment(UEdGraphNode* Node, const FString& NewComment, bool& bOutChanged, FString& OutError) const;
	bool ApplySetNodePosition(UEdGraphNode* Node, const TSharedPtr<FJsonObject>& Payload, bool& bOutChanged, FString& OutError) const;
	bool ApplyConnectPins(UEdGraph* Graph, UEdGraphPin* FromPin, UEdGraphPin* ToPin, bool& bOutChanged, FString& OutError) const;
	bool ApplyDisconnectLink(UEdGraphPin* FromPin, UEdGraphPin* ToPin, bool& bOutChanged, FString& OutError) const;
	bool ApplyReplaceLink(UEdGraph* Graph, const FBlueprintHelperResolvedLink& OldLink, UEdGraphPin* NewToPin, bool& bOutChanged, FString& OutError) const;

	const FBlueprintHelperGraphResolver& Resolver;
	const FBlueprintHelperLogicJsonPathService& PathService;
	const FBlueprintHelperTransactionJournalService& JournalService;
};
