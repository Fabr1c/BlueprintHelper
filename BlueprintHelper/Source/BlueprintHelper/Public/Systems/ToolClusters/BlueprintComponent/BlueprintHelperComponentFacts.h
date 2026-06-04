#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/BlueprintComponent/BlueprintHelperComponentService.h"

class UBlueprint;
class UActorComponent;
class AActor;
class USCS_Node;

class BLUEPRINTHELPER_API FBlueprintHelperComponentFacts
{
public:
	static TArray<FBlueprintHelperComponentInfo> BuildReadbackFacts(const UBlueprint& Blueprint);
	static FBlueprintHelperComponentInfo BuildReadbackFact(const UBlueprint& Blueprint, const USCS_Node& Node);
	static FBlueprintHelperComponentInfo BuildNativeReadbackFact(
		const UBlueprint& Blueprint,
		const AActor& NativeOwner,
		const UActorComponent& ComponentTemplate);
};
