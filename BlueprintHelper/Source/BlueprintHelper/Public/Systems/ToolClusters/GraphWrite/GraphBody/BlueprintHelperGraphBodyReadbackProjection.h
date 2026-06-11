#pragma once

#include "CoreMinimal.h"

struct BLUEPRINTHELPER_API FBlueprintHelperGraphBodyReadbackProjection
{
	FString ProjectionId;
	FString EntryNodeRef;
	TArray<FString> ExitNodeRefs;
	TArray<FString> FoldedBoundaryNodeRefs;
	TArray<FString> VisibleBoundaryNodeRefs;
	TMap<FString, FString> BoundaryDisplayNames;
	FString BodyEntryNodeGuid;
	FString BodyEntryNodeClass;
	FString BodyEntryFingerprint;
	FString BodyFingerprint;
	bool bSynthesizeLogicEntry = false;
	bool bSynthesizeLogicResult = false;
};
