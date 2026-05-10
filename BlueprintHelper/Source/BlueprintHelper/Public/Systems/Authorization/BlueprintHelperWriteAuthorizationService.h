// BlueprintHelper write authorization service.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "HAL/CriticalSection.h"
#include "Templates/Function.h"

struct FBlueprintHelperBridgeValidationError;

struct BLUEPRINTHELPER_API FBlueprintHelperWriteSessionRequest
{
	FString Reason;
	FString Scope = TEXT("project");
	int32 TtlSeconds = 900;
	TArray<FString> AssetPaths;
};

struct BLUEPRINTHELPER_API FBlueprintHelperWriteSessionGrant
{
	FString SessionId;
	FString Scope = TEXT("project");
	FDateTime ExpiresAtUtc;
	TArray<FString> AssetPaths;

	bool IsExpired() const;
	TSharedRef<FJsonObject> ToJson() const;
};

class BLUEPRINTHELPER_API FBlueprintHelperWriteAuthorizationService
{
public:
	static FBlueprintHelperWriteAuthorizationService& Get();

	TOptional<FBlueprintHelperWriteSessionGrant> RequestSession(
		const FBlueprintHelperWriteSessionRequest& Request,
		FString& OutError);

	FBlueprintHelperWriteSessionGrant CreateApprovedSessionForTesting(
		const FBlueprintHelperWriteSessionRequest& Request);

	bool ValidateSessionForCommand(
		const FString& SessionId,
		const FString& Command,
		const TSharedPtr<FJsonObject>& Payload,
		FBlueprintHelperBridgeValidationError& OutError);

	bool HasActiveSession();
	void ResetForTesting();
	void SetApprovalProviderForTesting(TFunction<bool(const FBlueprintHelperWriteSessionRequest&)> InProvider);

private:
	FBlueprintHelperWriteAuthorizationService() = default;

	FBlueprintHelperWriteSessionGrant CreateGrant(const FBlueprintHelperWriteSessionRequest& Request);
	bool RequestUserApproval(const FBlueprintHelperWriteSessionRequest& Request) const;
	bool GrantCoversPayload(const FBlueprintHelperWriteSessionGrant& Grant, const TSharedPtr<FJsonObject>& Payload) const;
	void RemoveExpiredSessions();

	mutable FCriticalSection Mutex;
	TMap<FString, FBlueprintHelperWriteSessionGrant> Sessions;
	TFunction<bool(const FBlueprintHelperWriteSessionRequest&)> ApprovalProviderForTesting;
};
