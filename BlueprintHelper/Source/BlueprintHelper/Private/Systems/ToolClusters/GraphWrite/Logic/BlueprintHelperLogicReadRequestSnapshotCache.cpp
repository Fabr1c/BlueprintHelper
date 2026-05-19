#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadRequestSnapshotCache.h"

bool FBlueprintHelperLogicReadRequestSnapshotCache::TryGet(
	const FBlueprintHelperLogicReadSnapshotCacheKey& Key,
	FBlueprintHelperLogicReadSnapshot& OutSnapshot)
{
	const FBlueprintHelperLogicReadSnapshot* FoundSnapshot = SnapshotsByKey.Find(Key.ToStableString());
	if (!FoundSnapshot)
	{
		++MissCount;
		return false;
	}

	++HitCount;
	OutSnapshot = *FoundSnapshot;
	return true;
}

void FBlueprintHelperLogicReadRequestSnapshotCache::Put(
	const FBlueprintHelperLogicReadSnapshotCacheKey& Key,
	const FBlueprintHelperLogicReadSnapshot& Snapshot)
{
	SnapshotsByKey.Add(Key.ToStableString(), Snapshot);
}

void FBlueprintHelperLogicReadRequestSnapshotCache::Reset()
{
	SnapshotsByKey.Reset();
	HitCount = 0;
	MissCount = 0;
}

int32 FBlueprintHelperLogicReadRequestSnapshotCache::GetHitCount() const
{
	return HitCount;
}

int32 FBlueprintHelperLogicReadRequestSnapshotCache::GetMissCount() const
{
	return MissCount;
}
