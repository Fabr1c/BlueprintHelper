#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"

#include "BlueprintNodeBinder.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphSchema.h"
#include "K2Node.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphDefaultValueApplier.h"

UK2Node* FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionResolutionResult& ActionResult,
	const FVector2D& Location,
	FString& OutError)
{
	FBlueprintHelperActionNodeSpawnOptions Options;
	return InvokeSelectedSpawner(TargetGraph, ActionResult, Location, Options, OutError);
}

void FBlueprintHelperActionNodeSpawnerAdapter::NoOpPinNormalization(
	UK2Node&,
	const FBlueprintHelperActionNodeSpawnContext&)
{
}

UK2Node* FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionResolutionResult& ActionResult,
	const FVector2D& Location,
	const FBlueprintHelperActionNodeSpawnOptions& Options,
	FString& OutError,
	TArray<FBlueprintGeneratorDiagnostic>* OutDefaultValueDiagnostics)
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
	if (Options.bReconstructAfterSpawn && TargetGraph->GetSchema())
	{
		TargetGraph->GetSchema()->ReconstructNode(*K2Node);
	}

	FBlueprintHelperActionNodeSpawnContext Context;
	Context.TargetGraph = TargetGraph;
	Context.ActionResult = &ActionResult;
	Context.Location = Location;
	Context.NodeId = Options.NodeId;
	if (Options.PinNormalizationHook)
	{
		Options.PinNormalizationHook(*K2Node, Context);
	}
	else
	{
		NoOpPinNormalization(*K2Node, Context);
	}

	TMap<FString, FString> EffectiveDefaultValues = Options.DefaultValues;
	if (Options.DefaultValueProvider)
	{
		Options.DefaultValueProvider(*K2Node, Context, EffectiveDefaultValues);
	}

	if (Options.bApplyDefaultValues && EffectiveDefaultValues.Num() > 0)
	{
		TArray<FBlueprintGeneratorDiagnostic> Diagnostics =
			FBlueprintGraphDefaultValueApplier::ApplyDefaultValues(K2Node, EffectiveDefaultValues, Options.NodeId);
		if (OutDefaultValueDiagnostics)
		{
			OutDefaultValueDiagnostics->Append(MoveTemp(Diagnostics));
		}
	}
	return K2Node;
}
