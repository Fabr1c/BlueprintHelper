#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

enum class EBlueprintHelperGenericActionProviderMode : uint8
{
	NodeSpawnerCandidate,
	DedicatedFragmentBuilderRequired,
	NeedsMoreSemanticContext,
	Unsupported
};

struct FBlueprintHelperGenericActionProviderBoundary
{
	EBlueprintHelperGenericActionProviderMode Mode = EBlueprintHelperGenericActionProviderMode::Unsupported;
	FString Reason;
	FString RequiredBuilder;
};

class BLUEPRINTHELPER_API FBlueprintHelperGenericActionProviderBoundaryService
{
public:
	static FBlueprintHelperGenericActionProviderBoundary Classify(const FBlueprintHelperActionResolutionRequest& Request);
};
