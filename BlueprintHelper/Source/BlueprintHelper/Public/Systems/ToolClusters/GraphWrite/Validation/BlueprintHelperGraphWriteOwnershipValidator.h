#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteResultTypes.h"

struct BLUEPRINTHELPER_API FBlueprintHelperGraphWriteOwnershipValidationInput
{
	TArray<FString> GeneratedBlockRefs;
	TArray<FBlueprintHelperReviewAtomicTarget> AtomicTargets;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphWriteOwnershipValidationResult
{
	bool bPassed = true;
	TArray<FBlueprintGeneratorDiagnostic> Diagnostics;
};

class BLUEPRINTHELPER_API FBlueprintHelperGraphWriteOwnershipValidator
{
public:
	static FBlueprintHelperGraphWriteOwnershipValidationResult Validate(
		const FBlueprintHelperGraphWriteOwnershipValidationInput& Input);
};
