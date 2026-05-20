#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlan.h"

class FJsonObject;

class FBlueprintGraphMutationPlanBuilder
{
public:
	static bool BuildFromGraphJson(
		const TSharedPtr<FJsonObject>& GraphJsonObject,
		FBlueprintGraphMutationPlan& OutPlan,
		TArray<FBlueprintGeneratorDiagnostic>& OutDiagnostics);

	static FBlueprintGraphMutationNodePlan MakeNodePlanFromParsedNode(const FParsedNode& ParsedNode);
	static FBlueprintGraphMutationLinkPlan MakeLinkPlanFromParsedLink(const FParsedLink& ParsedLink);
};
