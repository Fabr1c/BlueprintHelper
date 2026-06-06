#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/Patch/BlueprintHelperPatchOperationHandler.h"

class BLUEPRINTHELPER_API FBlueprintHelperPatchOperationHandlerRegistry
{
public:
	static TArray<FString> GetRegisteredPatchKinds();
	static bool IsPatchKindRegistered(const FString& PatchKind);
	static const IBlueprintHelperPatchOperationHandler* FindHandler(const FString& PatchKind);
};
