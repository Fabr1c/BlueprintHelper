#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyBoundaryModel.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphConnectivityPolicy.h"

struct BLUEPRINTHELPER_API FBlueprintHelperGraphBodyMutationPlanStep
{
	FString StepId;
	FString Description;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphBodyMutationPlan
{
	FString AdapterId;
	FBlueprintHelperGraphBodyBoundaryModel BoundaryModel;
	FBlueprintHelperGraphConnectivityPolicy ConnectivityPolicy;
	TArray<FBlueprintHelperGraphBodyMutationPlanStep> Steps;
	bool bCreatesNodesInsideAdapter = false;
};
