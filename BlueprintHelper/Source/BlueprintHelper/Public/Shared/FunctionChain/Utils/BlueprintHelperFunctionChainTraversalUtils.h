// BlueprintHelper Service Layer - FunctionChain graph traversal utilities

#pragma once

#include "CoreMinimal.h"
#include "Shared/FunctionChain/BlueprintHelperFunctionChainContextTypes.h"

class UBlueprint;

class FBlueprintHelperFunctionChainTraversalUtils
{
public:
	static bool BuildContext(
		UBlueprint* Blueprint,
		const FBlueprintHelperFunctionChainContextRequest& Request,
		FBlueprintHelperFunctionChainContextPack& OutContext,
		FString& OutErrorCode,
		FString& OutErrorMessage);
};
