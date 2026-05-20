// BlueprintHelper TaskRuntime cache configuration.

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCacheConfig.h"

FBlueprintHelperTaskRuntimeCacheConfig FBlueprintHelperTaskRuntimeCacheConfig::Default()
{
	FBlueprintHelperTaskRuntimeCacheConfig Config;
	Config.PartialPreviewTtl = FTimespan::FromSeconds(40.0);
	Config.PartialPreviewMaxGroups = 64;
	Config.PartialPreviewMaxStepEntries = 512;
	Config.PartialPreviewMaxBytes = int64(8) * 1024 * 1024;

	Config.CallFunctionFactTtl = FTimespan::FromSeconds(180.0);
	Config.CallFunctionFactMaxEntries = 2048;
	Config.CallFunctionFactMaxBytes = int64(8) * 1024 * 1024;

	Config.GraphWritePlanTtl = FTimespan::FromSeconds(90.0);
	Config.GraphWritePlanMaxEntries = 256;
	Config.GraphWritePlanMaxBytes = int64(16) * 1024 * 1024;

	Config.PruneOnAccessMinInterval = FTimespan::FromSeconds(1.0);
	return Config;
}
