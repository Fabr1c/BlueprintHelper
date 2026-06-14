// BlueprintHelper MaterialGraph connection validator.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Systems/ToolClusters/MaterialGraph/BlueprintHelperMaterialGraphTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperMaterialGraphConnectionValidator
{
public:
	static FBlueprintHelperMaterialGraphValidationResult ValidateLink(
		const TSharedPtr<FJsonObject>& LinkObject,
		const FString& FieldPath);

	static bool IsSupportedMaterialOutputProperty(const FString& PinName);
};
