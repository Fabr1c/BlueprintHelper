// BlueprintHelper Service Layer - internal dependency analysis service.

#pragma once

#include "CoreMinimal.h"
#include "Structure/BlueprintHelperDependencyAnalysisTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperDependencyAnalysisService
{
public:
	bool TryBuildReferenceContext(
		const FBlueprintHelperDependencyAnalysisTarget& Target,
		const FBlueprintHelperDependencyAnalysisOptions& Options,
		const FString& Scope,
		bool bIncludeSamples,
		FBlueprintHelperReferenceContextPack& OutContext,
		FString& OutErrorCode,
		FString& OutErrorMessage) const;
};
