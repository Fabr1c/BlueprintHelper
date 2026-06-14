// BlueprintHelper MaterialGraph selector resolver.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"
#include "Systems/ToolClusters/MaterialGraph/BlueprintHelperMaterialGraphTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperMaterialExpressionSelectorResolver
{
public:
	static FBlueprintHelperMaterialSelectorResolution ResolveSelector(
		const TSharedPtr<FJsonValue>& SelectorValue);
	static FBlueprintHelperMaterialSelectorResolution ResolveSelector(
		const TSharedPtr<FJsonValue>& SelectorValue,
		const FString& AssetPath);

	static bool IsCommonSelector(const FString& Selector);
	static FString ResolveCommonSelectorClassName(const FString& Selector);
};
