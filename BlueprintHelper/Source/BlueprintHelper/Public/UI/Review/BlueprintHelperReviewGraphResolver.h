#pragma once

#include "CoreMinimal.h"

class UBlueprint;
class UEdGraph;

class BLUEPRINTHELPER_API FBlueprintHelperReviewGraphResolver
{
public:
	static UEdGraph* ResolveGraphForReviewSelection(const UBlueprint* Blueprint, const FString& RequestedGraphName);
};
