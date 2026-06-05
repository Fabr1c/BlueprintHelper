#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphWriteRollbackFinalizer.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"

FBlueprintHelperGraphWriteRollbackResult FBlueprintHelperGraphWriteRollbackFinalizer::RollbackPostImportFailure(
	const FBlueprintHelperGraphWriteRollbackInput& Input) const
{
	FBlueprintHelperGraphWriteRollbackResult Result;

	if (!Input.Blueprint)
	{
		Result.ErrorMessage = TEXT("Rollback skipped because Blueprint is null.");
		return Result;
	}

	if (!Input.TargetGraph)
	{
		Result.ErrorMessage = TEXT("Rollback skipped because target graph is null.");
		return Result;
	}

	if (!Input.bGraphExistedBeforeWrite)
	{
		FBlueprintEditorUtils::RemoveGraph(Input.Blueprint, Input.TargetGraph);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Input.Blueprint);
	}
	else
	{
		TArray<UEdGraphNode*> NodesToRemove;
		for (UEdGraphNode* Node : Input.TargetGraph->Nodes)
		{
			if (Node && !Input.NodeSnapshot.Contains(Node))
			{
				NodesToRemove.Add(Node);
			}
		}

		for (UEdGraphNode* Node : NodesToRemove)
		{
			FBlueprintEditorUtils::RemoveNode(Input.Blueprint, Node, true);
		}

		if (NodesToRemove.Num() > 0)
		{
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Input.Blueprint);
		}
	}

	if (Input.Blueprint->GetOutermost())
	{
		Input.Blueprint->GetOutermost()->SetDirtyFlag(Input.bPackageWasDirtyBeforeWrite);
	}

	Result.bRolledBack = true;
	return Result;
}
