// BlueprintHelper Metrics store reader.

#pragma once

#include "CoreMinimal.h"
#include "Systems/Metrics/BlueprintHelperMetricsData.h"

class BLUEPRINTHELPER_API FBlueprintHelperMetricsStoreReader
{
public:
	static FString ResolveDefaultMetricsRoot();
	static FString ResolveMetricsRootFromEnvironment();
	static FBlueprintHelperMetricsLoadResult LoadDefault();
	static FBlueprintHelperMetricsLoadResult LoadFromRoot(const FString& MetricsRoot);
};
