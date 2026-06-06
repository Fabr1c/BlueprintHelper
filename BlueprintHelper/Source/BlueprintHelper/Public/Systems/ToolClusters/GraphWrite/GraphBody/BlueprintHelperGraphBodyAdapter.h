#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyBoundaryModel.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyMutationPlan.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphConnectivityPolicy.h"

class BLUEPRINTHELPER_API IBlueprintHelperGraphBodyAdapter
{
public:
	virtual ~IBlueprintHelperGraphBodyAdapter() = default;
	virtual FString GetAdapterId() const = 0;
	virtual FBlueprintHelperGraphBodyBoundaryModel BuildBoundaryModel(
		const TSharedRef<FJsonObject>& Payload) const = 0;
	virtual FBlueprintHelperGraphConnectivityPolicy SelectConnectivityPolicy(
		const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const = 0;
	virtual FBlueprintHelperGraphBodyMutationPlan BuildMutationPlan(
		const TSharedRef<FJsonObject>& Payload) const = 0;
};
