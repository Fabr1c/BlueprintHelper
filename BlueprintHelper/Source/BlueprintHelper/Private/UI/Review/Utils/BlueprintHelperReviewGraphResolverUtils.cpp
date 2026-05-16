// Review graph resolver utility helpers implementation.

#include "UI/Review/Utils/BlueprintHelperReviewGraphResolverUtils.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"

UEdGraph* FBlueprintHelperReviewGraphResolverUtils::FindGraphByName(
	const UBlueprint* Blueprint,
	const FString& GraphName)
{
	if (!Blueprint || GraphName.IsEmpty())
	{
		return nullptr;
	}

	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetName() == GraphName)
		{
			return Graph;
		}
	}
	return nullptr;
}
