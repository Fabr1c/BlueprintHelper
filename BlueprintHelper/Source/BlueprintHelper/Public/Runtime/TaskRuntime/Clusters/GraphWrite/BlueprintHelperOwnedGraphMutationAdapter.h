// BlueprintHelper owned GraphWrite task-runtime adapter.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteMutationAdapterTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperOwnedGraphMutationAdapter
{
public:
	static bool TryLower(
		const FBlueprintHelperGraphWriteLoweringRequest& Request,
		FBlueprintHelperGraphWriteLoweringResult& OutResult,
		FBlueprintHelperToolError& OutError);
};
