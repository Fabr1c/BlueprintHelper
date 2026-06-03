// BlueprintHelper TaskRuntime partial preview cache.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCacheConfig.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FJsonValue;

struct FBlueprintHelperPartialPreviewCacheKey
{
	FString CacheSchemaVersion = TEXT("BlueprintHelper.PartialPreviewCache.v2");
	FString TaskSpecGroupHash;
	FString StepId;
	FString StepPayloadHash;
	FString DependencyClosureHash;
	FString ExecutionPolicyHash;
	FString AssetStateHash;
	FString ContextRevisionManifestHash;
	FString DryRunPlannedStateHash = TEXT("none");

	bool IsValid() const;
	FString ToStorageKey() const;
};

struct FBlueprintHelperPartialPreviewCacheEntry
{
	FString StepId;
	bool bPassed = false;
	FBlueprintHelperToolResultBase Result;
	TArray<TSharedPtr<FJsonValue>> RuntimeFactValues;
	FDateTime CreatedAtUtc;
	FDateTime ExpiresAtUtc;
	FDateTime LastAccessedAtUtc;
	int64 EstimatedBytes = 0;
};

struct FBlueprintHelperPartialPreviewCacheStats
{
	int32 Hits = 0;
	int32 Misses = 0;
	int32 Entries = 0;
	int32 PrunedExpiredEntries = 0;
	int32 PrunedCapacityEntries = 0;
	int64 CurrentBytes = 0;
	TArray<FString> ReusedSteps;
};

class FBlueprintHelperTaskPartialPreviewCache
{
public:
	explicit FBlueprintHelperTaskPartialPreviewCache(
		const FBlueprintHelperTaskRuntimeCacheConfig& InConfig = FBlueprintHelperTaskRuntimeCacheConfig::Default());

	bool TryGet(
		const FBlueprintHelperPartialPreviewCacheKey& Key,
		const FDateTime& NowUtc,
		FBlueprintHelperPartialPreviewCacheEntry& OutEntry);

	void Store(
		const FBlueprintHelperPartialPreviewCacheKey& Key,
		const FBlueprintHelperPartialPreviewCacheEntry& Entry,
		const FDateTime& NowUtc);

	void ResetRequestStats();
	FBlueprintHelperPartialPreviewCacheStats GetStats() const;

private:
	void PruneIfNeeded(const FDateTime& NowUtc);
	void PruneExpired(const FDateTime& NowUtc);
	void TrimToBounds();
	FBlueprintHelperPartialPreviewCacheEntry CloneEntryForStore(
		const FBlueprintHelperPartialPreviewCacheEntry& Entry,
		const FDateTime& NowUtc) const;
	int64 EstimateEntryBytes(const FBlueprintHelperPartialPreviewCacheEntry& Entry) const;
	int32 CountGroups() const;

	FBlueprintHelperTaskRuntimeCacheConfig Config;
	TMap<FString, FBlueprintHelperPartialPreviewCacheEntry> EntriesByKey;
	FDateTime LastPruneAtUtc;
	int64 CurrentBytes = 0;
	FBlueprintHelperPartialPreviewCacheStats RequestStats;
};
