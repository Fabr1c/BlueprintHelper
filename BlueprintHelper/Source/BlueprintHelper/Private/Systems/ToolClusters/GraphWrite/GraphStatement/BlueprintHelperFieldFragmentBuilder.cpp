#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperFieldFragmentBuilder.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_SetFieldsInStruct.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentBuildUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/GraphWriteGraphStatementUtils.h"

FString FBlueprintHelperFieldFragmentBuilder::ExpectedNodeFamilyForCapability(const FString& CapabilityId)
{
	const FBlueprintHelperFieldCapabilitySpec* Spec = FBlueprintHelperFieldCapabilityRegistry::FindById(CapabilityId);
	return Spec ? Spec->ExpectedNodeFamily : FString();
}

bool FBlueprintHelperFieldFragmentBuilder::DoesCapabilityProduceExecPins(const FString& CapabilityId)
{
	const FBlueprintHelperFieldCapabilitySpec* Spec = FBlueprintHelperFieldCapabilityRegistry::FindById(CapabilityId);
	return Spec && Spec->bProducesExecPins;
}

bool FBlueprintHelperFieldFragmentBuilder::BuildVariableGetFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphFragmentBuildRequest& Request,
	const FBlueprintHelperActionResolutionResult& ActionResult,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	return UGraphWriteGraphStatementUtils::BuildVariableFragment(TargetGraph, Request, ActionResult, OutFragment, OutError);
}

bool FBlueprintHelperFieldFragmentBuilder::BuildVariableSetFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphFragmentBuildRequest& Request,
	const FBlueprintHelperActionResolutionResult& ActionResult,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	return UGraphWriteGraphStatementUtils::BuildVariableFragment(TargetGraph, Request, ActionResult, OutFragment, OutError);
}

bool FBlueprintHelperFieldFragmentBuilder::BuildStructReadFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperFieldFragmentPlan& Plan,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	return UGraphWriteGraphStatementUtils::BuildStructNodeFragment(TargetGraph, Plan, TEXT("struct_read"), false, OutFragment, OutError);
}

bool FBlueprintHelperFieldFragmentBuilder::BuildStructWriteFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperFieldFragmentPlan& Plan,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	return UGraphWriteGraphStatementUtils::BuildStructNodeFragment(TargetGraph, Plan, TEXT("struct_write"), true, OutFragment, OutError);
}

bool FBlueprintHelperFieldFragmentBuilder::BuildNestedPropertyPathFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperFieldFragmentPlan& Plan,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	return UGraphWriteGraphStatementUtils::BuildNestedStructBreakPathFragment(TargetGraph, Plan, OutFragment, OutError);
}
