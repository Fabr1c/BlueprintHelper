#include "Systems/ToolClusters/GraphWrite/Patch/Handlers/BlueprintHelperSetNodeCommentPatchHandler.h"

#include "EdGraph/EdGraphNode.h"

FString FBlueprintHelperSetNodeCommentPatchHandler::GetPatchKind() const
{
	return TEXT("set_node_comment");
}

bool FBlueprintHelperSetNodeCommentPatchHandler::ValidateRequest(
	const TSharedRef<FJsonObject>&,
	FBlueprintHelperToolError&) const
{
	return true;
}

bool FBlueprintHelperSetNodeCommentPatchHandler::BuildMutationIntent(
	const TSharedRef<FJsonObject>& PatchJson,
	FBlueprintHelperGraphWriteMutationIntent& OutIntent,
	FBlueprintHelperToolError&) const
{
	FString Comment;
	PatchJson->TryGetStringField(TEXT("comment"), Comment);
	OutIntent.Kind = EBlueprintHelperGraphWriteMutationIntentKind::Unknown;
	OutIntent.IntentId = TEXT("patch_set_node_comment");
	OutIntent.DefaultValue = Comment;
	return true;
}

bool FBlueprintHelperSetNodeCommentPatchHandler::ApplyResolvedPatch(
	const FBlueprintHelperPatchOperationApplyContext& Context,
	bool& bOutChanged,
	FString& OutError) const
{
	if (!Context.TargetNode)
	{
		OutError = TEXT("target_node_not_found");
		return false;
	}

	FString Comment;
	Context.PatchJson->TryGetStringField(TEXT("comment"), Comment);
	if (Context.TargetNode->NodeComment == Comment)
	{
		bOutChanged = false;
		return true;
	}

	Context.TargetNode->Modify();
	Context.TargetNode->NodeComment = Comment;
	bOutChanged = true;
	return true;
}

TUniquePtr<IBlueprintHelperPatchOperationHandler> CreateBlueprintHelperSetNodeCommentPatchHandler()
{
	return MakeUnique<FBlueprintHelperSetNodeCommentPatchHandler>();
}
