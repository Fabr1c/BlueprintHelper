#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class BLUEPRINTHELPER_API FBlueprintHelperFieldVariableActionCluster
{
public:
	static FBlueprintHelperActionResolutionResult Resolve(const FBlueprintHelperActionResolutionRequest& Request);

private:
	static bool OwnsSemanticKind(EBlueprintHelperActionSemanticKind Kind);
	static FBlueprintHelperActionResolutionResult MakeUnsupportedIntentResult(const FBlueprintHelperActionResolutionRequest& Request);
	static FBlueprintHelperActionResolutionResult MakeNeedsMoreSemanticContextResult(const FBlueprintHelperActionResolutionRequest& Request);
};
