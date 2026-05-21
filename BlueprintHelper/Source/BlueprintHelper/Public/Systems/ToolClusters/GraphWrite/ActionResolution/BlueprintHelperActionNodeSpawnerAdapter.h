#pragma once

#include "CoreMinimal.h"

class UEdGraph;
class UK2Node;
struct FBlueprintHelperActionResolutionResult;

class BLUEPRINTHELPER_API FBlueprintHelperActionNodeSpawnerAdapter
{
public:
	static UK2Node* InvokeSelectedSpawner(
		UEdGraph* TargetGraph,
		const FBlueprintHelperActionResolutionResult& ActionResult,
		const FVector2D& Location,
		FString& OutError);
};
