#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTypes.h"

namespace BlueprintHelper::GraphLayout
{
struct FLayoutRect
{
	FString NodeId;
	FString LayoutGroupId;
	FVector2D Min = FVector2D::ZeroVector;
	FVector2D Max = FVector2D::ZeroVector;
	bool bMovable = false;
};

enum class EGraphLayoutCollisionSearchMode : uint8
{
	DownwardOnly,
	PreferSameRow
};

struct FResolveTargetRequest
{
	FString NodeId;
	FString LayoutGroupId;
	FVector2D DesiredPosition = FVector2D::ZeroVector;
	FVector2D Size = FVector2D(180.0f, 80.0f);
	EGraphLayoutCollisionSearchMode SearchMode = EGraphLayoutCollisionSearchMode::DownwardOnly;
	int32 PreferredHorizontalDirection = 1;
	bool bIgnoreSameGroupReservations = false;
	bool bIgnoreMovableReservations = false;
	bool bIgnoreExternalGroupReservations = false;
};

class BLUEPRINTHELPER_API FOccupancyResolver
{
public:
	explicit FOccupancyResolver(const FRuleSet& InRuleSet);

	void ReserveExistingNode(const FNodeSnapshot& Node);
	void ReserveTarget(const FString& NodeId, const FVector2D& TargetPosition, const FVector2D& Size, bool bMovable);
	void ReserveTarget(
		const FString& NodeId,
		const FVector2D& TargetPosition,
		const FVector2D& Size,
		bool bMovable,
		const FString& LayoutGroupId);
	FVector2D ResolveTarget(const FResolveTargetRequest& Request) const;
	FVector2D ResolveNearestFreeTarget(const FString& NodeId, const FVector2D& DesiredPosition, const FVector2D& Size) const;
	FVector2D ResolveNearestFreeTargetPreferSameRow(
		const FString& NodeId,
		const FVector2D& DesiredPosition,
		const FVector2D& Size) const;
	bool WouldOverlap(const FString& NodeId, const FVector2D& TargetPosition, const FVector2D& Size) const;

private:
	FLayoutRect MakeRect(
		const FString& NodeId,
		const FString& LayoutGroupId,
		const FVector2D& Position,
		const FVector2D& Size,
		bool bMovable) const;
	bool OverlapsAny(
		const FLayoutRect& Candidate,
		bool bIgnoreSameGroupReservations,
		bool bIgnoreMovableReservations,
		bool bIgnoreExternalGroupReservations) const;
	bool WouldOverlap(
		const FResolveTargetRequest& Request,
		const FVector2D& TargetPosition) const;
	FVector2D ResolveDownward(const FResolveTargetRequest& Request) const;
	FVector2D ResolvePreferSameRow(const FResolveTargetRequest& Request) const;

	const FRuleSet& RuleSet;
	TArray<FLayoutRect> ReservedRects;
};
}
