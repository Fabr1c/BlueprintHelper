#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/Patch/BlueprintHelperPatchOperationHandler.h"

class FBlueprintHelperConnectPinsPatchHandler final : public IBlueprintHelperPatchOperationHandler
{
public:
	virtual FString GetPatchKind() const override;
	virtual bool ValidateRequest(
		const TSharedRef<FJsonObject>& PatchJson,
		FBlueprintHelperToolError& OutError) const override;
	virtual bool BuildMutationIntent(
		const TSharedRef<FJsonObject>& PatchJson,
		FBlueprintHelperGraphWriteMutationIntent& OutIntent,
		FBlueprintHelperToolError& OutError) const override;
	virtual bool ApplyResolvedPatch(
		const FBlueprintHelperPatchOperationApplyContext& Context,
		bool& bOutChanged,
		FString& OutError) const override;
};

TUniquePtr<IBlueprintHelperPatchOperationHandler> CreateBlueprintHelperConnectPinsPatchHandler();
