#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyAdapter.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyAdapterRegistry.h"

class BLUEPRINTHELPER_API FBlueprintHelperK2BlockBodyAdapter : public IBlueprintHelperGraphBodyAdapter
{
public:
	FBlueprintHelperK2BlockBodyAdapter();
	explicit FBlueprintHelperK2BlockBodyAdapter(
		const FBlueprintHelperGraphBodyAdapterDescriptor& InDescriptor);

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

	FBlueprintHelperGraphBodyBoundaryModel BuildBoundaryModel(
		const TSharedRef<FJsonObject>& Payload) const;
	FBlueprintHelperGraphConnectivityPolicy SelectConnectivityPolicy(
		const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const;
	FBlueprintHelperGraphBodyMutationPlan BuildMutationPlan(
		const TSharedRef<FJsonObject>& Payload) const;

private:
	FBlueprintHelperGraphBodyAdapterDescriptor Descriptor;
};
