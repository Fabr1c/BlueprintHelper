#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyBoundaryModel.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyReadbackProjection.h"

class IBlueprintHelperGraphBodyAdapter;
struct FBlueprintHelperGraphBodyRequest;
struct FBlueprintHelperGraphBodyTarget;

class BLUEPRINTHELPER_API FBlueprintHelperGraphBodyReadbackService
{
public:
	bool BuildAdapterBoundaryForTarget(
		const FBlueprintHelperTargetRef& Target,
		TSharedPtr<FJsonObject>& OutAdapterBoundaryJson,
		FString& OutError) const;

	TSharedRef<FJsonObject> BuildAdapterBoundaryJson(
		const FBlueprintHelperGraphBodyBoundaryModel& Boundary,
		const FBlueprintHelperGraphBodyReadbackProjection& Projection) const;

private:
	bool TryBuildAdapterBoundary(
		const IBlueprintHelperGraphBodyAdapter& Adapter,
		const FBlueprintHelperGraphBodyRequest& Request,
		TSharedPtr<FJsonObject>& OutAdapterBoundaryJson,
		FString& OutError) const;
};
