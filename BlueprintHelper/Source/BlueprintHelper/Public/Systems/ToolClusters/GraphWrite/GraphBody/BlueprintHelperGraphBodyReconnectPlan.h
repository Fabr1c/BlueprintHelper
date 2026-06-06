#pragma once

#include "CoreMinimal.h"

struct BLUEPRINTHELPER_API FBlueprintHelperGraphBodyReconnectPlan
{
	TArray<FString> EntryBoundaryRefs;
	TArray<FString> ExitBoundaryRefs;
	TArray<FString> RequiredLinkCodes;
	bool bReconnectEntryToFirstImportedExec = true;
	bool bReconnectImportedExecToExitBoundary = false;
};
