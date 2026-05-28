// BlueprintHelper write authorization service.

#include "Systems/Authorization/BlueprintHelperWriteAuthorizationService.h"
#include "Systems/Authorization/Utils/BlueprintHelperAuthorizationUtils.h"

#include "Shared/Bridge/BlueprintHelperBridgeTypes.h"
#include "Systems/Config/BlueprintHelperSafetyProfileResolver.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GenericPlatform/GenericWindow.h"
#include "HAL/PlatformProcess.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SWindow.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif


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
	if (FBlueprintHelperSafetyProfileResolver::IsApprovalBypassEnabled())
	{
		return CreateGrant(Request);
	}

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
	FScopeLock Lock(&Mutex);
	RemoveExpiredSessions();

	if (Sessions.Num() == 0)
	{
		UBlueprintHelperAuthorizationUtils::SetAuthError(OutError, TEXT("unauthorized"),
			TEXT("Write commands require an approved BlueprintHelper write session."));
		return false;
	}

	const FBlueprintHelperWriteSessionGrant* Grant = SessionId.IsEmpty() ? nullptr : Sessions.Find(SessionId);
	if (Grant && GrantCoversPayload(*Grant, Payload))
	{
		return true;
	}

	if (FindCoveringGrantLocked(Payload))
	{
		return true;
	}

	if (!SessionId.IsEmpty() && !Grant)
	{
		UBlueprintHelperAuthorizationUtils::SetAuthError(OutError, TEXT("unauthorized"),
			TEXT("auth_session is missing, expired, or not recognized by the running editor."));
		return false;
	}

	if (!SessionId.IsEmpty())
	{
		UBlueprintHelperAuthorizationUtils::SetAuthError(OutError, TEXT("unauthorized"),
			FString::Printf(TEXT("auth_session does not cover command %s target asset."), *Command));
		return false;
	}

	UBlueprintHelperAuthorizationUtils::SetAuthError(OutError, TEXT("unauthorized"),
		FString::Printf(TEXT("No active BlueprintHelper write session covers command %s target asset."), *Command));
	return false;
}

const FBlueprintHelperWriteSessionGrant* FBlueprintHelperWriteAuthorizationService::FindCoveringGrantLocked(
	const TSharedPtr<FJsonObject>& Payload) const
{
	for (const auto& Pair : Sessions)
	{
		const FBlueprintHelperWriteSessionGrant& Grant = Pair.Value;
		if (!Grant.IsExpired() && GrantCoversPayload(Grant, Payload))
		{
			return &Grant;
		}
	}

	return nullptr;
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
	Grant.Scope = UBlueprintHelperAuthorizationUtils::NormalizeScope(Request.Scope);
	Grant.ExpiresAtUtc = FDateTime::UtcNow() + FTimespan::FromSeconds(UBlueprintHelperAuthorizationUtils::ClampTtlSeconds(Request.TtlSeconds));
	Grant.AssetPaths = Request.AssetPaths;

	FScopeLock Lock(&Mutex);
	RemoveExpiredSessions();
	Sessions.Add(Grant.SessionId, Grant);
	return Grant;
}

bool FBlueprintHelperWriteAuthorizationService::RequestUserApproval(
	const FBlueprintHelperWriteSessionRequest& Request) const
{
	const FString Scope = UBlueprintHelperAuthorizationUtils::NormalizeScope(Request.Scope);
	UBlueprintHelperAuthorizationUtils::NotifyUserAboutPendingWriteApproval(Request, Scope);

	const FString Message = FString::Printf(
		TEXT("Allow BlueprintHelper write access?\n\nReason: %s\nScope: %s\nLifetime: %d seconds"),
		Request.Reason.IsEmpty() ? TEXT("(not provided)") : *Request.Reason,
		*Scope,
		UBlueprintHelperAuthorizationUtils::ClampTtlSeconds(Request.TtlSeconds));

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

	TArray<FString> AssetPaths;
	if (!UBlueprintHelperAuthorizationUtils::TryReadPayloadAssets(Payload, AssetPaths))
	{
		return false;
	}

	for (const FString& AssetPath : AssetPaths)
	{
		if (!Grant.AssetPaths.Contains(AssetPath))
		{
			return false;
		}
	}

	return true;
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
