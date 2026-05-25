#pragma once

#include "CoreMinimal.h"

class UBlueprintNodeSpawner;
class UClass;
class UEdGraphNode;

class FBlueprintHelperGenericTransformSpawnerFactory
{
public:
	static UBlueprintNodeSpawner* CreateCastSpawner(
		TSubclassOf<UEdGraphNode> ResolvedNodeClass,
		UClass* TargetClass);
};
