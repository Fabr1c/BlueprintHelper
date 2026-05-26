#pragma once

#include "CoreMinimal.h"

class UEdGraphNode;
struct FBlueprintHelperEventDelegateUseSiteEvidence;
struct FBlueprintHelperNodeFragment;

struct FBlueprintHelperEventDelegateReadbackFacts
{
	TMap<FString, FString> Facts;
};

class FBlueprintHelperEventDelegateReadback
{
public:
	static FBlueprintHelperEventDelegateReadbackFacts Collect(
		const FBlueprintHelperNodeFragment& Fragment,
		const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence);
};
