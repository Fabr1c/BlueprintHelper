#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"

#include "BlueprintNodeBinder.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphSchema.h"
#include "K2Node.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

UK2Node* FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionResolutionResult& ActionResult,
	const FVector2D& Location,
	FString& OutError)
{
	UBlueprintNodeSpawner* NodeSpawner = ActionResult.SelectedSpawner.Get();
	if (!TargetGraph)
	{
		OutError = TEXT("action provider spawn failed: target graph is invalid.");
		return nullptr;
	}
	if (!NodeSpawner)
	{
		OutError = FString::Printf(
			TEXT("action provider spawn failed: resolved spawner is no longer valid: %s."),
			*ActionResult.SelectedStableId);
		return nullptr;
	}

	IBlueprintNodeBinder::FBindingSet Bindings;
	UEdGraphNode* SpawnedNode = NodeSpawner->Invoke(TargetGraph, Bindings, Location);
	UK2Node* K2Node = Cast<UK2Node>(SpawnedNode);
	if (!K2Node)
	{
		OutError = FString::Printf(
			TEXT("action provider spawn failed: spawner did not create a K2 node: %s."),
			*ActionResult.SelectedStableId);
		return nullptr;
	}

	K2Node->NodePosX = static_cast<int32>(Location.X);
	K2Node->NodePosY = static_cast<int32>(Location.Y);
	if (TargetGraph->GetSchema())
	{
		TargetGraph->GetSchema()->ReconstructNode(*K2Node);
	}
	return K2Node;
}
