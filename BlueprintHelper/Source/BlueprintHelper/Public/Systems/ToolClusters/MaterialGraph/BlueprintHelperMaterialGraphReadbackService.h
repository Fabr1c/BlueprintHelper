// BlueprintHelper MaterialGraph execution readback service.

#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/MaterialGraph/BlueprintHelperMaterialGraphTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperMaterialGraphReadbackService
{
public:
	static void ValidatePlannedExpressionConsumption(FBlueprintHelperMaterialGraphExecutionState& State);
	static void ValidateExecutedConnections(FBlueprintHelperMaterialGraphExecutionState& State);
};
