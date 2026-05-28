#include "Systems/Authorization/Utils/BlueprintHelperAuthorizationUtils.h"

#include "Shared/Bridge/BlueprintHelperBridgeTypes.h"
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

int32 UBlueprintHelperAuthorizationUtils::ClampTtlSeconds(int32 TtlSeconds)
{
	return FMath::Clamp(TtlSeconds <= 0 ? 900 : TtlSeconds, 1, 3600);
}

FString UBlueprintHelperAuthorizationUtils::NormalizeScope(const FString& Scope)
{
	return Scope.Equals(TEXT("asset_list"), ESearchCase::IgnoreCase)
		? FString(TEXT("asset_list"))
		: FString(TEXT("project"));
}

void UBlueprintHelperAuthorizationUtils::SetAuthError(
	FBlueprintHelperBridgeValidationError& OutError,
	const FString& Code,
	const FString& Message)
{
	OutError.Code = Code;
	OutError.Field = TEXT("auth_session");
	OutError.Message = Message;
}

void UBlueprintHelperAuthorizationUtils::AddAssetPathIfPresent(TArray<FString>& OutAssetPaths, const FString& AssetPath)
{
	if (!AssetPath.IsEmpty())
	{
		OutAssetPaths.AddUnique(AssetPath);
	}
}

void UBlueprintHelperAuthorizationUtils::ReadAssetPathFields(const TSharedPtr<FJsonObject>& Json, TArray<FString>& OutAssetPaths)
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

void UBlueprintHelperAuthorizationUtils::ReadTargetAssetsArray(const TSharedPtr<FJsonObject>& Json, TArray<FString>& OutAssetPaths)
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

bool UBlueprintHelperAuthorizationUtils::TryReadPayloadAssets(const TSharedPtr<FJsonObject>& Payload, TArray<FString>& OutAssetPaths)
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

FString UBlueprintHelperAuthorizationUtils::BuildWriteApprovalSummary(const FBlueprintHelperWriteSessionRequest& Request, const FString& Scope)
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

void UBlueprintHelperAuthorizationUtils::ShowWriteApprovalEditorNotification(const FBlueprintHelperWriteSessionRequest& Request, const FString& Scope)
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
void UBlueprintHelperAuthorizationUtils::FlashEditorWindowForWriteApproval()
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

void UBlueprintHelperAuthorizationUtils::NotifyUserAboutPendingWriteApproval(const FBlueprintHelperWriteSessionRequest& Request, const FString& Scope)
{
	ShowWriteApprovalEditorNotification(Request, Scope);

#if PLATFORM_WINDOWS
	FlashEditorWindowForWriteApproval();
#endif
}
