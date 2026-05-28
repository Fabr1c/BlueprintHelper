// BlueprintHelper Utils -- Blueprint Class Settings 工具函数库

#pragma once

#include "CoreMinimal.h"
#include "BlueprintHelperClassSettingsUtils.generated.h"

struct FBlueprintHelperInvalidInterface;
struct FBlueprintHelperInvalidClassDefaultSetting;

UCLASS()
class BLUEPRINTHELPER_API UBlueprintHelperClassSettingsUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static FString BlueprintClassSettingsDescribeInvalidInterface(const FBlueprintHelperInvalidInterface& Invalid);

	static FString BlueprintClassSettingsDescribeInvalidDefaultSetting(const FBlueprintHelperInvalidClassDefaultSetting& Invalid);
};
