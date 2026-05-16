// BlueprintHelper GraphStatement BlueprintHelperGraphPatternRegistryUtils implementation.

#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphPatternRegistryUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphPatternRegistry.h"
#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FString FBlueprintHelperGraphPatternRegistryUtils::NormalizePatternKey(const FString& PatternName)
{
	return PatternName.TrimStartAndEnd().ToLower();
}
FString FBlueprintHelperGraphPatternRegistryUtils::NormalizeLookupKey(const FString& Name)
{
	return Name.TrimStartAndEnd().ToLower();
}
FString FBlueprintHelperGraphPatternRegistryUtils::JsonValueToBindingString(const TSharedPtr<FJsonValue>& Value)
{
	if (!Value.IsValid())
	{
		return FString();
	}

	switch (Value->Type)
	{
	case EJson::String:
		return Value->AsString();
	case EJson::Number:
		return FString::SanitizeFloat(Value->AsNumber());
	case EJson::Boolean:
		return Value->AsBool() ? TEXT("true") : TEXT("false");
	default:
		return FString();
	}
}
void FBlueprintHelperGraphPatternRegistryUtils::ReadStringMapField(
	const TSharedPtr<FJsonObject>& Object,
	const FString& FieldName,
	TMap<FString, FString>& OutMap,
	bool bNormalizeKeys)
{
	const TSharedPtr<FJsonObject>* MapObject = nullptr;
	if (!Object.IsValid() || !Object->TryGetObjectField(FieldName, MapObject) || !MapObject || !MapObject->IsValid())
	{
		return;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*MapObject)->Values)
	{
		const FString ValueString = JsonValueToBindingString(Pair.Value);
		if (ValueString.IsEmpty())
		{
			continue;
		}

		OutMap.Add(bNormalizeKeys ? NormalizeLookupKey(Pair.Key) : Pair.Key, ValueString);
	}
}
