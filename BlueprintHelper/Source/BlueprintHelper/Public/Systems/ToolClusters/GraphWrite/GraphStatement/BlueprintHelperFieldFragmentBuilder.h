#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuildRequest.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

class UEdGraph;

struct BLUEPRINTHELPER_API FBlueprintHelperFieldFragmentPlan
{
	FString CapabilityId;
	FString FieldName;
	TMap<FString, FString> CapabilityFacts;
};

class BLUEPRINTHELPER_API FBlueprintHelperFieldFragmentBuilder
{
public:
	static FString ExpectedNodeFamilyForCapability(const FString& CapabilityId);
	static bool DoesCapabilityProduceExecPins(const FString& CapabilityId);

	static bool BuildVariableGetFragment(
		UEdGraph* TargetGraph,
		const FBlueprintHelperGraphFragmentBuildRequest& Request,
		const FBlueprintHelperActionResolutionResult& ActionResult,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError);

	static bool BuildVariableSetFragment(
		UEdGraph* TargetGraph,
		const FBlueprintHelperGraphFragmentBuildRequest& Request,
		const FBlueprintHelperActionResolutionResult& ActionResult,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError);

	static bool BuildStructReadFragment(
		UEdGraph* TargetGraph,
		const FBlueprintHelperFieldFragmentPlan& Plan,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError);

	static bool BuildStructWriteFragment(
		UEdGraph* TargetGraph,
		const FBlueprintHelperFieldFragmentPlan& Plan,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError);

	static bool BuildNestedPropertyPathFragment(
		UEdGraph* TargetGraph,
		const FBlueprintHelperFieldFragmentPlan& Plan,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError);
};
