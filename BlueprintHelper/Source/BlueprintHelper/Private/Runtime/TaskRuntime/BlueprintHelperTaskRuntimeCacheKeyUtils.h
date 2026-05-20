// BlueprintHelper TaskRuntime cache key utilities.

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FJsonObject;
class FJsonValue;

class FBlueprintHelperTaskRuntimeCacheKeyUtils
{
public:
	static FString HashStableJson(const TSharedPtr<FJsonObject>& Object);
	static FString HashStableJsonValue(const TSharedPtr<FJsonValue>& Value);
	static FString HashString(const FString& Value);

	static int64 EstimateJsonBytes(const TSharedPtr<FJsonObject>& Object);
	static int64 EstimateJsonValueBytes(const TSharedPtr<FJsonValue>& Value);
	static int64 EstimateToolResultBytes(const FBlueprintHelperToolResultBase& Result);

	static TSharedPtr<FJsonObject> CloneJsonObject(const TSharedPtr<FJsonObject>& Source);
	static TSharedPtr<FJsonValue> CloneJsonValue(const TSharedPtr<FJsonValue>& Source);
	static FBlueprintHelperToolResultBase CloneToolResult(const FBlueprintHelperToolResultBase& Source);

	static FString StableSerializeJsonObject(const TSharedPtr<FJsonObject>& Object);
	static FString StableSerializeJsonValue(const TSharedPtr<FJsonValue>& Value);
};
