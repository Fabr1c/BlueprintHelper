#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class BLUEPRINTHELPER_API FBlueprintHelperFieldVariableActionResolver
{
public:
	FBlueprintHelperActionResolutionResult Resolve(const FBlueprintHelperActionResolutionRequest& Request) const;

private:
	static bool IsSupportedSemanticKind(EBlueprintHelperActionSemanticKind Kind);
	static bool IsWritableSemanticKind(EBlueprintHelperActionSemanticKind Kind);
};
