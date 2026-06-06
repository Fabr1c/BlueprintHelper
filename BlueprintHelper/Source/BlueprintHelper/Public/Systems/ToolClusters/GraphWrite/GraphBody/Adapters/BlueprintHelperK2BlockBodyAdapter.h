#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyAdapter.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyAdapterRegistry.h"

class BLUEPRINTHELPER_API FBlueprintHelperK2BlockBodyAdapter : public IBlueprintHelperGraphBodyAdapter
{
public:
	FBlueprintHelperK2BlockBodyAdapter();
	explicit FBlueprintHelperK2BlockBodyAdapter(
		const FBlueprintHelperGraphBodyAdapterDescriptor& InDescriptor);

	virtual FString GetAdapterId() const override;
	virtual FBlueprintHelperGraphBodyBoundaryModel BuildBoundaryModel(
		const TSharedRef<FJsonObject>& Payload) const override;
	virtual FBlueprintHelperGraphConnectivityPolicy SelectConnectivityPolicy(
		const FBlueprintHelperGraphBodyBoundaryModel& Boundary) const override;
	virtual FBlueprintHelperGraphBodyMutationPlan BuildMutationPlan(
		const TSharedRef<FJsonObject>& Payload) const override;

private:
	FBlueprintHelperGraphBodyAdapterDescriptor Descriptor;
};
