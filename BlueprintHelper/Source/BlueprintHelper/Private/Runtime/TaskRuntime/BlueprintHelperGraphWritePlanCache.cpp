// BlueprintHelper TaskRuntime GraphWrite pure data plan cache.

#include "Runtime/TaskRuntime/BlueprintHelperGraphWritePlanCache.h"

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCacheKeyUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

bool FBlueprintHelperGraphWritePlanCacheKey::IsValid() const
{
	return !CacheSchemaVersion.IsEmpty() &&
		!PayloadHash.IsEmpty() &&
		!GraphSchemaHash.IsEmpty() &&
		!AssetStateHash.IsEmpty();
}

FString FBlueprintHelperGraphWritePlanCacheKey::ToStorageKey() const
{
	return FString::Printf(
		TEXT("%s|payload=%s|schema=%s|asset=%s"),
		*CacheSchemaVersion,
		*PayloadHash,
		*GraphSchemaHash,
		*AssetStateHash);
}

FBlueprintHelperGraphWritePlanCache::FBlueprintHelperGraphWritePlanCache(
	const FBlueprintHelperTaskRuntimeCacheConfig& InConfig)
	: Config(InConfig)
{
}

bool FBlueprintHelperGraphWritePlanCache::TryGet(
	const FBlueprintHelperGraphWritePlanCacheKey& Key,
	const FDateTime& NowUtc,
	FBlueprintHelperGraphWritePlanCacheEntry& OutEntry)
{
	PruneIfNeeded(NowUtc);
	if (!Key.IsValid())
	{
		++RequestStats.Misses;
		return false;
	}

	FBlueprintHelperGraphWritePlanCacheEntry* Found = EntriesByKey.Find(Key.ToStorageKey());
	if (!Found)
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
	return true;
}

void FBlueprintHelperGraphWritePlanCache::Store(
	const FBlueprintHelperGraphWritePlanCacheKey& Key,
	const FBlueprintHelperGraphWritePlanCacheEntry& Entry,
	const FDateTime& NowUtc)
{
	PruneIfNeeded(NowUtc);
	if (!Key.IsValid())
	{
		return;
	}

	FBlueprintHelperGraphWritePlanCacheEntry Stored = CloneEntryForStore(Entry, NowUtc);
	const FString StorageKey = Key.ToStorageKey();
	if (const FBlueprintHelperGraphWritePlanCacheEntry* Existing = EntriesByKey.Find(StorageKey))
	{
		CurrentBytes = FMath::Max<int64>(0, CurrentBytes - Existing->EstimatedBytes);
	}
	CurrentBytes += Stored.EstimatedBytes;
	EntriesByKey.Add(StorageKey, MoveTemp(Stored));
	TrimToBounds();
}

void FBlueprintHelperGraphWritePlanCache::ResetRequestStats()
{
	RequestStats = FBlueprintHelperGraphWritePlanCacheStats();
}

FBlueprintHelperGraphWritePlanCacheStats FBlueprintHelperGraphWritePlanCache::GetStats() const
{
	FBlueprintHelperGraphWritePlanCacheStats Stats = RequestStats;
	Stats.Entries = EntriesByKey.Num();
	Stats.CurrentBytes = CurrentBytes;
	return Stats;
}

FBlueprintHelperGraphWritePlanCacheEntry FBlueprintHelperGraphWritePlanCache::MakeEntryFromPayload(
	const FString& GraphName,
	const TSharedPtr<FJsonObject>& Payload,
	const TArray<FString>& ResolvedCallFunctionStableIds)
{
	FBlueprintHelperGraphWritePlanCacheEntry Entry;
	Entry.NormalizedGraphName = GraphName.ToLower();
	Entry.ResolvedCallFunctionStableIds = ResolvedCallFunctionStableIds;
	Entry.ResolvedCallFunctionStableIds.Sort();

	const TSharedPtr<FJsonObject>* LogicSpecPtr = nullptr;
	if (Payload.IsValid() &&
		Payload->TryGetObjectField(TEXT("logic_spec"), LogicSpecPtr) &&
		LogicSpecPtr && LogicSpecPtr->IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* Statements = nullptr;
		if ((*LogicSpecPtr)->TryGetArrayField(TEXT("statements"), Statements) && Statements)
		{
			for (int32 Index = 0; Index < Statements->Num(); ++Index)
			{
				const TSharedPtr<FJsonObject> Statement = (*Statements)[Index].IsValid()
					? (*Statements)[Index]->AsObject()
					: nullptr;
				FString Kind;
				if (Statement.IsValid())
				{
					Statement->TryGetStringField(TEXT("kind"), Kind);
				}
				Entry.OrderedOpSummary.Add(FString::Printf(TEXT("%d:%s"), Index, *Kind));
			}
		}
	}
	return Entry;
}

void FBlueprintHelperGraphWritePlanCache::PruneIfNeeded(const FDateTime& NowUtc)
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

void FBlueprintHelperGraphWritePlanCache::PruneExpired(const FDateTime& NowUtc)
{
	TArray<FString> ExpiredKeys;
	for (const TPair<FString, FBlueprintHelperGraphWritePlanCacheEntry>& Pair : EntriesByKey)
	{
		if (Pair.Value.ExpiresAtUtc <= NowUtc)
		{
			ExpiredKeys.Add(Pair.Key);
		}
	}

	for (const FString& Key : ExpiredKeys)
	{
		if (const FBlueprintHelperGraphWritePlanCacheEntry* Entry = EntriesByKey.Find(Key))
		{
			CurrentBytes = FMath::Max<int64>(0, CurrentBytes - Entry->EstimatedBytes);
		}
		EntriesByKey.Remove(Key);
		++RequestStats.PrunedExpiredEntries;
	}
}

void FBlueprintHelperGraphWritePlanCache::TrimToBounds()
{
	auto RemoveOldest = [&]()
	{
		FString OldestKey;
		FDateTime OldestAccess = FDateTime::MaxValue();
		for (const TPair<FString, FBlueprintHelperGraphWritePlanCacheEntry>& Pair : EntriesByKey)
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
		if (const FBlueprintHelperGraphWritePlanCacheEntry* Entry = EntriesByKey.Find(OldestKey))
		{
			CurrentBytes = FMath::Max<int64>(0, CurrentBytes - Entry->EstimatedBytes);
		}
		EntriesByKey.Remove(OldestKey);
		++RequestStats.PrunedCapacityEntries;
		return true;
	};

	while (EntriesByKey.Num() > Config.GraphWritePlanMaxEntries && RemoveOldest())
	{
	}

	while (CurrentBytes > Config.GraphWritePlanMaxBytes && RemoveOldest())
	{
	}
}

FBlueprintHelperGraphWritePlanCacheEntry FBlueprintHelperGraphWritePlanCache::CloneEntryForStore(
	const FBlueprintHelperGraphWritePlanCacheEntry& Entry,
	const FDateTime& NowUtc) const
{
	FBlueprintHelperGraphWritePlanCacheEntry Cloned = Entry;
	Cloned.CreatedAtUtc = Entry.CreatedAtUtc.GetTicks() > 0 ? Entry.CreatedAtUtc : NowUtc;
	Cloned.ExpiresAtUtc = Entry.ExpiresAtUtc.GetTicks() > 0
		? Entry.ExpiresAtUtc
		: Cloned.CreatedAtUtc + Config.GraphWritePlanTtl;
	Cloned.LastAccessedAtUtc = NowUtc;
	Cloned.EstimatedBytes = Entry.EstimatedBytes > 0 ? Entry.EstimatedBytes : EstimateEntryBytes(Cloned);
	return Cloned;
}

int64 FBlueprintHelperGraphWritePlanCache::EstimateEntryBytes(
	const FBlueprintHelperGraphWritePlanCacheEntry& Entry) const
{
	int64 Bytes = FTCHARToUTF8(*Entry.NormalizedGraphName).Length();
	for (const FString& Item : Entry.OrderedOpSummary)
	{
		Bytes += FTCHARToUTF8(*Item).Length();
	}
	for (const TPair<FString, FString>& Pair : Entry.NodeIdToPlannedNodeKind)
	{
		Bytes += FTCHARToUTF8(*Pair.Key).Length() + FTCHARToUTF8(*Pair.Value).Length();
	}
	for (const TPair<FString, FString>& Pair : Entry.PinAliasMap)
	{
		Bytes += FTCHARToUTF8(*Pair.Key).Length() + FTCHARToUTF8(*Pair.Value).Length();
	}
	for (const FString& StableId : Entry.ResolvedCallFunctionStableIds)
	{
		Bytes += FTCHARToUTF8(*StableId).Length();
	}
	return Bytes;
}
