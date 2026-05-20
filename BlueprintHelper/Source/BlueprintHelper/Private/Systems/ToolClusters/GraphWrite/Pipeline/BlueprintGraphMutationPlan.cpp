#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphMutationPlan.h"

bool FBlueprintGraphMutationPlan::IsValid() const
{
	return !GraphName.IsEmpty() && Nodes.Num() > 0;
}

int32 FBlueprintGraphMutationPlan::CountRequestedNodes() const
{
	return Nodes.Num();
}

int32 FBlueprintGraphMutationPlan::CountRequestedDefaultValues() const
{
	int32 Count = 0;
	for (const FBlueprintGraphMutationNodePlan& Node : Nodes)
	{
		Count += Node.DefaultValues.Num();
	}
	return Count;
}

int32 FBlueprintGraphMutationPlan::CountRequestedLinks() const
{
	return Links.Num();
}
