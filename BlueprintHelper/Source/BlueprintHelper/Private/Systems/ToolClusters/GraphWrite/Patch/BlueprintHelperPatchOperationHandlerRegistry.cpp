#include "Systems/ToolClusters/GraphWrite/Patch/BlueprintHelperPatchOperationHandlerRegistry.h"

#include "Systems/ToolClusters/GraphWrite/Patch/Handlers/BlueprintHelperConnectPinsPatchHandler.h"
#include "Systems/ToolClusters/GraphWrite/Patch/Handlers/BlueprintHelperDeleteOwnedNodePatchHandler.h"
#include "Systems/ToolClusters/GraphWrite/Patch/Handlers/BlueprintHelperDisconnectLinkPatchHandler.h"
#include "Systems/ToolClusters/GraphWrite/Patch/Handlers/BlueprintHelperReplaceLinkPatchHandler.h"
#include "Systems/ToolClusters/GraphWrite/Patch/Handlers/BlueprintHelperSetNodeCommentPatchHandler.h"
#include "Systems/ToolClusters/GraphWrite/Patch/Handlers/BlueprintHelperSetPinDefaultPatchHandler.h"

static TArray<TUniquePtr<IBlueprintHelperPatchOperationHandler>>& BlueprintHelperPatchOperationHandlers()
{
	static TArray<TUniquePtr<IBlueprintHelperPatchOperationHandler>> Handlers;
	if (Handlers.Num() == 0)
	{
		Handlers.Add(CreateBlueprintHelperSetPinDefaultPatchHandler());
		Handlers.Add(CreateBlueprintHelperSetNodeCommentPatchHandler());
		Handlers.Add(CreateBlueprintHelperConnectPinsPatchHandler());
		Handlers.Add(CreateBlueprintHelperDisconnectLinkPatchHandler());
		Handlers.Add(CreateBlueprintHelperReplaceLinkPatchHandler());
		Handlers.Add(CreateBlueprintHelperDeleteOwnedNodePatchHandler());
	}
	return Handlers;
}

TArray<FString> FBlueprintHelperPatchOperationHandlerRegistry::GetRegisteredPatchKinds()
{
	TArray<FString> PatchKinds;
	for (const TUniquePtr<IBlueprintHelperPatchOperationHandler>& Handler : BlueprintHelperPatchOperationHandlers())
	{
		if (Handler.IsValid())
		{
			PatchKinds.Add(Handler->GetPatchKind());
		}
	}
	return PatchKinds;
}

bool FBlueprintHelperPatchOperationHandlerRegistry::IsPatchKindRegistered(const FString& PatchKind)
{
	return FindHandler(PatchKind) != nullptr;
}

const IBlueprintHelperPatchOperationHandler* FBlueprintHelperPatchOperationHandlerRegistry::FindHandler(
	const FString& PatchKind)
{
	for (const TUniquePtr<IBlueprintHelperPatchOperationHandler>& Handler : BlueprintHelperPatchOperationHandlers())
	{
		if (Handler.IsValid() && Handler->GetPatchKind().Equals(PatchKind, ESearchCase::IgnoreCase))
		{
			return Handler.Get();
		}
	}
	return nullptr;
}
