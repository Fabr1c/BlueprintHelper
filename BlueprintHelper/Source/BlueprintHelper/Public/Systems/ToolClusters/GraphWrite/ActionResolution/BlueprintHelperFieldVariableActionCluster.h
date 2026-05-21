#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class BLUEPRINTHELPER_API FBlueprintHelperFieldVariableActionCluster
{
public:
	static FBlueprintHelperActionResolutionResult Resolve(const FBlueprintHelperActionResolutionRequest& Request);

private:
	static bool OwnsIntent(EBlueprintHelperActionIntent Intent);
	static FBlueprintHelperActionResolutionResult MakeUnsupportedIntentResult(const FBlueprintHelperActionResolutionRequest& Request);
	static FBlueprintHelperActionResolutionResult MakeUnsupportedClusterMigrationResult(const FBlueprintHelperActionResolutionRequest& Request);
};