// BlueprintHelper Utils -- TaskRuntime 工具函数库

#pragma once

#include "CoreMinimal.h"
#include "BlueprintHelperTaskRuntimeUtils.generated.h"

UCLASS()
class BLUEPRINTHELPER_API UBlueprintHelperTaskRuntimeUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static FString BlueprintHelperNormalizeDryRunMode(FString Mode);
};
