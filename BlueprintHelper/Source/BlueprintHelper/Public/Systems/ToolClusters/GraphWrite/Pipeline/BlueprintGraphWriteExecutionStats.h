#pragma once

#include "CoreMinimal.h"

class FJsonObject;

struct BLUEPRINTHELPER_API FBlueprintGraphWriteExecutionStats
{
	int32 RequestedNodeCount = 0;
	int32 SpawnedNodeCount = 0;
	int32 RequestedDefaultValueCount = 0;
	int32 AppliedDefaultValueCount = 0;
	int32 RequestedLinkCount = 0;
	int32 CreatedLinkCount = 0;
	int32 LayoutRecordNodeCount = 0;
	double BuildContextMs = 0.0;
	double BuildPlanMs = 0.0;
	double SpawnNodesMs = 0.0;
	double ApplyDefaultsMs = 0.0;
	double ConnectLinksMs = 0.0;
	double RecordLayoutMs = 0.0;
};

class BLUEPRINTHELPER_API FBlueprintGraphWriteExecutionStatsSerializer
{
public:
	static TSharedRef<FJsonObject> ToJson(const FBlueprintGraphWriteExecutionStats& Stats);
};
