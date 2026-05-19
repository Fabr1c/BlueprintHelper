// Request-local cache for read-side logic snapshots.

#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadSnapshotTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperLogicReadRequestSnapshotCache
{
public:
	bool TryGet(
		const FBlueprintHelperLogicReadSnapshotCacheKey& Key,
		FBlueprintHelperLogicReadSnapshot& OutSnapshot);
	void Put(
		const FBlueprintHelperLogicReadSnapshotCacheKey& Key,
		const FBlueprintHelperLogicReadSnapshot& Snapshot);
	void Reset();

	int32 GetHitCount() const;
	int32 GetMissCount() const;

private:
	TMap<FString, FBlueprintHelperLogicReadSnapshot> SnapshotsByKey;
	int32 HitCount = 0;
	int32 MissCount = 0;
};
