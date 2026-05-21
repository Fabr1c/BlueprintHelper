#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class BLUEPRINTHELPER_API FBlueprintHelperFunctionActionCluster
{
public:
	static FBlueprintHelperActionResolutionResult Resolve(const FBlueprintHelperActionResolutionRequest& Request);

private:
	static FBlueprintHelperActionResolutionResult MakeUnsupportedIntentResult(const FBlueprintHelperActionResolutionRequest& Request);
};
