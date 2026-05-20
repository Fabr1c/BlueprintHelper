#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/PostOperations/BlueprintHelperTaskRuntimePostOperationTypes.h"

class FJsonObject;

class FBlueprintHelperTaskRuntimePostOperationPlanner
{
public:
	static FBlueprintHelperTaskRuntimePostOperationPlan BuildPlan(
		const TSharedPtr<FJsonObject>& TaskPlan,
		bool bDryRun);

	static FString NormalizeAssetPath(const FString& AssetPath);
	static TArray<FString> ReadUniqueTargetAssets(const TSharedPtr<FJsonObject>& TaskPlan);
};
