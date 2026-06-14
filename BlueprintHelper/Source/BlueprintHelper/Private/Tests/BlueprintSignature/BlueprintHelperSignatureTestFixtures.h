#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"

class UBlueprint;
class UClass;

class FBlueprintHelperSignatureTestFixtures
{
public:
	static FString MakeSignatureServiceTestObjectName(const FString& Prefix);
	static UBlueprint* MakeSignatureServiceBlueprint(const FString& Prefix, UClass* ParentClass);
	static UBlueprint* MakeSignatureServiceActorBlueprint(const FString& Prefix);
};

#endif
