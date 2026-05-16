// BlueprintHelper Bridge Layer — request validation helpers

#pragma once

#include "CoreMinimal.h"
#include "Entry/Bridge/BlueprintHelperBridgeTypes.h"
#include "Shared/BlueprintHelperServiceTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperRequestValidator
{
public:
	static bool NormalizeExportScope(
		const FString& InScope,
		EBlueprintHelperExportScope& OutScope,
		FString& OutEffectiveScope,
		FString& OutError);

	static bool ValidatePayloadForCommand(
		const FString& Command,
		const TSharedPtr<FJsonObject>& Payload,
		FBlueprintHelperBridgeValidationError& OutError);

	static bool ValidateAuthorization(
		const FBlueprintHelperBridgeRequest& Request,
		FBlueprintHelperBridgeValidationError& OutError);

	static bool IsWriteCommand(const FString& Command);
	static bool IsHighRiskCommand(const FString& Command);
	static bool IsHighRiskCommandEnabled();
};
