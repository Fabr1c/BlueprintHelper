#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class BLUEPRINTHELPER_API FBlueprintHelperSpawnerClusterResolver
{
public:
	static EBlueprintHelperSpawnerClusterKind SelectCluster(EBlueprintHelperActionIntent Intent);
	static FBlueprintHelperActionResolutionResult Resolve(const FBlueprintHelperActionResolutionRequest& Request);
};