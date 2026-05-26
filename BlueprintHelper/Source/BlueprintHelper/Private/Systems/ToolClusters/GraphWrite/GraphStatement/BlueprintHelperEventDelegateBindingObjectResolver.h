#pragma once

#include "CoreMinimal.h"

class UEdGraphPin;
class UObject;
struct FBlueprintHelperEventDelegateUseSiteEvidence;
struct FBlueprintHelperNodeFragment;

struct FBlueprintHelperEventDelegateBindingObjectResolution
{
	bool bResolved = false;
	FString ErrorCode;
	FString ObjectEvidenceId;
	UEdGraphPin* ObjectPin = nullptr;
	UObject* StableObject = nullptr;
};

class FBlueprintHelperEventDelegateBindingObjectResolver
{
public:
	static FBlueprintHelperEventDelegateBindingObjectResolution Resolve(
		const FBlueprintHelperEventDelegateUseSiteEvidence& Evidence,
		const FBlueprintHelperNodeFragment& Fragment);
};
