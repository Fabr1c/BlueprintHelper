#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class UEdGraphNode;

struct FBlueprintHelperGenericOpsReadbackExpectation
{
	FString Family;
	FString OperationId;
	FString StableId;
	FString NodeClassPath;
	TMap<FString, FString> RequiredFacts;
	TArray<FString> RequiredPins;
	TArray<FString> RequiredInputPins;
	TArray<FString> RequiredOutputPins;
	bool bRequireSelectedSpawner = true;
	bool bRequireSelectedFunction = false;
	bool bRequireNoWildcardResidual = true;
};

class FBlueprintHelperGenericOpsReadbackVerifier
{
public:
	static bool Verify(
		const FBlueprintHelperActionResolutionResult& Result,
		const FBlueprintHelperGenericOpsReadbackExpectation& Expectation,
		FString& OutFailureCode,
		FString& OutFailure);

	static bool Verify(
		const FBlueprintHelperActionResolutionResult& Result,
		const UEdGraphNode* SpawnedNode,
		const FBlueprintHelperGenericOpsReadbackExpectation& Expectation,
		FString& OutFailureCode,
		FString& OutFailure);
};
