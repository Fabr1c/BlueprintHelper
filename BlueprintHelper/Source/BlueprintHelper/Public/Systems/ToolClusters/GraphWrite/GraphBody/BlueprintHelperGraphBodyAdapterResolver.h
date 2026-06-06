#pragma once

#include "CoreMinimal.h"
#include "Shared/GraphWrite/BlueprintHelperReplaceGraphTypes.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyAdapter.h"

class UBlueprint;

class BLUEPRINTHELPER_API FBlueprintHelperGraphBodyAdapterResolver
{
public:
	static EBlueprintHelperGraphBodyKind BodyKindForReplaceScope(EBlueprintHelperReplaceScope Scope);
	static FString RuntimeAdapterIdForReplaceScope(EBlueprintHelperReplaceScope Scope);
	static bool TryCreateByRuntimeAdapterId(
		const FString& RuntimeAdapterId,
		TUniquePtr<IBlueprintHelperGraphBodyAdapter>& OutAdapter,
		FString& OutError);
	static bool TryCreateForReplaceScope(
		EBlueprintHelperReplaceScope Scope,
		TUniquePtr<IBlueprintHelperGraphBodyAdapter>& OutAdapter,
		FString& OutError);
	static bool TryCreateForReadTarget(
		const FBlueprintHelperTargetRef& Target,
		TUniquePtr<IBlueprintHelperGraphBodyAdapter>& OutAdapter,
		FString& OutError);
	static FBlueprintHelperGraphBodyRequest MakeReadRequestForTarget(
		const FBlueprintHelperTargetRef& Target,
		UBlueprint* Blueprint);
};
