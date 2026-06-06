#pragma once

#include "CoreMinimal.h"

struct BLUEPRINTHELPER_API FBlueprintHelperGraphBodySemanticContext
{
	FString ContextId;
	TArray<FString> VariableRefs;
	TArray<FString> FunctionParamRefs;
	TArray<FString> MacroTunnelPinRefs;
	TArray<FString> LocalSymbolRefs;
	TArray<FString> ComponentRefs;
	TArray<FString> GraphOwnedSymbolRefs;
};
