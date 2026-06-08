#include "Systems/GraphLayout/BlueprintHelperGraphLayoutGroupAvoidancePolicy.h"

namespace BlueprintHelper::GraphLayout
{
struct FGraphLayoutGroupEnvelope
{
	FString LayoutGroupId;
	int32 LayoutGroupOrder = INDEX_NONE;
	int32 FirstNodeOrder = INDEX_NONE;
	FVector2D Min = FVector2D::ZeroVector;
	FVector2D Max = FVector2D::ZeroVector;
	bool bGenerated = true;
	bool bInitialized = false;

	FVector2D Size() const
	{
		return Max - Min;
	}

	FGraphLayoutGroupEnvelope OffsetBy(const FVector2D& Offset) const
	{
		FGraphLayoutGroupEnvelope Result = *this;
		Result.Min += Offset;
		Result.Max += Offset;
		return Result;
	}
};

struct FGraphLayoutGroupEnvelopeMath
{
	static FString ResolveEffectiveGroupId(const FGraphLayoutGroupNode& Node)
	{
		if (!Node.LayoutGroupId.IsEmpty())
		{
			return Node.LayoutGroupId;
		}
		return Node.bGenerated
			? FString::Printf(TEXT("generated_node:%s"), *Node.NodeId)
			: FString::Printf(TEXT("existing_node:%s"), *Node.NodeId);
	}

	static bool Overlaps(const FGraphLayoutGroupEnvelope& A, const FGraphLayoutGroupEnvelope& B)
	{
		return A.Min.X < B.Max.X && A.Max.X > B.Min.X && A.Min.Y < B.Max.Y && A.Max.Y > B.Min.Y;
	}

	static void IncludeNode(
		FGraphLayoutGroupEnvelope& Envelope,
		const FGraphLayoutGroupNode& Node,
		const FVector2D& Padding)
	{
		const FVector2D NodeMin = Node.TargetPosition - Padding;
		const FVector2D NodeMax = Node.TargetPosition + Node.Size + Padding;
		if (!Envelope.bInitialized)
		{
			Envelope.Min = NodeMin;
			Envelope.Max = NodeMax;
			Envelope.LayoutGroupOrder = Node.LayoutGroupOrder;
			Envelope.FirstNodeOrder = Node.NodeOrder;
			Envelope.bGenerated = Node.bGenerated;
			Envelope.bInitialized = true;
			return;
		}

		Envelope.Min.X = FMath::Min(Envelope.Min.X, NodeMin.X);
		Envelope.Min.Y = FMath::Min(Envelope.Min.Y, NodeMin.Y);
		Envelope.Max.X = FMath::Max(Envelope.Max.X, NodeMax.X);
		Envelope.Max.Y = FMath::Max(Envelope.Max.Y, NodeMax.Y);
		if (Envelope.LayoutGroupOrder == INDEX_NONE ||
			(Node.LayoutGroupOrder != INDEX_NONE && Node.LayoutGroupOrder < Envelope.LayoutGroupOrder))
		{
			Envelope.LayoutGroupOrder = Node.LayoutGroupOrder;
		}
		if (Envelope.FirstNodeOrder == INDEX_NONE ||
			(Node.NodeOrder != INDEX_NONE && Node.NodeOrder < Envelope.FirstNodeOrder))
		{
			Envelope.FirstNodeOrder = Node.NodeOrder;
		}
		Envelope.bGenerated = Envelope.bGenerated || Node.bGenerated;
	}
};

TArray<FGraphLayoutGroupOffset> FGraphLayoutGroupAvoidancePolicy::ResolveGroupOffsets(
	const TArray<FGraphLayoutGroupNode>& Nodes,
	const FRuleSet& RuleSet)
{
	TMap<FString, FGraphLayoutGroupEnvelope> EnvelopesByGroupId;
	const FVector2D Padding(
		FMath::Max(0.0f, RuleSet.CollisionPaddingX),
		FMath::Max(0.0f, RuleSet.CollisionPaddingY));

	for (const FGraphLayoutGroupNode& Node : Nodes)
	{
		const FString GroupId = FGraphLayoutGroupEnvelopeMath::ResolveEffectiveGroupId(Node);
		FGraphLayoutGroupEnvelope& Envelope = EnvelopesByGroupId.FindOrAdd(GroupId);
		Envelope.LayoutGroupId = GroupId;
		FGraphLayoutGroupEnvelopeMath::IncludeNode(Envelope, Node, Padding);
	}

	TArray<FGraphLayoutGroupEnvelope> GeneratedGroups;
	TArray<FGraphLayoutGroupEnvelope> AcceptedBlockers;
	for (const TPair<FString, FGraphLayoutGroupEnvelope>& Pair : EnvelopesByGroupId)
	{
		if (!Pair.Value.bInitialized)
		{
			continue;
		}
		if (Pair.Value.bGenerated)
		{
			GeneratedGroups.Add(Pair.Value);
		}
		else
		{
			AcceptedBlockers.Add(Pair.Value);
		}
	}

	GeneratedGroups.Sort([](const FGraphLayoutGroupEnvelope& Left, const FGraphLayoutGroupEnvelope& Right)
	{
		if (Left.LayoutGroupOrder != Right.LayoutGroupOrder)
		{
			if (Left.LayoutGroupOrder != INDEX_NONE && Right.LayoutGroupOrder != INDEX_NONE)
			{
				return Left.LayoutGroupOrder < Right.LayoutGroupOrder;
			}
			return Left.LayoutGroupOrder != INDEX_NONE;
		}
		if (Left.FirstNodeOrder != Right.FirstNodeOrder)
		{
			if (Left.FirstNodeOrder != INDEX_NONE && Right.FirstNodeOrder != INDEX_NONE)
			{
				return Left.FirstNodeOrder < Right.FirstNodeOrder;
			}
			return Left.FirstNodeOrder != INDEX_NONE;
		}
		return Left.LayoutGroupId < Right.LayoutGroupId;
	});

	TArray<FGraphLayoutGroupOffset> Offsets;
	const float StepY = FMath::Max(1.0f, RuleSet.CollisionStepY);
	const int32 MaxAttempts = FMath::Max(1, RuleSet.MaxCollisionAttempts);

	for (const FGraphLayoutGroupEnvelope& Group : GeneratedGroups)
	{
		FVector2D Offset = FVector2D::ZeroVector;
		FGraphLayoutGroupEnvelope Candidate = Group;
		bool bMoved = false;

		int32 Attempt = 0;
		while (true)
		{
			bool bOverlaps = false;
			float RequiredDeltaY = 0.0f;
			for (const FGraphLayoutGroupEnvelope& Blocker : AcceptedBlockers)
			{
				if (!FGraphLayoutGroupEnvelopeMath::Overlaps(Candidate, Blocker))
				{
					continue;
				}

				bOverlaps = true;
				RequiredDeltaY = FMath::Max(RequiredDeltaY, (Blocker.Max.Y - Candidate.Min.Y) + StepY);
			}

			if (!bOverlaps)
			{
				break;
			}

			const float StepDeltaY = Attempt < MaxAttempts ? StepY : RequiredDeltaY;
			Offset.Y += FMath::Max(StepY, StepDeltaY);
			Candidate = Group.OffsetBy(Offset);
			bMoved = true;
			++Attempt;
		}

		if (bMoved)
		{
			FGraphLayoutGroupOffset& GroupOffset = Offsets.AddDefaulted_GetRef();
			GroupOffset.LayoutGroupId = Group.LayoutGroupId;
			GroupOffset.Offset = Offset;
			GroupOffset.Reason = TEXT("group_avoided_overlap");
		}

		AcceptedBlockers.Add(Candidate);
	}

	return Offsets;
}
}
