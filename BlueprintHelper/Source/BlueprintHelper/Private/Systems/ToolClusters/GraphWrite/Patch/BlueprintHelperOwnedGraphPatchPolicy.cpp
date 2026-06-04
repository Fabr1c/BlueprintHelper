#include "Systems/ToolClusters/GraphWrite/Patch/BlueprintHelperOwnedGraphPatchPolicy.h"

#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

FBlueprintHelperOwnedGraphPatchPolicyResult FBlueprintHelperOwnedGraphPatchPolicyResult::Pass()
{
	FBlueprintHelperOwnedGraphPatchPolicyResult Result;
	Result.bPassed = true;
	return Result;
}

FBlueprintHelperOwnedGraphPatchPolicyResult FBlueprintHelperOwnedGraphPatchPolicyResult::Fail(
	const FString& InCode,
	const FString& InMessage,
	const FString& InField)
{
	FBlueprintHelperOwnedGraphPatchPolicyResult Result;
	Result.bPassed = false;
	Result.Code = InCode;
	Result.Message = InMessage;
	Result.Field = InField;
	return Result;
}

bool FBlueprintHelperOwnedGraphPatchPolicy::TryReadNodeOwnership(
	UEdGraphNode* Node,
	FString& OutBlockId,
	bool& bOutOwned)
{
	OutBlockId.Reset();
	bOutOwned = false;
	if (!Node)
	{
		return false;
	}

	UPackage* Package = Node->GetOutermost();
	if (!Package)
	{
		return false;
	}

	FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
	OutBlockId = MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId"));
	bOutOwned = MetaData.GetValue(Node, TEXT("BlueprintHelperOwned")).Equals(TEXT("true"), ESearchCase::IgnoreCase);
	return bOutOwned && !OutBlockId.IsEmpty();
}

bool FBlueprintHelperOwnedGraphPatchPolicy::IsEntryNode(UEdGraphNode* Node)
{
	return Node && (
		Node->IsA<UK2Node_Event>() ||
		Node->IsA<UK2Node_CustomEvent>() ||
		Node->IsA<UK2Node_FunctionEntry>());
}

bool FBlueprintHelperOwnedGraphPatchPolicy::IsLifecycleRootNode(UEdGraphNode* Node)
{
	if (!Node)
	{
		return false;
	}

	UPackage* Package = Node->GetOutermost();
	if (!Package)
	{
		return false;
	}

	FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
	return MetaData.GetValue(Node, TEXT("BlueprintHelperLifecycleRoot")).Equals(TEXT("true"), ESearchCase::IgnoreCase) ||
		MetaData.GetValue(Node, TEXT("BlueprintHelperGraphLifecycleRoot")).Equals(TEXT("true"), ESearchCase::IgnoreCase) ||
		MetaData.GetValue(Node, TEXT("BlueprintHelperBlockRoot")).Equals(TEXT("true"), ESearchCase::IgnoreCase);
}

FBlueprintHelperOwnedGraphPatchPolicyResult FBlueprintHelperOwnedGraphPatchPolicy::RequireOwnedNodeInBlock(
	UEdGraphNode* Node,
	const FString& ExpectedBlockId,
	const FString& Field)
{
	if (!Node)
	{
		return FBlueprintHelperOwnedGraphPatchPolicyResult::Fail(
			TEXT("owned_patch_endpoint_not_owned"),
			TEXT("Owned GraphWrite patch endpoint node is null."),
			Field);
	}

	FString ActualBlockId;
	bool bOwned = false;
	if (!TryReadNodeOwnership(Node, ActualBlockId, bOwned) ||
		!bOwned ||
		ActualBlockId.IsEmpty())
	{
		return FBlueprintHelperOwnedGraphPatchPolicyResult::Fail(
			TEXT("owned_patch_endpoint_not_owned"),
			FString::Printf(TEXT("Node '%s' is not BlueprintHelper-owned."), *Node->GetName()),
			Field);
	}

	if (!ActualBlockId.Equals(ExpectedBlockId, ESearchCase::IgnoreCase))
	{
		return FBlueprintHelperOwnedGraphPatchPolicyResult::Fail(
			TEXT("owned_patch_cross_block_disallowed"),
			FString::Printf(
				TEXT("Node '%s' belongs to block '%s', not requested block '%s'."),
				*Node->GetName(),
				*ActualBlockId,
				*ExpectedBlockId),
			Field);
	}

	return FBlueprintHelperOwnedGraphPatchPolicyResult::Pass();
}

FBlueprintHelperOwnedGraphPatchPolicyResult FBlueprintHelperOwnedGraphPatchPolicy::RequireOwnedPinInBlock(
	UEdGraphPin* Pin,
	const FString& ExpectedBlockId,
	const FString& Field)
{
	UEdGraphNode* Node = Pin ? Pin->GetOwningNode() : nullptr;
	return RequireOwnedNodeInBlock(Node, ExpectedBlockId, Field);
}

FBlueprintHelperOwnedGraphPatchPolicyResult FBlueprintHelperOwnedGraphPatchPolicy::RequireOwnedLinkInBlock(
	const FBlueprintHelperResolvedLink& Link,
	const FString& ExpectedBlockId,
	const FString& Field)
{
	FBlueprintHelperOwnedGraphPatchPolicyResult SourceResult =
		RequireOwnedPinInBlock(Link.SourcePin, ExpectedBlockId, Field);
	if (!SourceResult.bPassed)
	{
		return SourceResult;
	}

	FBlueprintHelperOwnedGraphPatchPolicyResult TargetResult =
		RequireOwnedPinInBlock(Link.TargetPin, ExpectedBlockId, Field);
	if (!TargetResult.bPassed)
	{
		return TargetResult;
	}

	return FBlueprintHelperOwnedGraphPatchPolicyResult::Pass();
}

FBlueprintHelperOwnedGraphPatchPolicyResult FBlueprintHelperOwnedGraphPatchPolicy::RequireDeleteAllowed(
	UEdGraphNode* Node,
	const FString& ExpectedBlockId,
	bool bBreakLinks,
	bool bAllowEntryNode,
	bool bAllowLifecycleRoot)
{
	if (!Node)
	{
		return FBlueprintHelperOwnedGraphPatchPolicyResult::Fail(
			TEXT("owned_delete_node_not_found"),
			TEXT("delete_owned_node target node was not found."),
			TEXT("patched_ref.node_ref"));
	}

	if (!bBreakLinks || bAllowEntryNode || bAllowLifecycleRoot)
	{
		return FBlueprintHelperOwnedGraphPatchPolicyResult::Fail(
			TEXT("owned_delete_policy_disallowed"),
			TEXT("delete_owned_node requires break_links=true, allow_entry_node=false, and allow_lifecycle_root=false."),
			TEXT("patch"));
	}

	FBlueprintHelperOwnedGraphPatchPolicyResult OwnedResult =
		RequireOwnedNodeInBlock(Node, ExpectedBlockId, TEXT("patched_ref.node_ref"));
	if (!OwnedResult.bPassed)
	{
		return OwnedResult;
	}

	if (!bAllowEntryNode && IsEntryNode(Node))
	{
		return FBlueprintHelperOwnedGraphPatchPolicyResult::Fail(
			TEXT("owned_delete_entry_node_disallowed"),
			FString::Printf(TEXT("delete_owned_node cannot delete entry node '%s'."), *Node->GetName()),
			TEXT("patched_ref.node_ref"));
	}

	if (!bAllowLifecycleRoot && IsLifecycleRootNode(Node))
	{
		return FBlueprintHelperOwnedGraphPatchPolicyResult::Fail(
			TEXT("owned_delete_lifecycle_root_disallowed"),
			FString::Printf(TEXT("delete_owned_node cannot delete lifecycle root node '%s'."), *Node->GetName()),
			TEXT("patched_ref.node_ref"));
	}

	return FBlueprintHelperOwnedGraphPatchPolicyResult::Pass();
}
