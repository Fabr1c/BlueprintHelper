#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTopology.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

namespace BlueprintHelper::GraphLayout
{
enum class EPureDataNodeKind : uint8
{
	None,
	DataLeaf,
	DataTransform,
	DataSink
};

struct FPureDataSubgraphEnvelope
{
	FString SinkNodeId;
	FString SinkPinId;
	FString RootNodeId;
	TArray<FString> NodeIds;
	TMap<FString, FVector2D> RelativeTargets;
	FVector2D Size = FVector2D::ZeroVector;
};

class BLUEPRINTHELPER_API FPureDataSubgraphPolicy
{
public:
	static EPureDataNodeKind ClassifyNode(const FNodeSnapshot& Node);
	static FPureDataSubgraphEnvelope MeasureForRoot(
		const FGraphSnapshot& Snapshot,
		const FGraphTopology& Topology,
		const FString& SinkNodeId,
		const FString& SinkPinId,
		const FString& RootNodeId,
		const FRuleSet& RuleSet);
	static FPureDataSubgraphEnvelope MeasureForSink(
		const FGraphSnapshot& Snapshot,
		const FGraphTopology& Topology,
		const FString& SinkNodeId,
		const FString& SinkPinId,
		const FRuleSet& RuleSet);
};
}
