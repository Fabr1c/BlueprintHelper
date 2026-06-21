#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTopology.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

namespace BlueprintHelper::GraphLayout
{
enum class EGraphLayoutNodeMobility : uint8
{
	MovableGenerated,
	MovableExisting,
	Anchor,
	Obstacle
};

enum class EGraphLayoutComponentKind : uint8
{
	FullArrange,
	PartialAnchored,
	FixedOnly
};

struct FGraphLayoutArrangeScope
{
	TSet<FString> AllNodeIds;
	TSet<FString> ToArrangeNodeIds;
	TSet<FString> AnchorNodeIds;
	TSet<FString> ObstacleNodeIds;
	TMap<FString, EGraphLayoutNodeMobility> MobilityByNodeId;
};

class BLUEPRINTHELPER_API FGraphLayoutArrangeScopePolicy
{
public:
	static FGraphLayoutArrangeScope Build(
		const FGraphSnapshot& Snapshot,
		const FGraphTopology& Topology,
		const FRuleSet& RuleSet);
};
}
