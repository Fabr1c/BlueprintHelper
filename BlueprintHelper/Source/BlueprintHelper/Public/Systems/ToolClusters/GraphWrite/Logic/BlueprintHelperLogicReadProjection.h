#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Systems/Debug/BlueprintHelperDiagnosticProjection.h"

struct BLUEPRINTHELPER_API FBlueprintHelperLogicReadProjection
{
	FString RequestedFormat;
	FString RawSchema;
	TSharedPtr<FJsonObject> RawPayload;
	TArray<FBlueprintHelperDiagnosticProjection> Diagnostics;
	TArray<FString> CallbackCapabilities;
};

class BLUEPRINTHELPER_API FBlueprintHelperLogicReadProjectionUtils
{
public:
	static TArray<FString> GetCallbackCapabilities();
};
