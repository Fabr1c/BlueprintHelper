#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPriorityCollisionResolver.h"

namespace BlueprintHelper::GraphLayout
{
struct FGraphLayoutPriorityCollisionRect
{
	FVector2D Min = FVector2D::ZeroVector;
	FVector2D Max = FVector2D::ZeroVector;
};

struct FGraphLayoutPriorityCollisionResolverPrivate
{
	static EGraphLayoutCollisionFamily ResolveFamily(const ENodeRole Role)
	{
		switch (Role)
		{
		case ENodeRole::EventEntry:
		case ENodeRole::ExecNode:
		case ENodeRole::BranchControl:
		case ENodeRole::AsyncNode:
		case ENodeRole::DelegateNode:
			return EGraphLayoutCollisionFamily::Exec;
		case ENodeRole::PureFunction:
		case ENodeRole::OperatorOrCompare:
		case ENodeRole::VariableInput:
			return EGraphLayoutCollisionFamily::Data;
		default:
			return EGraphLayoutCollisionFamily::Other;
		}
	}

	static FGraphLayoutPriorityCollisionRect MakePaddedRect(
		const FGraphLayoutCollisionNode& Node,
		const FVector2D& Position,
		const FRuleSet& RuleSet)
	{
		const FVector2D Padding(
			FMath::Max(0.0f, RuleSet.CollisionPaddingX),
			FMath::Max(0.0f, RuleSet.CollisionPaddingY));
		FGraphLayoutPriorityCollisionRect Rect;
		Rect.Min = Position - Padding;
		Rect.Max = Position + Node.Size + Padding;
		return Rect;
	}

	static float ComputeOverlapRatio(
		const FGraphLayoutCollisionNode& A,
		const FVector2D& APosition,
		const FGraphLayoutCollisionNode& B,
		const FVector2D& BPosition,
		const FRuleSet& RuleSet)
	{
		const FGraphLayoutPriorityCollisionRect ARect = MakePaddedRect(A, APosition, RuleSet);
		const FGraphLayoutPriorityCollisionRect BRect = MakePaddedRect(B, BPosition, RuleSet);
		const float Left = FMath::Max(ARect.Min.X, BRect.Min.X);
		const float Right = FMath::Min(ARect.Max.X, BRect.Max.X);
		const float Top = FMath::Max(ARect.Min.Y, BRect.Min.Y);
		const float Bottom = FMath::Min(ARect.Max.Y, BRect.Max.Y);
		if (Right <= Left || Bottom <= Top)
		{
			return 0.0f;
		}

		const float OverlapArea = (Right - Left) * (Bottom - Top);
		const float MinArea = FMath::Max(1.0f, FMath::Min(
			FMath::Max(1.0f, A.Size.X) * FMath::Max(1.0f, A.Size.Y),
			FMath::Max(1.0f, B.Size.X) * FMath::Max(1.0f, B.Size.Y)));
		return OverlapArea / MinArea;
	}

	static bool IsBlockingPair(
		const FGraphLayoutCollisionNode& A,
		const FVector2D& APosition,
		const FGraphLayoutCollisionNode& B,
		const FVector2D& BPosition,
		const FRuleSet& RuleSet)
	{
		return ComputeOverlapRatio(A, APosition, B, BPosition, RuleSet) >
			FMath::Clamp(RuleSet.OverlapToleranceRatio, 0.0f, 0.5f);
	}

	static int32 ChooseLoserIndex(
		const FGraphLayoutCollisionNode& A,
		const FGraphLayoutCollisionNode& B,
		const int32 AIndex,
		const int32 BIndex)
	{
		if (!A.bMovable && !B.bMovable)
		{
			return INDEX_NONE;
		}
		if (A.bMovable != B.bMovable)
		{
			return A.bMovable ? AIndex : BIndex;
		}
		if (!FMath::IsNearlyEqual(A.Priority, B.Priority))
		{
			return A.Priority < B.Priority ? AIndex : BIndex;
		}
		return A.StableOrder > B.StableOrder ? AIndex : BIndex;
	}

	static bool HasBlockingCollisionWithAny(
		const TArray<FGraphLayoutCollisionNode>& Nodes,
		const TMap<FString, FVector2D>& Positions,
		const int32 CandidateIndex,
		const FVector2D& CandidatePosition,
		const FRuleSet& RuleSet)
	{
		const FGraphLayoutCollisionNode& CandidateNode = Nodes[CandidateIndex];
		for (int32 OtherIndex = 0; OtherIndex < Nodes.Num(); ++OtherIndex)
		{
			if (OtherIndex == CandidateIndex)
			{
				continue;
			}
			const FVector2D OtherPosition = Positions.FindRef(Nodes[OtherIndex].NodeId);
			if (IsBlockingPair(CandidateNode, CandidatePosition, Nodes[OtherIndex], OtherPosition, RuleSet))
			{
				return true;
			}
		}
		return false;
	}

	static TArray<FVector2D> BuildCandidates(
		const FGraphLayoutCollisionNode& Node,
		const FVector2D& CurrentPosition,
		const FRuleSet& RuleSet)
	{
		TArray<FVector2D> Candidates;
		const int32 MaxAttempts = FMath::Max(1, RuleSet.MaxCollisionAttempts);
		const float StepX = FMath::Max(1.0f, FMath::Max(RuleSet.CollisionPaddingX, Node.Size.X));
		const float StepY = FMath::Max(1.0f, RuleSet.CollisionStepY);
		const EGraphLayoutCollisionFamily Family = ResolveFamily(Node.Role);

		if (Family == EGraphLayoutCollisionFamily::Exec)
		{
			for (int32 Attempt = 1; Attempt <= MaxAttempts; ++Attempt)
			{
				Candidates.Add(CurrentPosition + FVector2D(Attempt * StepX, 0.0f));
			}
			for (int32 Attempt = 1; Attempt <= MaxAttempts; ++Attempt)
			{
				Candidates.Add(CurrentPosition + FVector2D(0.0f, Attempt * StepY));
			}
		}
		else if (Family == EGraphLayoutCollisionFamily::Data)
		{
			for (int32 Attempt = 1; Attempt <= MaxAttempts; ++Attempt)
			{
				Candidates.Add(CurrentPosition + FVector2D(-Attempt * StepX, 0.0f));
			}
			for (int32 Attempt = 1; Attempt <= MaxAttempts; ++Attempt)
			{
				Candidates.Add(CurrentPosition + FVector2D(0.0f, Attempt * StepY));
			}
		}
		else
		{
			for (int32 Attempt = 1; Attempt <= MaxAttempts; ++Attempt)
			{
				Candidates.Add(CurrentPosition + FVector2D(0.0f, Attempt * StepY));
			}
		}

		return Candidates;
	}
};

TMap<FString, FVector2D> FGraphLayoutPriorityCollisionResolver::Resolve(
	const TArray<FGraphLayoutCollisionNode>& Nodes,
	const FRuleSet& RuleSet)
{
	TMap<FString, FVector2D> Positions;
	for (const FGraphLayoutCollisionNode& Node : Nodes)
	{
		Positions.Add(Node.NodeId, Node.Position);
	}

	const int32 MaxOuterPasses = FMath::Max(1, RuleSet.MaxCollisionAttempts) * FMath::Max(1, Nodes.Num());
	for (int32 PassIndex = 0; PassIndex < MaxOuterPasses; ++PassIndex)
	{
		bool bMovedAny = false;
		for (int32 LeftIndex = 0; LeftIndex < Nodes.Num(); ++LeftIndex)
		{
			for (int32 RightIndex = LeftIndex + 1; RightIndex < Nodes.Num(); ++RightIndex)
			{
				const FVector2D LeftPosition = Positions.FindRef(Nodes[LeftIndex].NodeId);
				const FVector2D RightPosition = Positions.FindRef(Nodes[RightIndex].NodeId);
				if (!FGraphLayoutPriorityCollisionResolverPrivate::IsBlockingPair(
					Nodes[LeftIndex],
					LeftPosition,
					Nodes[RightIndex],
					RightPosition,
					RuleSet))
				{
					continue;
				}

				const int32 LoserIndex = FGraphLayoutPriorityCollisionResolverPrivate::ChooseLoserIndex(
					Nodes[LeftIndex],
					Nodes[RightIndex],
					LeftIndex,
					RightIndex);
				if (LoserIndex == INDEX_NONE)
				{
					continue;
				}

				const FVector2D LoserPosition = Positions.FindRef(Nodes[LoserIndex].NodeId);
				const TArray<FVector2D> Candidates =
					FGraphLayoutPriorityCollisionResolverPrivate::BuildCandidates(Nodes[LoserIndex], LoserPosition, RuleSet);
				for (const FVector2D& Candidate : Candidates)
				{
					if (!FGraphLayoutPriorityCollisionResolverPrivate::HasBlockingCollisionWithAny(
						Nodes,
						Positions,
						LoserIndex,
						Candidate,
						RuleSet))
					{
						Positions.Add(Nodes[LoserIndex].NodeId, Candidate);
						bMovedAny = true;
						break;
					}
				}
			}
		}
		if (!bMovedAny)
		{
			break;
		}
	}

	return Positions;
}
}
