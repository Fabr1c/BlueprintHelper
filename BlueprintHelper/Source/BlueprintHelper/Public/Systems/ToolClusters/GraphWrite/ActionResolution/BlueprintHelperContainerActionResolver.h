#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class FBlueprintHelperActionClusterContextView;

class BLUEPRINTHELPER_API FBlueprintHelperContainerActionResolver
{
public:
	static FBlueprintHelperActionResolutionResult Resolve(
		const FBlueprintHelperActionResolutionRequest& Request);

	static FBlueprintHelperActionResolutionResult Resolve(
		const FBlueprintHelperActionResolutionRequest& Request,
		const FBlueprintHelperActionClusterContextView& Context);
};
