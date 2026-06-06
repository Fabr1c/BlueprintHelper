#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyAdapter.h"

class BLUEPRINTHELPER_API FBlueprintHelperK2CustomEventBodyAdapter : public IBlueprintHelperGraphBodyAdapter
{
public:
	virtual FString GetAdapterId() const override;
	virtual bool ResolveTarget(
		const FBlueprintHelperGraphBodyRequest& Request,
		FBlueprintHelperGraphBodyTarget& OutTarget,
		FString& OutError) const override;
	virtual FBlueprintHelperGraphBodyBoundaryModel BuildBoundaryModel(
		const FBlueprintHelperGraphBodyTarget& Target,
		const FBlueprintHelperGraphBodyRequest& Request) const override;
	virtual FBlueprintHelperGraphBodyMutationPlan BuildMutationPlan(
		const FBlueprintHelperGraphBodyTarget& Target,
		const FBlueprintHelperGraphBodyBoundaryModel& Boundary,
		const FBlueprintHelperGraphBodyRequest& Request) const override;
	virtual FBlueprintHelperGraphBodySemanticContext BuildSemanticContext(
		const FBlueprintHelperGraphBodyTarget& Target,
		const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const override;
	virtual FBlueprintHelperGraphBodyReconnectPlan BuildReconnectPlan(
		const FBlueprintHelperGraphBodyTarget& Target,
		const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const override;
	virtual FBlueprintHelperGraphConnectivityPolicy BuildConnectivityPolicy(
		const FBlueprintHelperGraphBodyTarget& Target,
		const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const override;
	virtual FBlueprintHelperGraphBodyReadbackProjection BuildReadbackProjection(
		const FBlueprintHelperGraphBodyTarget& Target,
		const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const override;
};
