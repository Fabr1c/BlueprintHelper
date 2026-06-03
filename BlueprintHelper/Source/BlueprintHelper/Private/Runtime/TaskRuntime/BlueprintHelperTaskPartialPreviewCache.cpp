// BlueprintHelper TaskRuntime partial preview cache.

#include "Runtime/TaskRuntime/BlueprintHelperTaskPartialPreviewCache.h"

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCacheKeyUtils.h"

#include "Dom/JsonValue.h"

bool FBlueprintHelperPartialPreviewCacheKey::IsValid() const
{
	return !TaskSpecGroupHash.IsEmpty() &&
		!StepId.IsEmpty() &&
		!StepPayloadHash.IsEmpty() &&
		!DependencyClosureHash.IsEmpty() &&
		!ExecutionPolicyHash.IsEmpty() &&
		!AssetStateHash.IsEmpty() &&
		!ContextRevisionManifestHash.IsEmpty() &&
		!DryRunPlannedStateHash.IsEmpty() &&
		!CacheSchemaVersion.IsEmpty();
}

FString FBlueprintHelperPartialPreviewCacheKey::ToStorageKey() const
{
	return FString::Printf(
		TEXT("%s|group=%s|step=%s|payload=%s|deps=%s|policy=%s|asset=%s|context=%s|planned=%s"),
		*CacheSchemaVersion,
		*TaskSpecGroupHash,
		*StepId,
		*StepPayloadHash,
		*DependencyClosureHash,
		*ExecutionPolicyHash,
		*AssetStateHash,
		*ContextRevisionManifestHash,
		*DryRunPlannedStateHash);
}

FBlueprintHelperTaskPartialPreviewCache::FBlueprintHelperTaskPartialPreviewCache(
	const FBlueprintHelperTaskRuntimeCacheConfig& InConfig)
	: Config(InConfig)
{
}

bool FBlueprintHelperTaskPartialPreviewCache::TryGet(
	const FBlueprintHelperPartialPreviewCacheKey& Key,
	const FDateTime& NowUtc,
	FBlueprintHelperPartialPreviewCacheEntry& OutEntry)
{
	PruneIfNeeded(NowUtc);
	if (!Key.IsValid())
	{
		++RequestStats.Misses;
		return false;
	}

	FBlueprintHelperPartialPreviewCacheEntry* Found = EntriesByKey.Find(Key.ToStorageKey());
	if (!Found || !Found->bPassed)
	{
		++RequestStats.Misses;
		return false;
	}

	if (Found->ExpiresAtUtc <= NowUtc)
	{
		CurrentBytes = FMath::Max<int64>(0, CurrentBytes - Found->EstimatedBytes);
		EntriesByKey.Remove(Key.ToStorageKey());
		++RequestStats.PrunedExpiredEntries;
		++RequestStats.Misses;
		return false;
	}

	Found->LastAccessedAtUtc = NowUtc;
	OutEntry = CloneEntryForStore(*Found, Found->CreatedAtUtc);
	OutEntry.CreatedAtUtc = Found->CreatedAtUtc;
	OutEntry.ExpiresAtUtc = Found->ExpiresAtUtc;
	OutEntry.LastAccessedAtUtc = Found->LastAccessedAtUtc;
	++RequestStats.Hits;
	RequestStats.ReusedSteps.Add(Found->StepId);
	return true;
}

void FBlueprintHelperTaskPartialPreviewCache::Store(
	const FBlueprintHelperPartialPreviewCacheKey& Key,
	const FBlueprintHelperPartialPreviewCacheEntry& Entry,
	const FDateTime& NowUtc)
{
	PruneIfNeeded(NowUtc);
	if (!Key.IsValid() || !Entry.bPassed)
	{
		return;
	}

	FBlueprintHelperPartialPreviewCacheEntry Stored = CloneEntryForStore(Entry, NowUtc);
	const FString StorageKey = Key.ToStorageKey();
	if (const FBlueprintHelperPartialPreviewCacheEntry* Existing = EntriesByKey.Find(StorageKey))
	{
		CurrentBytes = FMath::Max<int64>(0, CurrentBytes - Existing->EstimatedBytes);
	}
	CurrentBytes += Stored.EstimatedBytes;
	EntriesByKey.Add(StorageKey, MoveTemp(Stored));
	TrimToBounds();
}

void FBlueprintHelperTaskPartialPreviewCache::ResetRequestStats()
{
	RequestStats = FBlueprintHelperPartialPreviewCacheStats();
}

FBlueprintHelperPartialPreviewCacheStats FBlueprintHelperTaskPartialPreviewCache::GetStats() const
{
	FBlueprintHelperPartialPreviewCacheStats Stats = RequestStats;
	Stats.Entries = EntriesByKey.Num();
	Stats.CurrentBytes = CurrentBytes;
	return Stats;
}

void FBlueprintHelperTaskPartialPreviewCache::PruneIfNeeded(const FDateTime& NowUtc)
{
	if (LastPruneAtUtc.GetTicks() > 0 &&
		NowUtc - LastPruneAtUtc < Config.PruneOnAccessMinInterval)
	{
		return;
	}

	PruneExpired(NowUtc);
	TrimToBounds();
	LastPruneAtUtc = NowUtc;
}

void FBlueprintHelperTaskPartialPreviewCache::PruneExpired(const FDateTime& NowUtc)
{
	TArray<FString> ExpiredKeys;
	for (const TPair<FString, FBlueprintHelperPartialPreviewCacheEntry>& Pair : EntriesByKey)
	{
		if (Pair.Value.ExpiresAtUtc <= NowUtc)
		{
			ExpiredKeys.Add(Pair.Key);
		}
	}

	for (const FString& Key : ExpiredKeys)
	{
		if (const FBlueprintHelperPartialPreviewCacheEntry* Entry = EntriesByKey.Find(Key))
		{
			CurrentBytes = FMath::Max<int64>(0, CurrentBytes - Entry->EstimatedBytes);
		}
		EntriesByKey.Remove(Key);
		++RequestStats.PrunedExpiredEntries;
	}
}

void FBlueprintHelperTaskPartialPreviewCache::TrimToBounds()
{
	auto RemoveOldest = [&]()
	{
		FString OldestKey;
		FDateTime OldestAccess = FDateTime::MaxValue();
		for (const TPair<FString, FBlueprintHelperPartialPreviewCacheEntry>& Pair : EntriesByKey)
		{
			if (Pair.Value.LastAccessedAtUtc < OldestAccess)
			{
				OldestAccess = Pair.Value.LastAccessedAtUtc;
				OldestKey = Pair.Key;
			}
		}
		if (OldestKey.IsEmpty())
		{
			return false;
		}
		if (const FBlueprintHelperPartialPreviewCacheEntry* Entry = EntriesByKey.Find(OldestKey))
		{
			CurrentBytes = FMath::Max<int64>(0, CurrentBytes - Entry->EstimatedBytes);
		}
		EntriesByKey.Remove(OldestKey);
		++RequestStats.PrunedCapacityEntries;
		return true;
	};

	while (EntriesByKey.Num() > Config.PartialPreviewMaxStepEntries && RemoveOldest())
	{
	}

	while (CountGroups() > Config.PartialPreviewMaxGroups && RemoveOldest())
	{
	}

	while (CurrentBytes > Config.PartialPreviewMaxBytes && RemoveOldest())
	{
	}
}

FBlueprintHelperPartialPreviewCacheEntry FBlueprintHelperTaskPartialPreviewCache::CloneEntryForStore(
	const FBlueprintHelperPartialPreviewCacheEntry& Entry,
	const FDateTime& NowUtc) const
{
	FBlueprintHelperPartialPreviewCacheEntry Cloned;
	Cloned.StepId = Entry.StepId;
	Cloned.bPassed = Entry.bPassed;
	Cloned.Result = FBlueprintHelperTaskRuntimeCacheKeyUtils::CloneToolResult(Entry.Result);
	for (const TSharedPtr<FJsonValue>& Value : Entry.RuntimeFactValues)
	{
		Cloned.RuntimeFactValues.Add(FBlueprintHelperTaskRuntimeCacheKeyUtils::CloneJsonValue(Value));
	}
	Cloned.CreatedAtUtc = Entry.CreatedAtUtc.GetTicks() > 0 ? Entry.CreatedAtUtc : NowUtc;
	Cloned.ExpiresAtUtc = Entry.ExpiresAtUtc.GetTicks() > 0
		? Entry.ExpiresAtUtc
		: Cloned.CreatedAtUtc + Config.PartialPreviewTtl;
	Cloned.LastAccessedAtUtc = NowUtc;
	Cloned.EstimatedBytes = Entry.EstimatedBytes > 0 ? Entry.EstimatedBytes : EstimateEntryBytes(Cloned);
	return Cloned;
}

int64 FBlueprintHelperTaskPartialPreviewCache::EstimateEntryBytes(
	const FBlueprintHelperPartialPreviewCacheEntry& Entry) const
{
	int64 Bytes = FBlueprintHelperTaskRuntimeCacheKeyUtils::EstimateToolResultBytes(Entry.Result);
	for (const TSharedPtr<FJsonValue>& Value : Entry.RuntimeFactValues)
	{
		Bytes += FBlueprintHelperTaskRuntimeCacheKeyUtils::EstimateJsonValueBytes(Value);
	}
	return Bytes;
}

int32 FBlueprintHelperTaskPartialPreviewCache::CountGroups() const
{
	TSet<FString> Groups;
	for (const TPair<FString, FBlueprintHelperPartialPreviewCacheEntry>& Pair : EntriesByKey)
	{
		FString Left;
		FString Right;
		if (Pair.Key.Split(TEXT("|step="), &Left, &Right))
		{
			Groups.Add(Left);
		}
	}
	return Groups.Num();
}
