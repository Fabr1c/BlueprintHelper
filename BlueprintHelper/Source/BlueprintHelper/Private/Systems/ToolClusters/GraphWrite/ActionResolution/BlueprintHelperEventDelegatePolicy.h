#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

struct FBlueprintHelperEventDelegateUseSiteEvidence;

struct FBlueprintHelperEventDelegatePolicyDecision
{
	bool bAllowed = false;
	EBlueprintHelperActionResolutionStatus Status = EBlueprintHelperActionResolutionStatus::Blocked;
	FString ErrorCode;
	FString Message;
};

class FBlueprintHelperEventDelegatePolicy
{
public:
	static FBlueprintHelperEventDelegatePolicyDecision Evaluate(
		const FBlueprintHelperActionResolutionRequest& Request,
		const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence);
};
