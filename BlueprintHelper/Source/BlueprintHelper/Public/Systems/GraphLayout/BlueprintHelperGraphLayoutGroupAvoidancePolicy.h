#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

namespace BlueprintHelper::GraphLayout
{
struct FGraphLayoutGroupNode
{
	FString NodeId;
	FString LayoutGroupId;
	int32 LayoutGroupOrder = INDEX_NONE;
	int32 NodeOrder = INDEX_NONE;
	FVector2D TargetPosition = FVector2D::ZeroVector;
	FVector2D Size = FVector2D(180.0f, 80.0f);
	bool bGenerated = true;
};

struct FGraphLayoutGroupOffset
{
	FString LayoutGroupId;
	FVector2D Offset = FVector2D::ZeroVector;
	FString Reason;
};

class BLUEPRINTHELPER_API FGraphLayoutGroupAvoidancePolicy
{
public:
	static TArray<FGraphLayoutGroupOffset> ResolveGroupOffsets(
		const TArray<FGraphLayoutGroupNode>& Nodes,
		const FRuleSet& RuleSet);
};
}
