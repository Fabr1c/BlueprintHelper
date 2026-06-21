#include "Systems/GraphLayout/BlueprintHelperGraphLayoutArrangeScopePolicy.h"

namespace BlueprintHelper::GraphLayout
{
struct FGraphLayoutArrangeScopePolicyPrivate
{
	static void AddLinkedNodeIds(const FNodeSnapshot& Node, TSet<FString>& OutLinkedNodeIds)
	{
		for (const FPinSnapshot& Pin : Node.Pins)
		{
			for (const FString& LinkedNodeId : Pin.LinkedNodeIds)
			{
				if (!LinkedNodeId.IsEmpty())
				{
					OutLinkedNodeIds.Add(LinkedNodeId);
				}
			}
		}
	}

	static TMap<FString, TSet<FString>> BuildUndirectedAdjacency(const FGraphSnapshot& Snapshot)
	{
		TMap<FString, TSet<FString>> Adjacency;
		for (const FNodeSnapshot& Node : Snapshot.Nodes)
		{
			Adjacency.FindOrAdd(Node.NodeId);
		}

		for (const FNodeSnapshot& Node : Snapshot.Nodes)
		{
			TSet<FString> LinkedNodeIds;
			AddLinkedNodeIds(Node, LinkedNodeIds);
			for (const FString& LinkedNodeId : LinkedNodeIds)
			{
				if (!Adjacency.Contains(LinkedNodeId))
				{
					continue;
				}
				Adjacency.FindOrAdd(Node.NodeId).Add(LinkedNodeId);
				Adjacency.FindOrAdd(LinkedNodeId).Add(Node.NodeId);
			}
		}
		return Adjacency;
	}

	static bool IsConnectedToMovableNode(
		const FString& NodeId,
		const TMap<FString, TSet<FString>>& Adjacency,
		const TSet<FString>& MovableNodeIds)
	{
		if (const TSet<FString>* LinkedNodeIds = Adjacency.Find(NodeId))
		{
			for (const FString& LinkedNodeId : *LinkedNodeIds)
			{
				if (MovableNodeIds.Contains(LinkedNodeId))
				{
					return true;
				}
			}
		}
		return false;
	}
};

FGraphLayoutArrangeScope FGraphLayoutArrangeScopePolicy::Build(
	const FGraphSnapshot& Snapshot,
	const FGraphTopology& Topology,
	const FRuleSet& RuleSet)
{
	(void)Topology;

	FGraphLayoutArrangeScope Scope;
	for (const FNodeSnapshot& Node : Snapshot.Nodes)
	{
		Scope.AllNodeIds.Add(Node.NodeId);
		if ((!Node.bExisting && RuleSet.bMoveGeneratedNodes) ||
			(Node.bExisting && RuleSet.bMoveExistingNodes))
		{
			Scope.ToArrangeNodeIds.Add(Node.NodeId);
			Scope.MobilityByNodeId.Add(
				Node.NodeId,
				Node.bExisting
					? EGraphLayoutNodeMobility::MovableExisting
					: EGraphLayoutNodeMobility::MovableGenerated);
		}
	}

	const TMap<FString, TSet<FString>> Adjacency =
		FGraphLayoutArrangeScopePolicyPrivate::BuildUndirectedAdjacency(Snapshot);
	for (const FNodeSnapshot& Node : Snapshot.Nodes)
	{
		if (Scope.ToArrangeNodeIds.Contains(Node.NodeId))
		{
			continue;
		}

		if (FGraphLayoutArrangeScopePolicyPrivate::IsConnectedToMovableNode(
			Node.NodeId,
			Adjacency,
			Scope.ToArrangeNodeIds))
		{
			Scope.AnchorNodeIds.Add(Node.NodeId);
			Scope.MobilityByNodeId.Add(Node.NodeId, EGraphLayoutNodeMobility::Anchor);
		}
		else
		{
			Scope.ObstacleNodeIds.Add(Node.NodeId);
			Scope.MobilityByNodeId.Add(Node.NodeId, EGraphLayoutNodeMobility::Obstacle);
		}
	}

	return Scope;
}
}
