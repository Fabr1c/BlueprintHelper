// BlueprintHelper GraphStatement FBlueprintHelperGraphPatternRegistryUtils declarations.

#pragma once

#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphPatternRegistry.h"
#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

class FBlueprintHelperGraphPatternRegistryUtils
{
public:
	static FString NormalizePatternKey(const FString& PatternName);
	static FString NormalizeLookupKey(const FString& Name);
	static FString JsonValueToBindingString(const TSharedPtr<FJsonValue>& Value);
	static void ReadStringMapField(
			const TSharedPtr<FJsonObject>& Object,
			const FString& FieldName,
			TMap<FString, FString>& OutMap,
			bool bNormalizeKeys);
};
