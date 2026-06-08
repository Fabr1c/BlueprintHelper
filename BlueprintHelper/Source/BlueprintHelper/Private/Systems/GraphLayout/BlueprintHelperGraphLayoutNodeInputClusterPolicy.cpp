#include "Systems/GraphLayout/BlueprintHelperGraphLayoutNodeInputClusterPolicy.h"

#include "Systems/GraphLayout/BlueprintHelperGraphLayoutClassifier.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutDataInputPlacement.h"

namespace BlueprintHelper::GraphLayout
{
struct FBlueprintHelperNodeInputClusterBudgetHelpers
{
	struct FMeasuredBounds
	{
		FVector2D Min = FVector2D::ZeroVector;
		FVector2D Max = FVector2D::ZeroVector;
	};

static void AddBudgetTarget(
	FNodeInputClusterBudget& Budget,
	const FString& NodeId,
	const FVector2D& RelativeTarget,
	const EPureDataNodeKind Kind = EPureDataNodeKind::None,
	const int32 DataDepth = INDEX_NONE,
	const float LayoutPriority = 0.0f)
{
	if (Budget.RelativeTargets.Contains(NodeId))
	{
		return;
	}

	Budget.NodeIds.Add(NodeId);
	Budget.RelativeTargets.Add(NodeId, RelativeTarget);
	if (Kind != EPureDataNodeKind::None)
	{
		Budget.KindByNodeId.Add(NodeId, Kind);
	}
	if (DataDepth != INDEX_NONE)
	{
		Budget.DataDepthByNodeId.Add(NodeId, DataDepth);
	}
	Budget.LayoutPriorityByNodeId.Add(NodeId, LayoutPriority);
}

static bool TryAddBudgetTarget(
	FNodeInputClusterBudget& Budget,
	const FString& NodeId,
	const FVector2D& RelativeTarget,
	const EPureDataNodeKind Kind = EPureDataNodeKind::None,
	const int32 DataDepth = INDEX_NONE,
	const float LayoutPriority = 0.0f)
{
	if (Budget.RelativeTargets.Contains(NodeId))
	{
		return false;
	}

	AddBudgetTarget(Budget, NodeId, RelativeTarget, Kind, DataDepth, LayoutPriority);
	return true;
}

static int32 ResolvePlacementOrder(const int32 SourceOrder, const ENodeRole SourceRole, const FRuleSet& RuleSet)
{
	if (SourceRole == ENodeRole::VariableInput && !RuleSet.bUseTargetPinOrderForVariableInputs)
	{
		return 0;
	}

	return SourceOrder;
}

static FVector2D BuildLeafRelativeTarget(
	const FRuleSet& RuleSet,
	const FString& ConsumerNodeId,
	const FString& SourceNodeId,
	const ENodeRole SourceRole,
	const int32 SourceOrder)
{
	FDataInputPlacementRequest Request;
	Request.ConsumerNodeId = ConsumerNodeId;
	Request.SourceNodeId = SourceNodeId;
	Request.SourceRole = SourceRole;
	Request.InputOrder = ResolvePlacementOrder(SourceOrder, SourceRole, RuleSet);
	Request.ConsumerTarget = FVector2D::ZeroVector;
	return FDataInputPlacement::BuildDesiredTarget(RuleSet, Request);
}

static void UpdateBudgetSize(
	FNodeInputClusterBudget& Budget,
	const FGraphTopology& Topology,
	const TArray<FMeasuredBounds>& SupplementalBounds,
	const FRuleSet& RuleSet)
{
	if (Budget.RelativeTargets.IsEmpty())
	{
		Budget.Width = 0.0f;
		Budget.Height = 0.0f;
		return;
	}

	FVector2D Min(TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
	FVector2D Max(-TNumericLimits<float>::Max(), -TNumericLimits<float>::Max());
	bool bHasBounds = false;

	for (const TPair<FString, FVector2D>& Pair : Budget.RelativeTargets)
	{
		const FNodeSnapshot* Node = Topology.FindNode(Pair.Key);
		if (!Node)
		{
			continue;
		}

		const FVector2D NodeMin = Pair.Value;
		const FVector2D NodeMax = Pair.Value + Node->Size;
		Min.X = FMath::Min(Min.X, NodeMin.X);
		Min.Y = FMath::Min(Min.Y, NodeMin.Y);
		Max.X = FMath::Max(Max.X, NodeMax.X);
		Max.Y = FMath::Max(Max.Y, NodeMax.Y);
		bHasBounds = true;
	}

	for (const FMeasuredBounds& Bounds : SupplementalBounds)
	{
		Min.X = FMath::Min(Min.X, Bounds.Min.X);
		Min.Y = FMath::Min(Min.Y, Bounds.Min.Y);
		Max.X = FMath::Max(Max.X, Bounds.Max.X);
		Max.Y = FMath::Max(Max.Y, Bounds.Max.Y);
		bHasBounds = true;
	}

	if (!bHasBounds)
	{
		Budget.Width = 0.0f;
		Budget.Height = 0.0f;
		return;
	}

	Budget.Width = (Max.X - Min.X) + RuleSet.DataClusterPaddingX;
	Budget.Height = (Max.Y - Min.Y) + RuleSet.DataClusterPaddingY;
}

static TOptional<FMeasuredBounds> MeasureEnvelopeBounds(
	const TArray<FString>& ClaimedNodeIds,
	const FPureDataSubgraphEnvelope& Envelope,
	const FGraphTopology& Topology,
	const FVector2D& RootTarget,
	const FRuleSet& RuleSet)
{
	FVector2D LocalMin(TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
	FVector2D LocalMax(-TNumericLimits<float>::Max(), -TNumericLimits<float>::Max());
	bool bHasBounds = false;

	for (const FString& NodeId : ClaimedNodeIds)
	{
		const FVector2D* RelativeTarget = Envelope.RelativeTargets.Find(NodeId);
		if (!RelativeTarget)
		{
			continue;
		}

		const FNodeSnapshot* Node = Topology.FindNode(NodeId);
		if (!Node)
		{
			continue;
		}

		LocalMin.X = FMath::Min(LocalMin.X, RelativeTarget->X);
		LocalMin.Y = FMath::Min(LocalMin.Y, RelativeTarget->Y);
		LocalMax.X = FMath::Max(LocalMax.X, RelativeTarget->X + Node->Size.X);
		LocalMax.Y = FMath::Max(LocalMax.Y, RelativeTarget->Y + Node->Size.Y);
		bHasBounds = true;
	}

	if (!bHasBounds)
	{
		return {};
	}

	FMeasuredBounds Bounds;
	Bounds.Min = RootTarget + LocalMin;
	Bounds.Max = RootTarget + LocalMax + FVector2D(RuleSet.DataClusterPaddingX, RuleSet.DataClusterPaddingY);
	return Bounds;
}
};

FNodeInputClusterBudget FNodeInputClusterPolicy::MeasureForConsumer(
	const FGraphSnapshot& Snapshot,
	const FGraphTopology& Topology,
	const FString& ConsumerNodeId,
	const FRuleSet& RuleSet)
{
	FNodeInputClusterBudget Budget;
	Budget.ConsumerNodeId = ConsumerNodeId;

	const FNodeSnapshot* ConsumerNode = Topology.FindNode(ConsumerNodeId);
	if (!ConsumerNode)
	{
		return Budget;
	}

	TArray<FBlueprintHelperNodeInputClusterBudgetHelpers::FMeasuredBounds> SupplementalBounds;
	int32 SourceOrder = 0;
	for (const FPinSnapshot& Pin : ConsumerNode->Pins)
	{
		if (Pin.Direction != EPinDirection::Input || Pin.bExec)
		{
			continue;
		}

		for (const FString& LinkedNodeId : Pin.LinkedNodeIds)
		{
			const FNodeSnapshot* SourceNode = Topology.FindNode(LinkedNodeId);
			if (!SourceNode)
			{
				continue;
			}

			const EPureDataNodeKind PureKind = FPureDataSubgraphPolicy::ClassifyNode(*SourceNode);
			const ENodeRole SourceRole = FClassifier::ClassifyNode(*SourceNode, RuleSet).Role;

			if (PureKind == EPureDataNodeKind::DataTransform)
			{
				const FVector2D RootTarget = FBlueprintHelperNodeInputClusterBudgetHelpers::BuildLeafRelativeTarget(
					RuleSet,
					ConsumerNodeId,
					LinkedNodeId,
					SourceRole,
					SourceOrder);
				const FPureDataSubgraphEnvelope Envelope = FPureDataSubgraphPolicy::MeasureForRoot(
					Snapshot,
					Topology,
					ConsumerNodeId,
					Pin.PinId,
					LinkedNodeId,
					RuleSet);
				TArray<FString> ClaimedEnvelopeNodeIds;
				for (const FString& NodeId : Envelope.NodeIds)
				{
					const FVector2D* EnvelopeTarget = Envelope.RelativeTargets.Find(NodeId);
					if (!EnvelopeTarget)
					{
						continue;
					}

					if (FBlueprintHelperNodeInputClusterBudgetHelpers::TryAddBudgetTarget(
							Budget,
							NodeId,
							RootTarget + *EnvelopeTarget,
							Envelope.KindByNodeId.FindRef(NodeId),
							Envelope.DataDepthByNodeId.FindRef(NodeId),
							Envelope.LayoutPriorityByNodeId.FindRef(NodeId)))
					{
						ClaimedEnvelopeNodeIds.Add(NodeId);
					}
				}
				if (const TOptional<FBlueprintHelperNodeInputClusterBudgetHelpers::FMeasuredBounds> EnvelopeBounds =
						FBlueprintHelperNodeInputClusterBudgetHelpers::MeasureEnvelopeBounds(
							ClaimedEnvelopeNodeIds,
							Envelope,
							Topology,
							RootTarget,
							RuleSet))
				{
					SupplementalBounds.Add(*EnvelopeBounds);
				}
				++SourceOrder;
				continue;
			}

			if (!FDataInputPlacement::IsDataInputRole(SourceRole))
			{
				continue;
			}

			FBlueprintHelperNodeInputClusterBudgetHelpers::AddBudgetTarget(
				Budget,
				LinkedNodeId,
				FBlueprintHelperNodeInputClusterBudgetHelpers::BuildLeafRelativeTarget(
					RuleSet,
					ConsumerNodeId,
					LinkedNodeId,
					SourceRole,
					SourceOrder),
				EPureDataNodeKind::DataLeaf,
				1,
				-0.1f);
			++SourceOrder;
		}
	}

	FBlueprintHelperNodeInputClusterBudgetHelpers::UpdateBudgetSize(Budget, Topology, SupplementalBounds, RuleSet);
	return Budget;
}
}
