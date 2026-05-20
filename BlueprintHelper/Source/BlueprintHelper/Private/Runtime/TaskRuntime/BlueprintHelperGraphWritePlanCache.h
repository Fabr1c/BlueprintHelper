// BlueprintHelper TaskRuntime GraphWrite pure data plan cache.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCacheConfig.h"

struct FBlueprintHelperGraphWritePlanCacheKey
{
	FString CacheSchemaVersion = TEXT("BlueprintHelper.GraphWritePlanCache.v1");
	FString PayloadHash;
	FString GraphSchemaHash;
	FString AssetStateHash;

	bool IsValid() const;
	FString ToStorageKey() const;
};

struct FBlueprintHelperGraphWritePlanCacheEntry
{
	FString NormalizedGraphName;
	TArray<FString> OrderedOpSummary;
	TMap<FString, FString> NodeIdToPlannedNodeKind;
	TMap<FString, FString> PinAliasMap;
	TArray<FString> ResolvedCallFunctionStableIds;
	FDateTime CreatedAtUtc;
	FDateTime ExpiresAtUtc;
	FDateTime LastAccessedAtUtc;
	int64 EstimatedBytes = 0;
};

struct FBlueprintHelperGraphWritePlanCacheStats
{
	int32 Hits = 0;
	int32 Misses = 0;
	int32 Entries = 0;
	int32 PrunedExpiredEntries = 0;
	int32 PrunedCapacityEntries = 0;
	int64 CurrentBytes = 0;
};

class FBlueprintHelperGraphWritePlanCache
{
public:
	explicit FBlueprintHelperGraphWritePlanCache(
		const FBlueprintHelperTaskRuntimeCacheConfig& InConfig = FBlueprintHelperTaskRuntimeCacheConfig::Default());

	bool TryGet(
		const FBlueprintHelperGraphWritePlanCacheKey& Key,
		const FDateTime& NowUtc,
		FBlueprintHelperGraphWritePlanCacheEntry& OutEntry);

	void Store(
		const FBlueprintHelperGraphWritePlanCacheKey& Key,
		const FBlueprintHelperGraphWritePlanCacheEntry& Entry,
		const FDateTime& NowUtc);

	void ResetRequestStats();
	FBlueprintHelperGraphWritePlanCacheStats GetStats() const;

	static FBlueprintHelperGraphWritePlanCacheEntry MakeEntryFromPayload(
		const FString& GraphName,
		const TSharedPtr<FJsonObject>& Payload,
		const TArray<FString>& ResolvedCallFunctionStableIds);

private:
	void PruneIfNeeded(const FDateTime& NowUtc);
	void PruneExpired(const FDateTime& NowUtc);
	void TrimToBounds();
	FBlueprintHelperGraphWritePlanCacheEntry CloneEntryForStore(
		const FBlueprintHelperGraphWritePlanCacheEntry& Entry,
		const FDateTime& NowUtc) const;
	int64 EstimateEntryBytes(const FBlueprintHelperGraphWritePlanCacheEntry& Entry) const;

	FBlueprintHelperTaskRuntimeCacheConfig Config;
	TMap<FString, FBlueprintHelperGraphWritePlanCacheEntry> EntriesByKey;
	FDateTime LastPruneAtUtc;
	int64 CurrentBytes = 0;
	FBlueprintHelperGraphWritePlanCacheStats RequestStats;
};
