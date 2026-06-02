#include "Systems/GraphLayout/BlueprintHelperGraphLayoutOccupancyResolver.h"

namespace BlueprintHelper::GraphLayout
{
FOccupancyResolver::FOccupancyResolver(const FRuleSet& InRuleSet)
	: RuleSet(InRuleSet)
{
}

FLayoutRect FOccupancyResolver::MakeRect(
	const FString& NodeId,
	const FVector2D& Position,
	const FVector2D& Size,
	bool bMovable) const
{
	const FVector2D Padding(
		FMath::Max(0.0f, RuleSet.CollisionPaddingX),
		FMath::Max(0.0f, RuleSet.CollisionPaddingY));

	FLayoutRect Rect;
	Rect.NodeId = NodeId;
	Rect.Min = Position - Padding;
	Rect.Max = Position + Size + Padding;
	Rect.bMovable = bMovable;
	return Rect;
}

static bool RectsOverlap(const FLayoutRect& A, const FLayoutRect& B)
{
	return A.Min.X < B.Max.X && A.Max.X > B.Min.X && A.Min.Y < B.Max.Y && A.Max.Y > B.Min.Y;
}

bool FOccupancyResolver::OverlapsAny(const FLayoutRect& Candidate) const
{
	for (const FLayoutRect& Reserved : ReservedRects)
	{
		if (Reserved.NodeId == Candidate.NodeId)
		{
			continue;
		}
		if (RectsOverlap(Candidate, Reserved))
		{
			return true;
		}
	}
	return false;
}

void FOccupancyResolver::ReserveExistingNode(const FNodeSnapshot& Node)
{
	ReserveTarget(Node.NodeId, Node.Position, Node.Size, false);
}

void FOccupancyResolver::ReserveTarget(
	const FString& NodeId,
	const FVector2D& TargetPosition,
	const FVector2D& Size,
	bool bMovable)
{
	ReservedRects.Add(MakeRect(NodeId, TargetPosition, Size, bMovable));
}

bool FOccupancyResolver::WouldOverlap(
	const FString& NodeId,
	const FVector2D& TargetPosition,
	const FVector2D& Size) const
{
	return OverlapsAny(MakeRect(NodeId, TargetPosition, Size, true));
}

FVector2D FOccupancyResolver::ResolveNearestFreeTarget(
	const FString& NodeId,
	const FVector2D& DesiredPosition,
	const FVector2D& Size) const
{
	const int32 MaxAttempts = FMath::Max(1, RuleSet.MaxCollisionAttempts);
	const float StepY = FMath::Max(1.0f, RuleSet.CollisionStepY);

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		const FVector2D Candidate = DesiredPosition + FVector2D(0.0f, Attempt * StepY);
		if (!WouldOverlap(NodeId, Candidate, Size))
		{
			return Candidate;
		}
	}

	FVector2D EmergencyTarget = DesiredPosition + FVector2D(0.0f, MaxAttempts * StepY);
	while (WouldOverlap(NodeId, EmergencyTarget, Size))
	{
		EmergencyTarget.Y += StepY;
	}
	return EmergencyTarget;
}

FVector2D FOccupancyResolver::ResolveNearestFreeTargetPreferSameRow(
	const FString& NodeId,
	const FVector2D& DesiredPosition,
	const FVector2D& Size) const
{
	const int32 MaxAttempts = FMath::Max(1, RuleSet.MaxCollisionAttempts);
	const float StepX = FMath::Max(1.0f, FMath::Max(RuleSet.CollisionPaddingX, Size.X));

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		const FVector2D Candidate = DesiredPosition + FVector2D(Attempt * StepX, 0.0f);
		if (!WouldOverlap(NodeId, Candidate, Size))
		{
			return Candidate;
		}
	}

	return ResolveNearestFreeTarget(NodeId, DesiredPosition, Size);
}
}
