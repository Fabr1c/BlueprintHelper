#include "UI/Review/BlueprintHelperReviewGraphResolver.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"

class FBlueprintHelperReviewGraphResolverLocalUtils
{
public:
	static UEdGraph* FindGraphByName(const UBlueprint* Blueprint, const FString& GraphName)
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

};

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
		return FBlueprintHelperReviewGraphResolverLocalUtils::FindGraphByName(Blueprint, RequestedGraphName);
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
