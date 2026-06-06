#include "Systems/ToolClusters/GraphWrite/Patch/Handlers/BlueprintHelperConnectPinsPatchHandler.h"

#include "EdGraph/EdGraphPin.h"

FString FBlueprintHelperConnectPinsPatchHandler::GetPatchKind() const
{
	return TEXT("connect_pins");
}

bool FBlueprintHelperConnectPinsPatchHandler::ValidateRequest(
	const TSharedRef<FJsonObject>&,
	FBlueprintHelperToolError&) const
{
	return true;
}

bool FBlueprintHelperConnectPinsPatchHandler::BuildMutationIntent(
	const TSharedRef<FJsonObject>&,
	FBlueprintHelperGraphWriteMutationIntent& OutIntent,
	FBlueprintHelperToolError&) const
{
	OutIntent.Kind = EBlueprintHelperGraphWriteMutationIntentKind::ConnectPins;
	OutIntent.IntentId = TEXT("patch_connect_pins");
	return true;
}

bool FBlueprintHelperConnectPinsPatchHandler::ApplyResolvedPatch(
	const FBlueprintHelperPatchOperationApplyContext& Context,
	bool& bOutChanged,
	FString& OutError) const
{
	if (!Context.TargetPin)
	{
		OutError = TEXT("connect_pins requires target pin_ref.");
		return false;
	}

	if (!Context.SourcePin)
	{
		OutError = TEXT("connect_pins requires resolved source pin.");
		return false;
	}

	if (!Context.ExecuteMutationIntent)
	{
		OutError = TEXT("connect_pins requires mutation executor.");
		return false;
	}

	FBlueprintHelperToolError ToolError;
	FBlueprintHelperGraphWriteMutationIntent Intent;
	BuildMutationIntent(Context.PatchJson.ToSharedRef(), Intent, ToolError);
	Intent.Source.Pin = Context.SourcePin;
	Intent.Target.Pin = Context.TargetPin;
	return Context.ExecuteMutationIntent(Intent, bOutChanged, OutError);
}

TUniquePtr<IBlueprintHelperPatchOperationHandler> CreateBlueprintHelperConnectPinsPatchHandler()
{
	return MakeUnique<FBlueprintHelperConnectPinsPatchHandler>();
}
