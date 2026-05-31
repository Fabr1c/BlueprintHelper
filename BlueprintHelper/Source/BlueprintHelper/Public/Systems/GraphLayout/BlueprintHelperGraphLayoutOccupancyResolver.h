#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

namespace BlueprintHelper::GraphLayout
{
struct FLayoutRect
{
	FString NodeId;
	FVector2D Min = FVector2D::ZeroVector;
	FVector2D Max = FVector2D::ZeroVector;
	bool bMovable = false;
};

class BLUEPRINTHELPER_API FOccupancyResolver
{
public:
	explicit FOccupancyResolver(const FRuleSet& InRuleSet);

	void ReserveExistingNode(const FNodeSnapshot& Node);
	void ReserveTarget(const FString& NodeId, const FVector2D& TargetPosition, const FVector2D& Size, bool bMovable);
	FVector2D ResolveNearestFreeTarget(const FString& NodeId, const FVector2D& DesiredPosition, const FVector2D& Size) const;
	bool WouldOverlap(const FString& NodeId, const FVector2D& TargetPosition, const FVector2D& Size) const;

private:
	FLayoutRect MakeRect(const FString& NodeId, const FVector2D& Position, const FVector2D& Size, bool bMovable) const;
	bool OverlapsAny(const FLayoutRect& Candidate) const;

	const FRuleSet& RuleSet;
	TArray<FLayoutRect> ReservedRects;
};
}
