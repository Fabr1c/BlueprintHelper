// BlueprintHelper GraphWrite task-runtime dispatcher.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteMutationAdapterTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperGraphWriteRuntimeDispatcher
{
public:
	static bool TryLower(
		const FBlueprintHelperGraphWriteLoweringRequest& Request,
		FBlueprintHelperGraphWriteLoweringResult& OutResult,
		FBlueprintHelperToolError& OutError);
};
