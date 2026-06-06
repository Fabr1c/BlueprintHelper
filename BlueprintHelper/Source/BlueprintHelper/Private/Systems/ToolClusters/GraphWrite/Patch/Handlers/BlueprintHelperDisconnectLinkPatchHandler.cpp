#include "Systems/ToolClusters/GraphWrite/Patch/Handlers/BlueprintHelperDisconnectLinkPatchHandler.h"

#include "EdGraph/EdGraphPin.h"

FString FBlueprintHelperDisconnectLinkPatchHandler::GetPatchKind() const
{
	return TEXT("disconnect_link");
}

bool FBlueprintHelperDisconnectLinkPatchHandler::ValidateRequest(
	const TSharedRef<FJsonObject>&,
	FBlueprintHelperToolError&) const
{
	return true;
}

bool FBlueprintHelperDisconnectLinkPatchHandler::BuildMutationIntent(
	const TSharedRef<FJsonObject>&,
	FBlueprintHelperGraphWriteMutationIntent& OutIntent,
	FBlueprintHelperToolError&) const
{
	OutIntent.Kind = EBlueprintHelperGraphWriteMutationIntentKind::DisconnectPins;
	OutIntent.IntentId = TEXT("patch_disconnect_link");
	return true;
}

bool FBlueprintHelperDisconnectLinkPatchHandler::ApplyResolvedPatch(
	const FBlueprintHelperPatchOperationApplyContext& Context,
	bool& bOutChanged,
	FString& OutError) const
{
	if (!Context.Link.SourcePin || !Context.Link.TargetPin)
	{
		OutError = TEXT("disconnect_link requires source and target pins.");
		return false;
	}

	if (!Context.ExecuteMutationIntent)
	{
		OutError = TEXT("disconnect_link requires mutation executor.");
		return false;
	}

	FBlueprintHelperToolError ToolError;
	FBlueprintHelperGraphWriteMutationIntent Intent;
	BuildMutationIntent(Context.PatchJson.ToSharedRef(), Intent, ToolError);
	Intent.Source.Pin = Context.Link.SourcePin;
	Intent.Target.Pin = Context.Link.TargetPin;
	return Context.ExecuteMutationIntent(Intent, bOutChanged, OutError);
}

TUniquePtr<IBlueprintHelperPatchOperationHandler> CreateBlueprintHelperDisconnectLinkPatchHandler()
{
	return MakeUnique<FBlueprintHelperDisconnectLinkPatchHandler>();
}
