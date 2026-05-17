// BlueprintHelper Service Layer - FunctionChainContext read service

#pragma once

#include "CoreMinimal.h"
#include "Shared/FunctionChain/BlueprintHelperFunctionChainContextTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperFunctionChainContextService
{
public:
	bool TryBuildFunctionChainContext(
		const FBlueprintHelperFunctionChainContextRequest& Request,
		FBlueprintHelperFunctionChainContextPack& OutContext,
		FString& OutErrorCode,
		FString& OutErrorMessage) const;
};
