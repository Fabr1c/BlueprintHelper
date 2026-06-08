#include "Systems/GraphLayout/BlueprintHelperGraphLayoutOccupancyResolver.h"

namespace BlueprintHelper::GraphLayout
{
struct FBlueprintHelperGraphLayoutOccupancyRectMath
{
	static bool RectsOverlap(const FLayoutRect& A, const FLayoutRect& B)
	{
		return A.Min.X < B.Max.X && A.Max.X > B.Min.X && A.Min.Y < B.Max.Y && A.Max.Y > B.Min.Y;
	}

	static FString BuildExistingReservationGroupId(const FNodeSnapshot& Node)
	{
		return FString::Printf(TEXT("existing_node:%s"), *Node.NodeId);
	}
};

FOccupancyResolver::FOccupancyResolver(const FRuleSet& InRuleSet)
	: RuleSet(InRuleSet)
{
}

FLayoutRect FOccupancyResolver::MakeRect(
	const FString& NodeId,
	const FString& LayoutGroupId,
	const FVector2D& Position,
	const FVector2D& Size,
	bool bMovable) const
{
	const FVector2D Padding(
		FMath::Max(0.0f, RuleSet.CollisionPaddingX),
		FMath::Max(0.0f, RuleSet.CollisionPaddingY));

	FLayoutRect Rect;
	Rect.NodeId = NodeId;
	Rect.LayoutGroupId = LayoutGroupId;
	Rect.Min = Position - Padding;
	Rect.Max = Position + Size + Padding;
	Rect.bMovable = bMovable;
	return Rect;
}

bool FOccupancyResolver::OverlapsAny(
	const FLayoutRect& Candidate,
	const bool bIgnoreSameGroupReservations,
	const bool bIgnoreMovableReservations,
	const bool bIgnoreExternalGroupReservations) const
{
	for (const FLayoutRect& Reserved : ReservedRects)
	{
		if (Reserved.NodeId == Candidate.NodeId)
		{
			continue;
		}
		if (bIgnoreMovableReservations && Reserved.bMovable)
		{
			continue;
		}
		if (bIgnoreExternalGroupReservations &&
			!Candidate.LayoutGroupId.IsEmpty() &&
			Candidate.LayoutGroupId != Reserved.LayoutGroupId)
		{
			continue;
		}
		if (bIgnoreSameGroupReservations &&
			!Candidate.LayoutGroupId.IsEmpty() &&
			Candidate.LayoutGroupId == Reserved.LayoutGroupId)
		{
			continue;
		}
		if (FBlueprintHelperGraphLayoutOccupancyRectMath::RectsOverlap(Candidate, Reserved))
		{
			return true;
		}
	}
	return false;
}

void FOccupancyResolver::ReserveExistingNode(const FNodeSnapshot& Node)
{
	ReserveTarget(
		Node.NodeId,
		Node.Position,
		Node.Size,
		false,
		FBlueprintHelperGraphLayoutOccupancyRectMath::BuildExistingReservationGroupId(Node));
}

void FOccupancyResolver::ReserveTarget(
	const FString& NodeId,
	const FVector2D& TargetPosition,
	const FVector2D& Size,
	bool bMovable)
{
	ReserveTarget(NodeId, TargetPosition, Size, bMovable, FString());
}

void FOccupancyResolver::ReserveTarget(
	const FString& NodeId,
	const FVector2D& TargetPosition,
	const FVector2D& Size,
	bool bMovable,
	const FString& LayoutGroupId)
{
	ReservedRects.Add(MakeRect(NodeId, LayoutGroupId, TargetPosition, Size, bMovable));
}

bool FOccupancyResolver::WouldOverlap(
	const FString& NodeId,
	const FVector2D& TargetPosition,
	const FVector2D& Size) const
{
	FResolveTargetRequest Request;
	Request.NodeId = NodeId;
	Request.DesiredPosition = TargetPosition;
	Request.Size = Size;
	return WouldOverlap(Request, TargetPosition);
}

bool FOccupancyResolver::WouldOverlap(
	const FResolveTargetRequest& Request,
	const FVector2D& TargetPosition) const
{
	return OverlapsAny(
		MakeRect(Request.NodeId, Request.LayoutGroupId, TargetPosition, Request.Size, true),
		Request.bIgnoreSameGroupReservations,
		Request.bIgnoreMovableReservations,
		Request.bIgnoreExternalGroupReservations);
}

FVector2D FOccupancyResolver::ResolveTarget(const FResolveTargetRequest& Request) const
{
	return Request.SearchMode == EGraphLayoutCollisionSearchMode::PreferSameRow
		? ResolvePreferSameRow(Request)
		: ResolveDownward(Request);
}

FVector2D FOccupancyResolver::ResolveDownward(const FResolveTargetRequest& Request) const
{
	const int32 MaxAttempts = FMath::Max(1, RuleSet.MaxCollisionAttempts);
	const float StepY = FMath::Max(1.0f, RuleSet.CollisionStepY);

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		const FVector2D Candidate = Request.DesiredPosition + FVector2D(0.0f, Attempt * StepY);
		if (!WouldOverlap(Request, Candidate))
		{
			return Candidate;
		}
	}

	FVector2D EmergencyTarget = Request.DesiredPosition + FVector2D(0.0f, MaxAttempts * StepY);
	while (WouldOverlap(Request, EmergencyTarget))
	{
		EmergencyTarget.Y += StepY;
	}
	return EmergencyTarget;
}

FVector2D FOccupancyResolver::ResolvePreferSameRow(const FResolveTargetRequest& Request) const
{
	const int32 MaxAttempts = FMath::Max(1, RuleSet.MaxCollisionAttempts);
	const float StepX = FMath::Max(1.0f, FMath::Max(RuleSet.CollisionPaddingX, Request.Size.X));
	const float Direction = Request.PreferredHorizontalDirection < 0 ? -1.0f : 1.0f;

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		const FVector2D Candidate = Request.DesiredPosition + FVector2D(Direction * Attempt * StepX, 0.0f);
		if (!WouldOverlap(Request, Candidate))
		{
			return Candidate;
		}
	}

	return ResolveDownward(Request);
}

FVector2D FOccupancyResolver::ResolveNearestFreeTarget(
	const FString& NodeId,
	const FVector2D& DesiredPosition,
	const FVector2D& Size) const
{
	FResolveTargetRequest Request;
	Request.NodeId = NodeId;
	Request.DesiredPosition = DesiredPosition;
	Request.Size = Size;
	Request.SearchMode = EGraphLayoutCollisionSearchMode::DownwardOnly;
	return ResolveTarget(Request);
}

FVector2D FOccupancyResolver::ResolveNearestFreeTargetPreferSameRow(
	const FString& NodeId,
	const FVector2D& DesiredPosition,
	const FVector2D& Size) const
{
	FResolveTargetRequest Request;
	Request.NodeId = NodeId;
	Request.DesiredPosition = DesiredPosition;
	Request.Size = Size;
	Request.SearchMode = EGraphLayoutCollisionSearchMode::PreferSameRow;
	return ResolveTarget(Request);
}
}
