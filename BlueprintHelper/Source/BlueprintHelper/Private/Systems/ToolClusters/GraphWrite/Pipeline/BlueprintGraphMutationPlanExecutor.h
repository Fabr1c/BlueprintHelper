#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteResultTypes.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlan.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphWriteContext.h"

class FBlueprintGraphMutationPlanExecutor
{
public:
	static FBlueprintGenerateResult Execute(
		FBlueprintGraphWriteContext& Context,
		const FBlueprintGraphMutationPlan& Plan);

private:
	static void SpawnNodes(
		FBlueprintGraphWriteContext& Context,
		const FBlueprintGraphMutationPlan& Plan,
		FBlueprintGenerateResult& Result);
	static void ApplyDefaults(
		FBlueprintGraphWriteContext& Context,
		const FBlueprintGraphMutationPlan& Plan,
		FBlueprintGenerateResult& Result);
	static void ConnectLinks(
		FBlueprintGraphWriteContext& Context,
		const FBlueprintGraphMutationPlan& Plan,
		FBlueprintGenerateResult& Result);
};
