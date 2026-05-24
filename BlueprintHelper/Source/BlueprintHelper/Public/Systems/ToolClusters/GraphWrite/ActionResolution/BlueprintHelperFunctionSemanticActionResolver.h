#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class FBlueprintHelperActionClusterContextView;

class BLUEPRINTHELPER_API FBlueprintHelperFunctionSemanticActionResolver
{
public:
	static bool IsSupportedSemanticKind(const FBlueprintHelperActionSemanticConstraints& Semantic);

	static FBlueprintHelperActionResolutionResult Resolve(
		const FBlueprintHelperActionResolutionRequest& Request,
		const FBlueprintHelperActionClusterContextView& Context);
};
