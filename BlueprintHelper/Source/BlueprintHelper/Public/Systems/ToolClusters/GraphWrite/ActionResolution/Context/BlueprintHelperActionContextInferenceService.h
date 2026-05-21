#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperActionContextInferenceService
{
public:
	static FBlueprintHelperResolvedActionContextBundle Infer(
		const FBlueprintHelperActionContextSnapshot& Snapshot,
		const TArray<FBlueprintHelperActionContextDemand>& Demands);

private:
	static FBlueprintHelperResolvedActionContext BuildContext(
		const FBlueprintHelperActionContextSnapshot& Snapshot,
		const FBlueprintHelperActionContextDemand& Demand);
};
