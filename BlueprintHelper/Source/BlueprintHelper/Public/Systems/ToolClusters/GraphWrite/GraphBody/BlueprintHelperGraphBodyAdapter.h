#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyBoundaryModel.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyMutationPlan.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyReadbackProjection.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyReconnectPlan.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyRequest.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodySemanticContext.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyTarget.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphConnectivityPolicy.h"

class BLUEPRINTHELPER_API IBlueprintHelperGraphBodyAdapter
{
public:
	virtual ~IBlueprintHelperGraphBodyAdapter() = default;
	virtual FString GetAdapterId() const = 0;
	virtual bool ResolveTarget(
		const FBlueprintHelperGraphBodyRequest& Request,
		FBlueprintHelperGraphBodyTarget& OutTarget,
		FString& OutError) const = 0;
	virtual FBlueprintHelperGraphBodyBoundaryModel BuildBoundaryModel(
		const FBlueprintHelperGraphBodyTarget& Target,
		const FBlueprintHelperGraphBodyRequest& Request) const = 0;
	virtual FBlueprintHelperGraphBodyMutationPlan BuildMutationPlan(
		const FBlueprintHelperGraphBodyTarget& Target,
		const FBlueprintHelperGraphBodyBoundaryModel& Boundary,
		const FBlueprintHelperGraphBodyRequest& Request) const = 0;
	virtual FBlueprintHelperGraphBodySemanticContext BuildSemanticContext(
		const FBlueprintHelperGraphBodyTarget& Target,
		const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const = 0;
	virtual FBlueprintHelperGraphBodyReconnectPlan BuildReconnectPlan(
		const FBlueprintHelperGraphBodyTarget& Target,
		const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const = 0;
	virtual FBlueprintHelperGraphConnectivityPolicy BuildConnectivityPolicy(
		const FBlueprintHelperGraphBodyTarget& Target,
		const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const = 0;
	virtual FBlueprintHelperGraphBodyReadbackProjection BuildReadbackProjection(
		const FBlueprintHelperGraphBodyTarget& Target,
		const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const = 0;
};
