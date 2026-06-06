#include "Systems/ToolClusters/GraphWrite/Patch/Handlers/BlueprintHelperDeleteOwnedNodePatchHandler.h"

#include "EdGraph/EdGraphNode.h"
#include "Kismet2/BlueprintEditorUtils.h"

FString FBlueprintHelperDeleteOwnedNodePatchHandler::GetPatchKind() const
{
	return TEXT("delete_owned_node");
}

bool FBlueprintHelperDeleteOwnedNodePatchHandler::ValidateRequest(
	const TSharedRef<FJsonObject>&,
	FBlueprintHelperToolError&) const
{
	return true;
}

bool FBlueprintHelperDeleteOwnedNodePatchHandler::BuildMutationIntent(
	const TSharedRef<FJsonObject>&,
	FBlueprintHelperGraphWriteMutationIntent& OutIntent,
	FBlueprintHelperToolError&) const
{
	OutIntent.Kind = EBlueprintHelperGraphWriteMutationIntentKind::Unknown;
	OutIntent.IntentId = TEXT("patch_delete_owned_node");
	return true;
}

bool FBlueprintHelperDeleteOwnedNodePatchHandler::ApplyResolvedPatch(
	const FBlueprintHelperPatchOperationApplyContext& Context,
	bool& bOutChanged,
	FString& OutError) const
{
	if (!Context.Blueprint || !Context.TargetNode)
	{
		OutError = TEXT("delete_owned_node target node is missing.");
		return false;
	}

	Context.TargetNode->Modify();
	if (Context.bDeleteBreakLinks)
	{
		Context.TargetNode->BreakAllNodeLinks();
	}
	FBlueprintEditorUtils::RemoveNode(Context.Blueprint, Context.TargetNode, true);
	bOutChanged = true;
	return true;
}

TUniquePtr<IBlueprintHelperPatchOperationHandler> CreateBlueprintHelperDeleteOwnedNodePatchHandler()
{
	return MakeUnique<FBlueprintHelperDeleteOwnedNodePatchHandler>();
}
