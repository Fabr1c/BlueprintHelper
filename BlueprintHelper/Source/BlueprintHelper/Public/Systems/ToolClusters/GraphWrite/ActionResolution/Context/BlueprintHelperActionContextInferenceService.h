#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperActionContextInferenceService
{
public:
	static FBlueprintHelperResolvedActionContextBundle Infer(
		const FBlueprintHelperActionContextSnapshot& Snapshot,
		const TArray<FBlueprintHelperActionContextDemand>& Demands);

#if WITH_DEV_AUTOMATION_TESTS
	static FBlueprintHelperResolvedActionContext BuildContextForTest(
		const FBlueprintHelperActionContextSnapshot& Snapshot,
		const FBlueprintHelperActionContextDemand& Demand);
#endif

private:
	static FBlueprintHelperResolvedActionContext BuildContext(
		const FBlueprintHelperActionContextSnapshot& Snapshot,
		const FBlueprintHelperActionContextDemand& Demand);
};
