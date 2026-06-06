#include "Systems/ToolClusters/GraphWrite/Patch/Handlers/BlueprintHelperReplaceLinkPatchHandler.h"

#include "EdGraph/EdGraphPin.h"

FString FBlueprintHelperReplaceLinkPatchHandler::GetPatchKind() const
{
	return TEXT("replace_link");
}

bool FBlueprintHelperReplaceLinkPatchHandler::ValidateRequest(
	const TSharedRef<FJsonObject>&,
	FBlueprintHelperToolError&) const
{
	return true;
}

bool FBlueprintHelperReplaceLinkPatchHandler::BuildMutationIntent(
	const TSharedRef<FJsonObject>&,
	FBlueprintHelperGraphWriteMutationIntent& OutIntent,
	FBlueprintHelperToolError&) const
{
	OutIntent.Kind = EBlueprintHelperGraphWriteMutationIntentKind::ReplacePinConnection;
	OutIntent.IntentId = TEXT("patch_replace_link");
	return true;
}

bool FBlueprintHelperReplaceLinkPatchHandler::ApplyResolvedPatch(
	const FBlueprintHelperPatchOperationApplyContext& Context,
	bool& bOutChanged,
	FString& OutError) const
{
	if (!Context.Link.SourcePin)
	{
		OutError = TEXT("replace_link requires an existing link.");
		return false;
	}

	if (!Context.ReplacementPin)
	{
		OutError = TEXT("replace_link requires a new target pin.");
		return false;
	}

	if (!Context.ExecuteMutationIntent)
	{
		OutError = TEXT("replace_link requires mutation executor.");
		return false;
	}

	FBlueprintHelperToolError ToolError;
	FBlueprintHelperGraphWriteMutationIntent Intent;
	BuildMutationIntent(Context.PatchJson.ToSharedRef(), Intent, ToolError);
	Intent.Source.Pin = Context.Link.SourcePin;
	Intent.Target.Pin = Context.Link.TargetPin;
	Intent.ReplacementTarget.Pin = Context.ReplacementPin;
	return Context.ExecuteMutationIntent(Intent, bOutChanged, OutError);
}

TUniquePtr<IBlueprintHelperPatchOperationHandler> CreateBlueprintHelperReplaceLinkPatchHandler()
{
	return MakeUnique<FBlueprintHelperReplaceLinkPatchHandler>();
}
