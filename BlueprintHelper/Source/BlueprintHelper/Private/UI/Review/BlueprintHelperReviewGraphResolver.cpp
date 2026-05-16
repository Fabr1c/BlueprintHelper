#include "UI/Review/BlueprintHelperReviewGraphResolver.h"
#include "UI/Review/Utils/BlueprintHelperReviewGraphResolverUtils.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"

UEdGraph* FBlueprintHelperReviewGraphResolver::ResolveGraphForReviewSelection(
	const UBlueprint* Blueprint,
	const FString& RequestedGraphName)
{
	if (!Blueprint)
	{
		return nullptr;
	}

	if (!RequestedGraphName.IsEmpty())
	{
		return FBlueprintHelperReviewGraphResolverUtils::FindGraphByName(Blueprint, RequestedGraphName);
	}

	if (Blueprint->UbergraphPages.Num() > 0)
	{
		return Blueprint->UbergraphPages[0];
	}

	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph)
		{
			return Graph;
		}
	}

	return nullptr;
}
