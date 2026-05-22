#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class FBlueprintHelperActionClusterContextView;

class BLUEPRINTHELPER_API FBlueprintHelperGenericAssetStructControlActionResolver
{
public:
	static FBlueprintHelperActionResolutionResult ResolveNodeSpawnerCandidate(
		const FBlueprintHelperActionResolutionRequest& Request,
		const FBlueprintHelperActionClusterContextView& Context);

private:
	static FBlueprintHelperActionResolutionResult MakeNeedsContextResult(
		const FBlueprintHelperActionResolutionRequest& Request,
		const FString& Message);
	static FBlueprintHelperActionResolutionResult MakeStructTypeNotFoundResult(
		const FBlueprintHelperActionResolutionRequest& Request,
		const FString& Message);
	static FBlueprintHelperActionResolutionResult MakeUnsupportedIntentResult(
		const FBlueprintHelperActionResolutionRequest& Request,
		const FString& Message);
};
