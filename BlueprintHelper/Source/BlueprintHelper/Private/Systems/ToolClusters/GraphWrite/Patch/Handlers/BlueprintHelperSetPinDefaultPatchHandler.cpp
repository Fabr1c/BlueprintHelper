#include "Systems/ToolClusters/GraphWrite/Patch/Handlers/BlueprintHelperSetPinDefaultPatchHandler.h"

#include "EdGraph/EdGraphPin.h"

FString FBlueprintHelperSetPinDefaultPatchHandler::GetPatchKind() const
{
	return TEXT("set_pin_default");
}

bool FBlueprintHelperSetPinDefaultPatchHandler::ValidateRequest(
	const TSharedRef<FJsonObject>&,
	FBlueprintHelperToolError&) const
{
	return true;
}

bool FBlueprintHelperSetPinDefaultPatchHandler::BuildMutationIntent(
	const TSharedRef<FJsonObject>& PatchJson,
	FBlueprintHelperGraphWriteMutationIntent& OutIntent,
	FBlueprintHelperToolError&) const
{
	FString NewValue;
	PatchJson->TryGetStringField(TEXT("value"), NewValue);
	OutIntent.Kind = EBlueprintHelperGraphWriteMutationIntentKind::SetPinDefault;
	OutIntent.IntentId = TEXT("patch_set_pin_default");
	OutIntent.DefaultValue = NewValue;
	return true;
}

bool FBlueprintHelperSetPinDefaultPatchHandler::ApplyResolvedPatch(
	const FBlueprintHelperPatchOperationApplyContext& Context,
	bool& bOutChanged,
	FString& OutError) const
{
	if (!Context.TargetPin)
	{
		OutError = TEXT("set_pin_default requires target pin_ref.");
		return false;
	}

	if (!Context.ExecuteMutationIntent)
	{
		OutError = TEXT("set_pin_default requires mutation executor.");
		return false;
	}

	FBlueprintHelperToolError ToolError;
	FBlueprintHelperGraphWriteMutationIntent Intent;
	if (!BuildMutationIntent(Context.PatchJson.ToSharedRef(), Intent, ToolError))
	{
		OutError = ToolError.Message;
		return false;
	}

	Intent.Target.Pin = Context.TargetPin;
	return Context.ExecuteMutationIntent(Intent, bOutChanged, OutError);
}

TUniquePtr<IBlueprintHelperPatchOperationHandler> CreateBlueprintHelperSetPinDefaultPatchHandler()
{
	return MakeUnique<FBlueprintHelperSetPinDefaultPatchHandler>();
}
