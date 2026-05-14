#pragma once

#include "CoreMinimal.h"
#include "Shared/Services/BlueprintHelperAgentImportService.h"

class FBlueprintHelperGraphResolver;

class BLUEPRINTHELPER_API FBlueprintHelperAgentImportSemanticExecutor
{
public:
	explicit FBlueprintHelperAgentImportSemanticExecutor(const FBlueprintHelperGraphResolver& InResolver);

	FBlueprintHelperAgentImportResult Execute(
		const FString& OriginalJsonText,
		const FBlueprintHelperAgentImportParsedRequest& ParsedRequest) const;

private:
	const FBlueprintHelperGraphResolver& Resolver;
};