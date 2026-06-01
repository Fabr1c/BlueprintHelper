#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPureDataSubgraphPolicy.h"

namespace BlueprintHelper::GraphLayout
{
namespace
{
static bool HasPin(const FNodeSnapshot& Node, EPinDirection Direction, bool bExec)
{
	for (const FPinSnapshot& Pin : Node.Pins)
	{
		if (Pin.Direction == Direction && Pin.bExec == bExec)
		{
			return true;
		}
	}
	return false;
}

static bool AddRelativeTarget(
	FPureDataSubgraphEnvelope& Envelope,
	TSet<FString>& ClaimedNodes,
	const FNodeSnapshot& Node,
	const FVector2D& RelativeTarget)
{
	if (ClaimedNodes.Contains(Node.NodeId))
	{
		return false;
	}

	ClaimedNodes.Add(Node.NodeId);
	Envelope.NodeIds.Add(Node.NodeId);
	Envelope.RelativeTargets.Add(Node.NodeId, RelativeTarget);
	return true;
}

static void MeasureNodeFromInputs(
	const FGraphTopology& Topology,
	const FString& NodeId,
	const FVector2D& RelativeTarget,
	const FRuleSet& RuleSet,
	TSet<FString>& ClaimedNodes,
	FPureDataSubgraphEnvelope& Envelope)
{
	const FNodeSnapshot* Node = Topology.FindNode(NodeId);
	if (!Node)
	{
		return;
	}

	const EPureDataNodeKind Kind = FPureDataSubgraphPolicy::ClassifyNode(*Node);
	if (Kind == EPureDataNodeKind::None)
	{
		return;
	}

	if (!AddRelativeTarget(Envelope, ClaimedNodes, *Node, RelativeTarget))
	{
		return;
	}

	if (Kind != EPureDataNodeKind::DataTransform)
	{
		return;
	}

	const TArray<FDataEdge> InputEdges = Topology.GetDataInputs(NodeId);
	int32 InputOrder = 0;
	for (const FDataEdge& Edge : InputEdges)
	{
		const FNodeSnapshot* SourceNode = Topology.FindNode(Edge.SourceNodeId);
		if (!SourceNode)
		{
			continue;
		}

		const EPureDataNodeKind SourceKind = FPureDataSubgraphPolicy::ClassifyNode(*SourceNode);
		if (SourceKind == EPureDataNodeKind::None)
		{
			continue;
		}

		const FVector2D SourceTarget(
			RelativeTarget.X - RuleSet.VariableInputOffsetX,
			RelativeTarget.Y + InputOrder * RuleSet.InputPinRowSpacing);
		MeasureNodeFromInputs(Topology, Edge.SourceNodeId, SourceTarget, RuleSet, ClaimedNodes, Envelope);
		++InputOrder;
	}
}

static void UpdateEnvelopeSize(
	FPureDataSubgraphEnvelope& Envelope,
	const FGraphTopology& Topology,
	const FRuleSet& RuleSet)
{
	if (Envelope.RelativeTargets.IsEmpty())
	{
		Envelope.Size = FVector2D::ZeroVector;
		return;
	}

	FVector2D Min(TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
	FVector2D Max(-TNumericLimits<float>::Max(), -TNumericLimits<float>::Max());
	bool bHasBounds = false;

	for (const TPair<FString, FVector2D>& TargetByNode : Envelope.RelativeTargets)
	{
		const FNodeSnapshot* Node = Topology.FindNode(TargetByNode.Key);
		if (!Node)
		{
			continue;
		}

		const FVector2D NodeMin = TargetByNode.Value;
		const FVector2D NodeMax = TargetByNode.Value + Node->Size;
		Min.X = FMath::Min(Min.X, NodeMin.X);
		Min.Y = FMath::Min(Min.Y, NodeMin.Y);
		Max.X = FMath::Max(Max.X, NodeMax.X);
		Max.Y = FMath::Max(Max.Y, NodeMax.Y);
		bHasBounds = true;
	}

	if (!bHasBounds)
	{
		Envelope.Size = FVector2D::ZeroVector;
		return;
	}

	Envelope.Size = FVector2D(
		(Max.X - Min.X) + RuleSet.DataClusterPaddingX,
		(Max.Y - Min.Y) + RuleSet.DataClusterPaddingY);
}
}

EPureDataNodeKind FPureDataSubgraphPolicy::ClassifyNode(const FNodeSnapshot& Node)
{
	for (const FPinSnapshot& Pin : Node.Pins)
	{
		if (Pin.bExec)
		{
			return EPureDataNodeKind::None;
		}
	}

	const bool bHasDataInput = HasPin(Node, EPinDirection::Input, false);
	const bool bHasDataOutput = HasPin(Node, EPinDirection::Output, false);
	if (!bHasDataInput && bHasDataOutput)
	{
		return EPureDataNodeKind::DataLeaf;
	}
	if (bHasDataInput && bHasDataOutput)
	{
		return EPureDataNodeKind::DataTransform;
	}
	return EPureDataNodeKind::None;
}

FPureDataSubgraphEnvelope FPureDataSubgraphPolicy::MeasureForSink(
	const FGraphSnapshot& Snapshot,
	const FGraphTopology& Topology,
	const FString& SinkNodeId,
	const FString& SinkPinId,
	const FRuleSet& RuleSet)
{
	(void)Snapshot;

	FPureDataSubgraphEnvelope Envelope;
	Envelope.SinkNodeId = SinkNodeId;
	Envelope.SinkPinId = SinkPinId;

	const TArray<FDataEdge> SinkInputs = Topology.GetDataInputs(SinkNodeId);
	for (const FDataEdge& Edge : SinkInputs)
	{
		if (Edge.TargetInputPinId != SinkPinId && Edge.TargetInputPinName != SinkPinId)
		{
			continue;
		}

		const FNodeSnapshot* SourceNode = Topology.FindNode(Edge.SourceNodeId);
		if (!SourceNode || ClassifyNode(*SourceNode) == EPureDataNodeKind::None)
		{
			continue;
		}

		return MeasureForRoot(Snapshot, Topology, SinkNodeId, SinkPinId, Edge.SourceNodeId, RuleSet);
	}

	return Envelope;
}

FPureDataSubgraphEnvelope FPureDataSubgraphPolicy::MeasureForRoot(
	const FGraphSnapshot& Snapshot,
	const FGraphTopology& Topology,
	const FString& SinkNodeId,
	const FString& SinkPinId,
	const FString& RootNodeId,
	const FRuleSet& RuleSet)
{
	(void)Snapshot;

	FPureDataSubgraphEnvelope Envelope;
	Envelope.SinkNodeId = SinkNodeId;
	Envelope.SinkPinId = SinkPinId;

	const FNodeSnapshot* RootNode = Topology.FindNode(RootNodeId);
	if (!RootNode || ClassifyNode(*RootNode) == EPureDataNodeKind::None)
	{
		return Envelope;
	}

	Envelope.RootNodeId = RootNodeId;
	TSet<FString> ClaimedNodes;
	MeasureNodeFromInputs(Topology, RootNodeId, FVector2D::ZeroVector, RuleSet, ClaimedNodes, Envelope);
	UpdateEnvelopeSize(Envelope, Topology, RuleSet);
	return Envelope;
}
}
