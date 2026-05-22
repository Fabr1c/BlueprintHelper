#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class FBlueprintHelperActionClusterContextView;

class BLUEPRINTHELPER_API FBlueprintHelperGenericAssetStructControlActionCluster
{
public:
	static FBlueprintHelperActionResolutionResult Resolve(
		const FBlueprintHelperActionResolutionRequest& Request,
		const FBlueprintHelperActionClusterContextView& Context);

private:
	static bool OwnsSemanticKind(EBlueprintHelperActionSemanticKind Kind);
	static FBlueprintHelperActionResolutionResult MakeUnsupportedIntentResult(const FBlueprintHelperActionResolutionRequest& Request);
};
