#pragma once

#include "CoreMinimal.h"

class FJsonObject;

class BLUEPRINTHELPER_API FBlueprintHelperReadContextProjectionBridge
{
public:
	static void AttachTaskCoreProjectionMetadata(
		TSharedRef<FJsonObject> Payload,
		const FString& RequestedFormat,
		bool bProjectionUnavailable);
};
