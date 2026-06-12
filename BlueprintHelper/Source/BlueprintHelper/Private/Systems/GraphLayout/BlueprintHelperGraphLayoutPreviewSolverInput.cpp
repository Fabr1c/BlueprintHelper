#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSolverInput.h"

namespace BlueprintHelper::GraphLayout
{
bool FGraphLayoutPreviewSolverInput::IsPreviewOverlayNode(
	const FGraphLayoutPreviewSample& Sample,
	const FString& NodeId)
{
	const FGraphLayoutPreviewNodeSpec* NodeSpec = Sample.Nodes.FindByPredicate([&NodeId](const FGraphLayoutPreviewNodeSpec& Candidate)
	{
		return Candidate.NodeId == NodeId;
	});
	return NodeSpec && NodeSpec->bPreviewOverlay;
}

FGraphSnapshot FGraphLayoutPreviewSolverInput::BuildSolverSnapshot(const FGraphLayoutPreviewSample& Sample)
{
	FGraphSnapshot SolverSnapshot;
	SolverSnapshot.GraphName = Sample.Snapshot.GraphName;

	TSet<FString> RetainedNodeIds;
	for (const FNodeSnapshot& Node : Sample.Snapshot.Nodes)
	{
		if (!IsPreviewOverlayNode(Sample, Node.NodeId))
		{
			RetainedNodeIds.Add(Node.NodeId);
		}
	}

	for (const FNodeSnapshot& Node : Sample.Snapshot.Nodes)
	{
		if (!RetainedNodeIds.Contains(Node.NodeId))
		{
			continue;
		}

		FNodeSnapshot RetainedNode = Node;
		for (FPinSnapshot& Pin : RetainedNode.Pins)
		{
			Pin.LinkedNodeIds.RemoveAll([&RetainedNodeIds](const FString& LinkedNodeId)
			{
				return !RetainedNodeIds.Contains(LinkedNodeId);
			});
		}
		SolverSnapshot.Nodes.Add(MoveTemp(RetainedNode));
	}
	return SolverSnapshot;
}
}
