#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicReadProjection.h"

TArray<FString> FBlueprintHelperLogicReadProjectionUtils::GetCallbackCapabilities()
{
	return {
		TEXT("ue.raw_snapshot.logic_json"),
		TEXT("ue.raw_snapshot.logic_md"),
		TEXT("ue.raw_snapshot.logic_flow")
	};
}
