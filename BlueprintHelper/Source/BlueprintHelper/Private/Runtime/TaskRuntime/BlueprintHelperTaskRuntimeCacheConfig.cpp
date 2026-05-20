// BlueprintHelper TaskRuntime cache configuration.

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCacheConfig.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeSettingsResolver.h"

FBlueprintHelperTaskRuntimeCacheConfig FBlueprintHelperTaskRuntimeCacheConfig::Default()
{
	const FBlueprintHelperTaskRuntimeCacheSettings Settings =
		FBlueprintHelperTaskRuntimeSettingsResolver::LoadCacheSettings();

	FBlueprintHelperTaskRuntimeCacheConfig Config;
	Config.PartialPreviewTtl = FTimespan::FromSeconds(Settings.PartialPreview.TtlSeconds);
	Config.PartialPreviewMaxGroups = Settings.PartialPreview.MaxGroups;
	Config.PartialPreviewMaxStepEntries = Settings.PartialPreview.MaxStepEntries;
	Config.PartialPreviewMaxBytes = Settings.PartialPreview.MaxBytes;

	Config.CallFunctionFactTtl = FTimespan::FromSeconds(Settings.CallFunctionFact.TtlSeconds);
	Config.CallFunctionFactMaxEntries = Settings.CallFunctionFact.MaxEntries;
	Config.CallFunctionFactMaxBytes = Settings.CallFunctionFact.MaxBytes;

	Config.GraphWritePlanTtl = FTimespan::FromSeconds(Settings.GraphWritePlan.TtlSeconds);
	Config.GraphWritePlanMaxEntries = Settings.GraphWritePlan.MaxEntries;
	Config.GraphWritePlanMaxBytes = Settings.GraphWritePlan.MaxBytes;

	Config.PruneOnAccessMinInterval = FTimespan::FromSeconds(Settings.PruneOnAccessMinIntervalSeconds);
	return Config;
}
