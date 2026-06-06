// BlueprintHelper Service Layer - PatchBlueprintGraph service

#pragma once

#include "CoreMinimal.h"
#include "Shared/GraphWrite/BlueprintHelperPatchGraphTypes.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonPathService.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FBlueprintHelperGraphResolver;
class FBlueprintHelperLogicJsonPathService;
class UEdGraph;
class UBlueprint;
class UEdGraphNode;
class UEdGraphPin;
class FJsonObject;

struct FBlueprintHelperResolvedPatchTarget
{
	UEdGraphNode* Node = nullptr;
	UEdGraphPin* Pin = nullptr;
	UEdGraphPin* SourcePin = nullptr;
	UEdGraphPin* ReplacementPin = nullptr;
	FBlueprintHelperResolvedLink Link;
	FBlueprintHelperPatchedRef PatchedRef;
};

class BLUEPRINTHELPER_API FBlueprintHelperPatchBlueprintGraphService
{
public:
	FBlueprintHelperPatchBlueprintGraphService(
		const FBlueprintHelperGraphResolver& InResolver,
		const FBlueprintHelperLogicJsonPathService& InPathService);
	FBlueprintHelperToolResultBase Execute(const TSharedPtr<FJsonObject>& Payload) const;

private:
	struct FPatchRequest
	{
		FString AssetPath;
		FString GraphName;
		EBlueprintHelperPatchScope PatchScope = EBlueprintHelperPatchScope::PinDefault;
		EBlueprintHelperPatchType PatchType = EBlueprintHelperPatchType::SetPinDefault;
		bool bDryRun = false;
		FString BlockId, GroupEntryNodePath;
		FString NodeRef, PinRef, LinkRef;
		FString NodePath, PinPath, LinkPath;
		FString SourceBlockId, SourceNodeRef, SourcePinRef;
		FString SourceNodePath, SourcePinPath;
		FString ReplacementBlockId, ReplacementNodeRef, ReplacementPinRef;
		FString ReplacementNodePath, ReplacementPinPath;
		TSharedPtr<FJsonObject> PatchPayload;
		TSharedPtr<FJsonObject> LogicSpec;
		FString ExpectedOldValue;
		FString ExpectedSourceNodeRef, ExpectedSourcePinRef;
		FString ExpectedTargetNodeRef, ExpectedTargetPinRef;
		FString ExpectedNodeRef, ExpectedNodeClass;
		bool bDeleteBreakLinks = true;
		bool bDeleteAllowEntryNode = false;
		bool bDeleteAllowLifecycleRoot = false;
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
	bool PreflightOwnedPatchContract(const FPatchRequest& Request, FPatchPreflightResult& OutResult) const;
	FBlueprintHelperToolResultBase ExecuteDryRun(const FPatchRequest& Request) const;
	FBlueprintHelperToolResultBase ExecuteWrite(const FPatchRequest& Request) const;

	bool ApplyPatch(
		UBlueprint* Blueprint, UEdGraph* Graph,
		const FPatchRequest& Request, const FBlueprintHelperResolvedPatchTarget& Target,
		bool& bOutChanged, FString& OutError) const;

	// 銆俻atch_type 瀹炵幇
	bool ResolvePatchSourcePin(
		UEdGraph* Graph,
		const FPatchRequest& Request,
		UEdGraphPin*& OutPin,
		FString& OutError,
		FString* OutField = nullptr,
		FString* OutCode = nullptr) const;
	bool ResolvePatchReplacementPin(
		UEdGraph* Graph,
		const FPatchRequest& Request,
		UEdGraphPin*& OutPin,
		FString& OutError,
		FString* OutField = nullptr,
		FString* OutCode = nullptr) const;
	bool ExecuteMutationIntent(UEdGraph* Graph, const struct FBlueprintHelperGraphWriteMutationIntent& Intent, bool& bOutChanged, FString& OutError) const;

	const FBlueprintHelperGraphResolver& Resolver;
	const FBlueprintHelperLogicJsonPathService& PathService;
};
