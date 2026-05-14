#pragma once

#include "CoreMinimal.h"
#include "Shared/Services/BlueprintHelperAgentImportService.h"

class BLUEPRINTHELPER_API FBlueprintHelperAgentImportJsonParser
{
public:
	static bool Parse(
		const FString& JsonText,
		FBlueprintHelperAgentImportParsedRequest& OutRequest,
		FBlueprintHelperAgentImportResult& OutResult);
};