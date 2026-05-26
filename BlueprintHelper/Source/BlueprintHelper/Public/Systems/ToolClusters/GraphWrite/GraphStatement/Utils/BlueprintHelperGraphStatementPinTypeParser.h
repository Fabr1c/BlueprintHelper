#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"

class BLUEPRINTHELPER_API FBlueprintHelperGraphStatementPinTypeParser
{
public:
	static FBlueprintHelperCallFunctionPinType ParsePinTypeToken(const FString& Token);
};
