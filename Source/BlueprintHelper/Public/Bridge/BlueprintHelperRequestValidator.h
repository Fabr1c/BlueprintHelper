// BlueprintHelper Bridge Layer — request validation helpers

#pragma once

#include "CoreMinimal.h"
#include "Bridge/BlueprintHelperBridgeTypes.h"
#include "Services/BlueprintHelperServiceTypes.h"

struct BLUEPRINTHELPER_API FBlueprintHelperBridgeValidationError
{
	FString Code;
	FString Field;
	FString ExpectedType;
	FString ActualType;
	FString Message;
};

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

	static FString GetConfiguredToken();
};
