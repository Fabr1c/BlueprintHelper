#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonPathService.h"

class UEdGraphNode;
class UEdGraphPin;

struct BLUEPRINTHELPER_API FBlueprintHelperOwnedGraphPatchPolicyResult
{
	bool bPassed = true;
	FString Code;
	FString Message;
	FString Field;

	static FBlueprintHelperOwnedGraphPatchPolicyResult Pass();
	static FBlueprintHelperOwnedGraphPatchPolicyResult Fail(
		const FString& InCode,
		const FString& InMessage,
		const FString& InField);
};

class BLUEPRINTHELPER_API FBlueprintHelperOwnedGraphPatchPolicy
{
public:
	static FBlueprintHelperOwnedGraphPatchPolicyResult RequireOwnedNodeInBlock(
		UEdGraphNode* Node,
		const FString& ExpectedBlockId,
		const FString& Field);

	static FBlueprintHelperOwnedGraphPatchPolicyResult RequireOwnedPinInBlock(
		UEdGraphPin* Pin,
		const FString& ExpectedBlockId,
		const FString& Field);

	static FBlueprintHelperOwnedGraphPatchPolicyResult RequireOwnedLinkInBlock(
		const FBlueprintHelperResolvedLink& Link,
		const FString& ExpectedBlockId,
		const FString& Field);

	static FBlueprintHelperOwnedGraphPatchPolicyResult RequireDeleteAllowed(
		UEdGraphNode* Node,
		const FString& ExpectedBlockId,
		bool bBreakLinks,
		bool bAllowEntryNode,
		bool bAllowLifecycleRoot);

private:
	static bool TryReadNodeOwnership(
		UEdGraphNode* Node,
		FString& OutBlockId,
		bool& bOutOwned);

	static bool IsEntryNode(UEdGraphNode* Node);
	static bool IsLifecycleRootNode(UEdGraphNode* Node);
};
