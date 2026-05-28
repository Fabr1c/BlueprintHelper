#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"

#include "BlueprintNodeBinder.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphSchema.h"
#include "K2Node.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphDefaultValueApplier.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Utils/GraphWriteActionAdapterUtils.h"

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
	return UGraphWriteActionAdapterUtils::InvokeNodeSpawnerInternal(
		TargetGraph,
		NodeSpawner,
		&ActionResult,
		ActionResult.SelectedStableId,
		Location,
		Options,
		OutError,
		OutDefaultValueDiagnostics);
}

UK2Node* FBlueprintHelperActionNodeSpawnerAdapter::InvokeNodeSpawner(
	UEdGraph* TargetGraph,
	UBlueprintNodeSpawner* NodeSpawner,
	const FString& StableId,
	const FVector2D& Location,
	const FBlueprintHelperActionNodeSpawnOptions& Options,
	FString& OutError,
	TArray<FBlueprintGeneratorDiagnostic>* OutDefaultValueDiagnostics)
{
	return UGraphWriteActionAdapterUtils::InvokeNodeSpawnerInternal(
		TargetGraph,
		NodeSpawner,
		nullptr,
		StableId,
		Location,
		Options,
		OutError,
		OutDefaultValueDiagnostics);
}
