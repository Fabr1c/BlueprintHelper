// BlueprintHelper TaskRuntime request-level CallFunction resolution cache.

#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"

struct FBlueprintHelperTaskRuntimeCallFunctionResolutionCacheStats
{
	int32 Hits = 0;
	int32 Misses = 0;
	int32 Entries = 0;
};

struct FBlueprintHelperTaskRuntimeCachedCallFunctionResolution
{
	bool bResolved = false;
	FString ErrorCode;
	FString Message;
	FString StableId;
	FString NativeName;
	FString DisplayName;
	FString OwnerClassPath;
	TArray<FBlueprintHelperCallFunctionCandidateInfo> CandidateFunctions;
};

class FBlueprintHelperTaskRuntimeCallFunctionResolutionCache
{
public:
	bool TryGet(const FString& Key, FBlueprintHelperTaskRuntimeCachedCallFunctionResolution& OutValue);
	void Store(const FString& Key, const FBlueprintHelperTaskRuntimeCachedCallFunctionResolution& Value);
	FBlueprintHelperTaskRuntimeCallFunctionResolutionCacheStats GetStats() const;

	static FString MakeKey(
		const FBlueprintHelperCallFunctionResolveRequest& Request,
		const FString& AssetPath,
		const FString& GraphName);

private:
	TMap<FString, FBlueprintHelperTaskRuntimeCachedCallFunctionResolution> ValuesByKey;
	int32 HitCount = 0;
	int32 MissCount = 0;
};

