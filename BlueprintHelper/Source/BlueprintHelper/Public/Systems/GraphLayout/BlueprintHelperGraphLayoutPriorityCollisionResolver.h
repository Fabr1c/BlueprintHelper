#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

namespace BlueprintHelper::GraphLayout
{
enum class EGraphLayoutCollisionFamily : uint8
{
	Exec,
	Data,
	Other
};

struct FGraphLayoutCollisionNode
{
	FString NodeId;
	FString LayoutGroupId;
	ENodeRole Role = ENodeRole::Unknown;
	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Size = FVector2D(180.0f, 80.0f);
	float Priority = 0.0f;
	bool bMovable = false;
	int32 StableOrder = 0;
};

class BLUEPRINTHELPER_API FGraphLayoutPriorityCollisionResolver
{
public:
	static TMap<FString, FVector2D> Resolve(
		const TArray<FGraphLayoutCollisionNode>& Nodes,
		const FRuleSet& RuleSet);
};
}
