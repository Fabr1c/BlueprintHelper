#pragma once

#include "CoreMinimal.h"

struct BLUEPRINTHELPER_API FBlueprintHelperGraphBodyReadbackProjection
{
	FString ProjectionId;
	FString EntryNodeRef;
	TArray<FString> ExitNodeRefs;
	TArray<FString> FoldedBoundaryNodeRefs;
	TArray<FString> VisibleBoundaryNodeRefs;
	bool bSynthesizeLogicEntry = false;
	bool bSynthesizeLogicResult = false;
};
