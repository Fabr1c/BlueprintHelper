// BlueprintHelper write authorization service.

#include "Systems/Authorization/BlueprintHelperWriteAuthorizationService.h"

#include "Entry/Bridge/BlueprintHelperRequestValidator.h"
#include "Systems/Config/BlueprintHelperSafetyProfileResolver.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GenericPlatform/GenericWindow.h"
#include "HAL/PlatformProcess.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SWindow.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/PreWindowsApi.h"
#include <Windows.h>
#include "Windows/PostWindowsApi.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif

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

	void AddAssetPathIfPresent(TArray<FString>& OutAssetPaths, const FString& AssetPath)
	{
		if (!AssetPath.IsEmpty())
		{
			OutAssetPaths.AddUnique(AssetPath);
		}
	}

	void ReadAssetPathFields(const TSharedPtr<FJsonObject>& Json, TArray<FString>& OutAssetPaths)
	{
		if (!Json.IsValid())
		{
			return;
		}

		FString AssetPath;
		if (Json->TryGetStringField(TEXT("asset_path"), AssetPath))
		{
			AddAssetPathIfPresent(OutAssetPaths, AssetPath);
		}
		if (Json->TryGetStringField(TEXT("target_blueprint"), AssetPath))
		{
			AddAssetPathIfPresent(OutAssetPaths, AssetPath);
		}

		const TSharedPtr<FJsonObject>* TargetObjectPtr = nullptr;
		if (Json->TryGetObjectField(TEXT("target"), TargetObjectPtr) &&
			TargetObjectPtr &&
			TargetObjectPtr->IsValid() &&
			(*TargetObjectPtr)->TryGetStringField(TEXT("asset_path"), AssetPath))
		{
			AddAssetPathIfPresent(OutAssetPaths, AssetPath);
		}
	}

	void ReadTargetAssetsArray(const TSharedPtr<FJsonObject>& Json, TArray<FString>& OutAssetPaths)
	{
		if (!Json.IsValid())
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* TargetAssets = nullptr;
		if (!Json->TryGetArrayField(TEXT("target_assets"), TargetAssets) || !TargetAssets)
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& Value : *TargetAssets)
		{
			FString AssetPath;
			if (Value.IsValid() && Value->TryGetString(AssetPath))
			{
				AddAssetPathIfPresent(OutAssetPaths, AssetPath);
			}
		}
	}

	bool TryReadPayloadAssets(const TSharedPtr<FJsonObject>& Payload, TArray<FString>& OutAssetPaths)
	{
		if (!Payload.IsValid())
		{
			return false;
		}

		ReadAssetPathFields(Payload, OutAssetPaths);
		ReadTargetAssetsArray(Payload, OutAssetPaths);

		const TSharedPtr<FJsonObject>* TaskPlanPtr = nullptr;
		if (Payload->TryGetObjectField(TEXT("task_plan"), TaskPlanPtr) &&
			TaskPlanPtr &&
			TaskPlanPtr->IsValid())
		{
			ReadAssetPathFields(*TaskPlanPtr, OutAssetPaths);
			ReadTargetAssetsArray(*TaskPlanPtr, OutAssetPaths);
		}

		return OutAssetPaths.Num() > 0;
	}

	FString BuildWriteApprovalSummary(const FBlueprintHelperWriteSessionRequest& Request, const FString& Scope)
	{
		const FString Reason = Request.Reason.IsEmpty() ? FString(TEXT("(not provided)")) : Request.Reason;
		if (Request.AssetPaths.Num() > 0)
		{
			return FString::Printf(
				TEXT("Reason: %s | Scope: %s | Asset: %s"),
				*Reason,
				*Scope,
				*Request.AssetPaths[0]);
		}

		return FString::Printf(
			TEXT("Reason: %s | Scope: %s"),
			*Reason,
			*Scope);
	}

	void ShowWriteApprovalEditorNotification(const FBlueprintHelperWriteSessionRequest& Request, const FString& Scope)
	{
		if (!FSlateApplication::IsInitialized())
		{
			return;
		}

		FNotificationInfo Info(FText::FromString(FString::Printf(
			TEXT("BlueprintHelper is waiting for write approval. %s"),
			*BuildWriteApprovalSummary(Request, Scope))));
		Info.bFireAndForget = true;
		Info.bUseLargeFont = false;
		Info.bUseThrobber = true;
		Info.bUseSuccessFailIcons = false;
		Info.FadeInDuration = 0.1f;
		Info.FadeOutDuration = 0.5f;
		Info.ExpireDuration = 12.0f;

		FSlateNotificationManager::Get().AddNotification(Info);
	}

#if PLATFORM_WINDOWS
	void FlashEditorWindowForWriteApproval()
	{
		MessageBeep(MB_ICONEXCLAMATION);

		if (!FSlateApplication::IsInitialized())
		{
			return;
		}

		const TSharedPtr<SWindow> ActiveWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
		if (!ActiveWindow.IsValid() || !ActiveWindow->GetNativeWindow().IsValid())
		{
			return;
		}

		void* WindowHandle = ActiveWindow->GetNativeWindow()->GetOSWindowHandle();
		if (!WindowHandle)
		{
			return;
		}

		FLASHWINFO FlashInfo = {};
		FlashInfo.cbSize = sizeof(FLASHWINFO);
		FlashInfo.hwnd = static_cast<HWND>(WindowHandle);
		FlashInfo.dwFlags = FLASHW_TRAY | FLASHW_TIMERNOFG;
		FlashInfo.uCount = 5;
		FlashInfo.dwTimeout = 0;
		FlashWindowEx(&FlashInfo);
	}
#endif

	void NotifyUserAboutPendingWriteApproval(const FBlueprintHelperWriteSessionRequest& Request, const FString& Scope)
	{
		ShowWriteApprovalEditorNotification(Request, Scope);

#if PLATFORM_WINDOWS
		FlashEditorWindowForWriteApproval();
#endif
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
	if (FBlueprintHelperSafetyProfileResolver::IsAutoRepair())
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
		SetAuthError(OutError, TEXT("unauthorized"),
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
		SetAuthError(OutError, TEXT("unauthorized"),
			TEXT("auth_session is missing, expired, or not recognized by the running editor."));
		return false;
	}

	if (!SessionId.IsEmpty())
	{
		SetAuthError(OutError, TEXT("unauthorized"),
			FString::Printf(TEXT("auth_session does not cover command %s target asset."), *Command));
		return false;
	}

	SetAuthError(OutError, TEXT("unauthorized"),
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
	NotifyUserAboutPendingWriteApproval(Request, Scope);

	const FString Message = FString::Printf(
		TEXT("Allow BlueprintHelper write access?\n\nReason: %s\nScope: %s\nLifetime: %d seconds"),
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

	TArray<FString> AssetPaths;
	if (!TryReadPayloadAssets(Payload, AssetPaths))
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
