#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FJsonObject;

class BLUEPRINTHELPER_API IBlueprintHelperReadContextProjectionBackend
{
public:
	virtual ~IBlueprintHelperReadContextProjectionBackend() = default;

	virtual bool Project(
		const TSharedRef<FJsonObject>& RawLogicJson,
		const FString& RequestedFormat,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError) = 0;
};

class BLUEPRINTHELPER_API FBlueprintHelperReadContextProjectionGateway
{
public:
	static void SetBackend(TSharedPtr<IBlueprintHelperReadContextProjectionBackend> InBackend);
	static void ClearBackend();

	static bool Project(
		const TSharedRef<FJsonObject>& RawLogicJson,
		const FString& RequestedFormat,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError);

	static FBlueprintHelperToolError MakeBackendUnavailableError(const FString& RequestedFormat);
};
