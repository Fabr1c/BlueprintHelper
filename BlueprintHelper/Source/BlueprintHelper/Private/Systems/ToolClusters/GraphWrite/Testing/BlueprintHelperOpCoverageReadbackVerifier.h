#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

struct FBlueprintHelperOpCoverageReadbackExpectation
{
	FString OperationId;
	FString StableId;
	FString NodeClassPath;
	FString ExpectedReturnType;
	TArray<FString> RequiredInputPins;
	bool bTypePromotion = false;
};

class FBlueprintHelperOpCoverageReadbackVerifier
{
public:
	static bool Verify(
		const FBlueprintHelperActionResolutionResult& Result,
		const FBlueprintHelperOpCoverageReadbackExpectation& Expectation,
		FString& OutFailure);
};
