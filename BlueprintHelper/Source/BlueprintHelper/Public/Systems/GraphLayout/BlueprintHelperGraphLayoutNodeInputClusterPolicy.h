#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPureDataSubgraphPolicy.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTopology.h"

namespace BlueprintHelper::GraphLayout
{
struct FNodeInputClusterBudget
{
	FString ConsumerNodeId;
	TArray<FString> NodeIds;
	TMap<FString, FVector2D> RelativeTargets;
	TMap<FString, int32> DataDepthByNodeId;
	TMap<FString, float> LayoutPriorityByNodeId;
	TMap<FString, EPureDataNodeKind> KindByNodeId;
	float Width = 0.0f;
	float Height = 0.0f;
};

class BLUEPRINTHELPER_API FNodeInputClusterPolicy
{
public:
	static FNodeInputClusterBudget MeasureForConsumer(
		const FGraphSnapshot& Snapshot,
		const FGraphTopology& Topology,
		const FString& ConsumerNodeId,
		const FRuleSet& RuleSet);
};
}
