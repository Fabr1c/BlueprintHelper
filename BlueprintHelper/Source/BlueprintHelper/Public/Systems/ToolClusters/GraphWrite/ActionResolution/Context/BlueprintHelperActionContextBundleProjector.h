#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h"

class UBlueprint;
class UEdGraph;

class BLUEPRINTHELPER_API FBlueprintHelperActionContextBundleProjector
{
public:
	static bool TryBuildRequest(
		const FBlueprintHelperResolvedActionContextBundle& Bundle,
		const FString& StatementId,
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		FBlueprintHelperActionResolutionRequest& OutRequest,
		FString& OutError);
};
