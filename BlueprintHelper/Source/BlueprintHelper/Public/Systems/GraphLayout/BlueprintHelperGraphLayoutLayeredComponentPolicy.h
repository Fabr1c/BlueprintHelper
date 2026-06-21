#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutArrangeScopePolicy.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTopology.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

namespace BlueprintHelper::GraphLayout
{
struct FGraphLayoutLayeredPlacement
{
	FString NodeId;
	FVector2D TargetPosition = FVector2D::ZeroVector;
	FString Reason;
};

struct FGraphLayoutLayeredResult
{
	TArray<FGraphLayoutLayeredPlacement> Placements;
	TArray<FString> Issues;
};

class BLUEPRINTHELPER_API FGraphLayoutLayeredComponentPolicy
{
public:
	static FGraphLayoutLayeredResult Layout(
		const FGraphSnapshot& Snapshot,
		const FGraphTopology& Topology,
		const FGraphLayoutArrangeScope& Scope,
		const FRuleSet& RuleSet);
};
}
