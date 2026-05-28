#pragma once
#include "CoreMinimal.h"
#include "GraphWriteReadbackUtils.generated.h"

struct FBlueprintHelperNodeFragment;

UCLASS()
class BLUEPRINTHELPER_API UGraphWriteReadbackUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static void AddIfPresent(TMap<FString, FString>& Facts, const FString& Key, const FString& Value);
	static void AddPrimaryNodeFacts(const FBlueprintHelperNodeFragment& Fragment, TMap<FString, FString>& Facts);
	static void AddPinFacts(const FBlueprintHelperNodeFragment& Fragment, TMap<FString, FString>& Facts);
};
