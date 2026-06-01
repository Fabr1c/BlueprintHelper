#include "Systems/GraphLayout/BlueprintHelperGraphLayoutTopology.h"

#include "Algo/Sort.h"

namespace BlueprintHelper::GraphLayout
{
void FGraphTopology::AddNode(const FNodeSnapshot& Node)
{
	OwnedNodesById.Add(Node.NodeId, Node);
	ExecInputCounts.FindOrAdd(Node.NodeId) = 0;
	ExecOutputPinCounts.FindOrAdd(Node.NodeId) = 0;
}

void FGraphTopology::AddExecEdge(const FExecEdge& Edge)
{
	if (!OwnedNodesById.Contains(Edge.SourceNodeId) || !OwnedNodesById.Contains(Edge.TargetNodeId))
	{
		return;
	}
	ExecEdgesBySource.Add(Edge.SourceNodeId, Edge);
	++ExecInputCounts.FindOrAdd(Edge.TargetNodeId);
}

void FGraphTopology::AddDataEdge(const FDataEdge& Edge)
{
	if (!OwnedNodesById.Contains(Edge.SourceNodeId) || !OwnedNodesById.Contains(Edge.TargetNodeId))
	{
		return;
	}
	DataEdgesByTarget.Add(Edge.TargetNodeId, Edge);
}

const FNodeSnapshot* FGraphTopology::FindNode(const FString& NodeId) const
{
	return OwnedNodesById.Find(NodeId);
}

TArray<FExecEdge> FGraphTopology::GetExecOutputEdges(const FString& NodeId) const
{
	TArray<FExecEdge> Edges;
	ExecEdgesBySource.MultiFind(NodeId, Edges);
	Algo::Sort(Edges, [](const FExecEdge& Left, const FExecEdge& Right)
	{
		if (Left.SourceOutputOrdinal != Right.SourceOutputOrdinal)
		{
			return Left.SourceOutputOrdinal < Right.SourceOutputOrdinal;
		}
		return Left.TargetOrdinalWithinOutput < Right.TargetOrdinalWithinOutput;
	});
	return Edges;
}

TArray<FDataEdge> FGraphTopology::GetDataInputs(const FString& NodeId) const
{
	TArray<FDataEdge> Edges;
	DataEdgesByTarget.MultiFind(NodeId, Edges);
	Algo::Sort(Edges, [](const FDataEdge& Left, const FDataEdge& Right)
	{
		if (Left.TargetInputOrdinal != Right.TargetInputOrdinal)
		{
			return Left.TargetInputOrdinal < Right.TargetInputOrdinal;
		}
		if (Left.TargetInputPinId != Right.TargetInputPinId)
		{
			return Left.TargetInputPinId < Right.TargetInputPinId;
		}
		if (Left.TargetLinkedNodeOrdinal != Right.TargetLinkedNodeOrdinal)
		{
			return Left.TargetLinkedNodeOrdinal < Right.TargetLinkedNodeOrdinal;
		}
		return Left.SourceNodeId < Right.SourceNodeId;
	});
	return Edges;
}

bool FGraphTopology::IsMultiExecOutputNode(const FString& NodeId) const
{
	return ExecOutputPinCounts.FindRef(NodeId) > 1;
}

int32 FGraphTopology::CountExecInputs(const FString& NodeId) const
{
	return ExecInputCounts.FindRef(NodeId);
}

FGraphTopology FGraphLayoutTopology::Build(const FGraphSnapshot& Snapshot)
{
	FGraphTopology Topology;
	for (const FNodeSnapshot& Node : Snapshot.Nodes)
	{
		Topology.AddNode(Node);
	}

	for (const FNodeSnapshot& Node : Snapshot.Nodes)
	{
		int32 ExecOutputOrdinal = 0;
		for (int32 PinIndex = 0; PinIndex < Node.Pins.Num(); ++PinIndex)
		{
			const FPinSnapshot& Pin = Node.Pins[PinIndex];
			if (Pin.Direction == EPinDirection::Output && Pin.bExec)
			{
				++Topology.ExecOutputPinCounts.FindOrAdd(Node.NodeId);
				for (int32 TargetIndex = 0; TargetIndex < Pin.LinkedNodeIds.Num(); ++TargetIndex)
				{
					FExecEdge Edge;
					Edge.SourceNodeId = Node.NodeId;
					Edge.SourceOutputPinId = Pin.PinId;
					Edge.SourceOutputPinName = Pin.Name;
					Edge.SourceOutputOrdinal = ExecOutputOrdinal;
					Edge.TargetNodeId = Pin.LinkedNodeIds[TargetIndex];
					Edge.TargetOrdinalWithinOutput = TargetIndex;
					Topology.AddExecEdge(Edge);
				}
				++ExecOutputOrdinal;
			}
			else if (Pin.Direction == EPinDirection::Input && !Pin.bExec)
			{
				for (int32 LinkedNodeIndex = 0; LinkedNodeIndex < Pin.LinkedNodeIds.Num(); ++LinkedNodeIndex)
				{
					FDataEdge Edge;
					Edge.SourceNodeId = Pin.LinkedNodeIds[LinkedNodeIndex];
					Edge.TargetNodeId = Node.NodeId;
					Edge.TargetInputPinId = Pin.PinId;
					Edge.TargetInputPinName = Pin.Name;
					Edge.TargetInputOrdinal = PinIndex;
					Edge.TargetLinkedNodeOrdinal = LinkedNodeIndex;
					Topology.AddDataEdge(Edge);
				}
			}
		}
	}
	return Topology;
}
}
