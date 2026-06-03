// BlueprintHelper TaskRuntime CallFunction resolution cache.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCacheConfig.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"

struct FBlueprintHelperTaskRuntimeCallFunctionResolutionCacheStats
{
	int32 Hits = 0;
	int32 Misses = 0;
	int32 Entries = 0;
	int32 PrunedExpiredEntries = 0;
	int32 PrunedCapacityEntries = 0;
	int64 CurrentBytes = 0;
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
	FString AssetStateHash;
	FString ContextRevisionManifestHash;
	FString ResolverVersion;
	FDateTime CreatedAtUtc;
	FDateTime ExpiresAtUtc;
	FDateTime LastAccessedAtUtc;
	int64 EstimatedBytes = 0;
};

class FBlueprintHelperTaskRuntimeCallFunctionResolutionCache
{
public:
	explicit FBlueprintHelperTaskRuntimeCallFunctionResolutionCache(
		const FBlueprintHelperTaskRuntimeCacheConfig& InConfig = FBlueprintHelperTaskRuntimeCacheConfig::Default());

	bool TryGet(const FString& Key, FBlueprintHelperTaskRuntimeCachedCallFunctionResolution& OutValue);
	bool TryGet(
		const FString& Key,
		const FString& AssetStateHash,
		const FDateTime& NowUtc,
		FBlueprintHelperTaskRuntimeCachedCallFunctionResolution& OutValue);
	bool TryGet(
		const FString& Key,
		const FString& AssetStateHash,
		const FString& ContextRevisionManifestHash,
		const FDateTime& NowUtc,
		FBlueprintHelperTaskRuntimeCachedCallFunctionResolution& OutValue);
	void Store(const FString& Key, const FBlueprintHelperTaskRuntimeCachedCallFunctionResolution& Value);
	void Store(
		const FString& Key,
		const FBlueprintHelperTaskRuntimeCachedCallFunctionResolution& Value,
		const FDateTime& NowUtc);
	void ResetRequestStats();
	FBlueprintHelperTaskRuntimeCallFunctionResolutionCacheStats GetStats() const;

	static FString MakeKey(
		const FBlueprintHelperCallFunctionResolveRequest& Request,
		const FString& AssetPath,
		const FString& GraphName);
	static FString CurrentResolverVersion();

private:
	void PruneIfNeeded(const FDateTime& NowUtc);
	void PruneExpired(const FDateTime& NowUtc);
	void TrimToBounds();
	FBlueprintHelperTaskRuntimeCachedCallFunctionResolution CloneForStore(
		const FBlueprintHelperTaskRuntimeCachedCallFunctionResolution& Value,
		const FDateTime& NowUtc) const;
	int64 EstimateBytes(const FBlueprintHelperTaskRuntimeCachedCallFunctionResolution& Value) const;

	FBlueprintHelperTaskRuntimeCacheConfig Config;
	TMap<FString, FBlueprintHelperTaskRuntimeCachedCallFunctionResolution> ValuesByKey;
	FDateTime LastPruneAtUtc;
	int64 CurrentBytes = 0;
	FBlueprintHelperTaskRuntimeCallFunctionResolutionCacheStats RequestStats;
};
