// BlueprintHelper Utils -- TaskRuntime 工具函数库实现

#include "Runtime/TaskRuntime/Utils/BlueprintHelperTaskRuntimeUtils.h"

FString UBlueprintHelperTaskRuntimeUtils::BlueprintHelperNormalizeDryRunMode(FString Mode)
{
	Mode.TrimStartAndEndInline();
	Mode = Mode.ToLower();
	if (Mode == TEXT("quick") || Mode == TEXT("none"))
	{
		return Mode;
	}
	return TEXT("full");
}
