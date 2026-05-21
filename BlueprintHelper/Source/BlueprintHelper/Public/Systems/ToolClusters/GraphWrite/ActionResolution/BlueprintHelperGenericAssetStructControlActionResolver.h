#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class BLUEPRINTHELPER_API FBlueprintHelperGenericAssetStructControlActionResolver
{
public:
	static FBlueprintHelperActionResolutionResult ResolveNodeSpawnerCandidate(const FBlueprintHelperActionResolutionRequest& Request);

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
