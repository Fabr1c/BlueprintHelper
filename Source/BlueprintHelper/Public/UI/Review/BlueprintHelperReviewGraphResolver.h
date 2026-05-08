#pragma once

#include "CoreMinimal.h"

class UBlueprint;
class UEdGraph;

namespace BlueprintHelperReviewGraphResolver
{
	UEdGraph* ResolveGraphForReviewSelection(const UBlueprint* Blueprint, const FString& RequestedGraphName);
}
