// BlueprintHelper write authorization service.

#include "Systems/Authorization/BlueprintHelperWriteAuthorizationService.h"

#include "Entry/Bridge/BlueprintHelperRequestValidator.h"
#include "HAL/PlatformProcess.h"
#include "Misc/MessageDialog.h"

namespace
{
	int32 ClampTtlSeconds(int32 TtlSeconds)
	{
		return FMath::Clamp(TtlSeconds <= 0 ? 900 : TtlSeconds, 1, 3600);
	}

	FString NormalizeScope(const FString& Scope)
	{
		return Scope.Equals(TEXT("asset_list"), ESearchCase::IgnoreCase)
			? FString(TEXT("asset_list"))
			: FString(TEXT("project"));
	}

	void SetAuthError(
		FBlueprintHelperBridgeValidationError& OutError,
		const FString& Code,
		const FString& Message)
	{
		OutError.Code = Code;
		OutError.Field = TEXT("auth_session");
		OutError.Message = Message;
	}

	bool TryReadPayloadAsset(const TSharedPtr<FJsonObject>& Payload, FString& OutAssetPath)
	{
		if (!Payload.IsValid())
		{
			return false;
		}

		return Payload->TryGetStringField(TEXT("asset_path"), OutAssetPath)
			|| Payload->TryGetStringField(TEXT("target_blueprint"), OutAssetPath);
	}
}

bool FBlueprintHelperWriteSessionGrant::IsExpired() const
{
	return SessionId.IsEmpty() || FDateTime::UtcNow() >= ExpiresAtUtc;
}

TSharedRef<FJsonObject> FBlueprintHelperWriteSessionGrant::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("session_id"), SessionId);
	Json->SetStringField(TEXT("scope"), Scope);
	Json->SetStringField(TEXT("expires_at_utc"), ExpiresAtUtc.ToIso8601());

	TArray<TSharedPtr<FJsonValue>> AssetValues;
	for (const FString& AssetPath : AssetPaths)
	{
		AssetValues.Add(MakeShared<FJsonValueString>(AssetPath));
	}
	Json->SetArrayField(TEXT("asset_paths"), AssetValues);
	return Json;
}

FBlueprintHelperWriteAuthorizationService& FBlueprintHelperWriteAuthorizationService::Get()
{
	static FBlueprintHelperWriteAuthorizationService Instance;
	return Instance;
}

TOptional<FBlueprintHelperWriteSessionGrant> FBlueprintHelperWriteAuthorizationService::RequestSession(
	const FBlueprintHelperWriteSessionRequest& Request,
	FString& OutError)
{
	const bool bApproved = ApprovalProviderForTesting
		? ApprovalProviderForTesting(Request)
		: RequestUserApproval(Request);

	if (!bApproved)
	{
		OutError = TEXT("Write session request was denied by the user.");
		return {};
	}

	return CreateGrant(Request);
}

FBlueprintHelperWriteSessionGrant FBlueprintHelperWriteAuthorizationService::CreateApprovedSessionForTesting(
	const FBlueprintHelperWriteSessionRequest& Request)
{
	return CreateGrant(Request);
}

bool FBlueprintHelperWriteAuthorizationService::ValidateSessionForCommand(
	const FString& SessionId,
	const FString& Command,
	const TSharedPtr<FJsonObject>& Payload,
	FBlueprintHelperBridgeValidationError& OutError)
{
	if (SessionId.IsEmpty())
	{
		SetAuthError(OutError, TEXT("unauthorized"),
			TEXT("Write commands require an approved BlueprintHelper auth_session."));
		return false;
	}

	FScopeLock Lock(&Mutex);
	RemoveExpiredSessions();

	const FBlueprintHelperWriteSessionGrant* Grant = Sessions.Find(SessionId);
	if (!Grant)
	{
		SetAuthError(OutError, TEXT("unauthorized"),
			TEXT("auth_session is missing, expired, or not recognized by the running editor."));
		return false;
	}

	if (!GrantCoversPayload(*Grant, Payload))
	{
		SetAuthError(OutError, TEXT("unauthorized"),
			FString::Printf(TEXT("auth_session does not cover command %s target asset."), *Command));
		return false;
	}

	return true;
}

bool FBlueprintHelperWriteAuthorizationService::HasActiveSession()
{
	FScopeLock Lock(&Mutex);
	RemoveExpiredSessions();
	return Sessions.Num() > 0;
}

void FBlueprintHelperWriteAuthorizationService::ResetForTesting()
{
	FScopeLock Lock(&Mutex);
	Sessions.Reset();
	ApprovalProviderForTesting = nullptr;
}

void FBlueprintHelperWriteAuthorizationService::SetApprovalProviderForTesting(
	TFunction<bool(const FBlueprintHelperWriteSessionRequest&)> InProvider)
{
	FScopeLock Lock(&Mutex);
	ApprovalProviderForTesting = MoveTemp(InProvider);
}

FBlueprintHelperWriteSessionGrant FBlueprintHelperWriteAuthorizationService::CreateGrant(
	const FBlueprintHelperWriteSessionRequest& Request)
{
	FBlueprintHelperWriteSessionGrant Grant;
	Grant.SessionId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	Grant.Scope = NormalizeScope(Request.Scope);
	Grant.ExpiresAtUtc = FDateTime::UtcNow() + FTimespan::FromSeconds(ClampTtlSeconds(Request.TtlSeconds));
	Grant.AssetPaths = Request.AssetPaths;

	FScopeLock Lock(&Mutex);
	RemoveExpiredSessions();
	Sessions.Add(Grant.SessionId, Grant);
	return Grant;
}

bool FBlueprintHelperWriteAuthorizationService::RequestUserApproval(
	const FBlueprintHelperWriteSessionRequest& Request) const
{
	const FString Scope = NormalizeScope(Request.Scope);
	const FString Message = FString::Printf(
		TEXT("Allow BlueprintHelper MCP write access?\n\nReason: %s\nScope: %s\nLifetime: %d seconds"),
		Request.Reason.IsEmpty() ? TEXT("(not provided)") : *Request.Reason,
		*Scope,
		ClampTtlSeconds(Request.TtlSeconds));

	return FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(Message)) == EAppReturnType::Yes;
}

bool FBlueprintHelperWriteAuthorizationService::GrantCoversPayload(
	const FBlueprintHelperWriteSessionGrant& Grant,
	const TSharedPtr<FJsonObject>& Payload) const
{
	if (Grant.Scope != TEXT("asset_list"))
	{
		return true;
	}

	FString AssetPath;
	if (!TryReadPayloadAsset(Payload, AssetPath))
	{
		return false;
	}

	return Grant.AssetPaths.Contains(AssetPath);
}

void FBlueprintHelperWriteAuthorizationService::RemoveExpiredSessions()
{
	const FDateTime Now = FDateTime::UtcNow();
	for (auto It = Sessions.CreateIterator(); It; ++It)
	{
		if (It.Value().SessionId.IsEmpty() || Now >= It.Value().ExpiresAtUtc)
		{
			It.RemoveCurrent();
		}
	}
}
