#pragma once

#include "CoreMinimal.h"

class UBlueprint;
class UEdGraph;

struct FBlueprintHelperContainerActionReadbackExpectation
{
	FString OperationId;
	FString ContainerKind;
	FString ContainerOperation;
	FString TargetName;
	FString ElementType;
	FString KeyType;
	FString ValueType;
	TArray<FString> RequiredRoles;
	bool bRequiresExecFlow = false;
	bool bRequiresOutput = false;
};

class FBlueprintHelperContainerActionReadbackVerifier
{
public:
	static bool Verify(
		const UBlueprint* Blueprint,
		const UEdGraph* Graph,
		const FBlueprintHelperContainerActionReadbackExpectation& Expectation,
		FString& OutFailure);
};
