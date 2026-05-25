#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuildRequest.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

class UEdGraph;

enum class EBlueprintHelperActionFragmentPinProfile : uint8
{
	Call,
	ActionProvider
};

struct BLUEPRINTHELPER_API FBlueprintHelperActionFragmentSpawnCoordinatorRequest
{
	UEdGraph* TargetGraph = nullptr;
	const FBlueprintHelperGraphFragmentBuildRequest* BuildRequest = nullptr;
	FBlueprintHelperActionResolutionRequest ActionRequest;
	EBlueprintHelperActionSemanticKind SemanticKind = EBlueprintHelperActionSemanticKind::Unknown;
	EBlueprintHelperActionFragmentPinProfile PinProfile = EBlueprintHelperActionFragmentPinProfile::ActionProvider;
	FString CandidateGroupTarget;
	FString FailurePrefix;
	bool bAppendSemanticKindOwnershipTag = false;
};

class BLUEPRINTHELPER_API FBlueprintHelperActionFragmentSpawnCoordinator
{
public:
	static bool ValidateResolvedActionFragment(
		const FBlueprintHelperActionFragmentSpawnCoordinatorRequest& Request,
		FString& OutError,
		FString* OutSelectedStableId = nullptr,
		TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions = nullptr);

	static bool BuildResolvedActionFragment(
		const FBlueprintHelperActionFragmentSpawnCoordinatorRequest& Request,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError,
		TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions = nullptr);
};
