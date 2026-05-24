#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentSpawnCoordinator.h"

class UK2Node;

class FBlueprintHelperActionFragmentBuildUtils
{
public:
	static void PopulatePins(
		EBlueprintHelperActionFragmentPinProfile PinProfile,
		UK2Node* Node,
		FBlueprintHelperNodeFragment& OutFragment);

	static void PopulateCallFragmentPins(
		UK2Node* CallNode,
		FBlueprintHelperNodeFragment& OutFragment);

	static void PopulateActionProviderFragmentPins(
		UK2Node* Node,
		FBlueprintHelperNodeFragment& OutFragment);

	static void PopulateCommonMetadata(
		const FBlueprintHelperGraphFragmentBuildRequest& Request,
		FBlueprintHelperNodeFragment& OutFragment);

	static void AppendCandidateActionGroup(
		const FString& Target,
		const FBlueprintHelperActionResolutionResult& ResolveResult,
		TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions);
};
