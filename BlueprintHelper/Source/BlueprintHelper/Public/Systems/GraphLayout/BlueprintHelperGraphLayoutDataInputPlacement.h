#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

namespace BlueprintHelper::GraphLayout
{
struct FDataInputPlacementRequest
{
	FString ConsumerNodeId;
	FString SourceNodeId;
	ENodeRole SourceRole = ENodeRole::Unknown;
	int32 InputOrder = 0;
	FVector2D ConsumerTarget = FVector2D::ZeroVector;
	FVector2D SourceSize = FVector2D(180.0f, 80.0f);
};

class BLUEPRINTHELPER_API FDataInputPlacement
{
public:
	static bool IsDataInputRole(ENodeRole Role);
	static FVector2D BuildDesiredTarget(const FRuleSet& RuleSet, const FDataInputPlacementRequest& Request);
	static const TCHAR* GetReason(ENodeRole Role);
};
}
