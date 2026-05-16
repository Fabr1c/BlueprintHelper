// Review graph resolver utility helpers.

#pragma once

#include "CoreMinimal.h"

class UBlueprint;
class UEdGraph;

class FBlueprintHelperReviewGraphResolverUtils
{
public:
	static UEdGraph* FindGraphByName(const UBlueprint* Blueprint, const FString& GraphName);
};
