// BlueprintHelper Metrics time-series projection service.

#pragma once

#include "CoreMinimal.h"
#include "Systems/Metrics/BlueprintHelperMetricsData.h"

class BLUEPRINTHELPER_API FBlueprintHelperMetricsTimeSeriesService
{
public:
	static FBlueprintHelperMetricsPanelSnapshot BuildSnapshot(
		const FBlueprintHelperMetricsLoadResult& LoadResult,
		const FBlueprintHelperMetricsQuery& Query);
};
