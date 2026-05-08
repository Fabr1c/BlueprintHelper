// BlueprintHelper TaskRuntime - DataTable static cluster

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"

class FBlueprintHelperDataTableService;

class BLUEPRINTHELPER_API FBlueprintHelperDataTableTaskRuntimeCluster
{
public:
	explicit FBlueprintHelperDataTableTaskRuntimeCluster(
		const FBlueprintHelperDataTableService& InDataTableService);

	static bool CanExecuteStep(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep);

	FBlueprintHelperToolResultBase ExecuteStep(const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) const;

private:
	const FBlueprintHelperDataTableService& DataTableService;
};
