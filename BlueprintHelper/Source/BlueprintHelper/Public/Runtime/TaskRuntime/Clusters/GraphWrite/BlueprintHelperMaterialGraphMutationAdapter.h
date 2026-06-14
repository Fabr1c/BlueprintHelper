// BlueprintHelper MaterialGraph task-runtime mutation adapter.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteMutationAdapterTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperMaterialGraphMutationAdapter
{
public:
	static bool TryLower(
		const FBlueprintHelperGraphWriteLoweringRequest& Request,
		FBlueprintHelperGraphWriteLoweringResult& OutResult,
		FBlueprintHelperToolError& OutError);
};
