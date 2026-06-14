// BlueprintHelper MaterialGraph facade.

#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/MaterialGraph/BlueprintHelperMaterialGraphTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperMaterialGraphFacade
{
public:
	FBlueprintHelperMaterialGraphPreflightResult Preflight(
		const FBlueprintHelperMaterialGraphPreflightInput& Input) const;

	FBlueprintHelperToolResultBase Execute(
		const FBlueprintHelperMaterialGraphExecutionInput& Input) const;
};
