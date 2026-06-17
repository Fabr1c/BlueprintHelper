// BlueprintHelper editor close modal recovery coordinator.

#include "Systems/Debug/BlueprintHelperEditorCloseModalCoordinator.h"

#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "Widgets/SWindow.h"

FBlueprintHelperEditorCloseModalCoordinator& FBlueprintHelperEditorCloseModalCoordinator::Get()
{
	static FBlueprintHelperEditorCloseModalCoordinator Instance;
	return Instance;
}

void FBlueprintHelperEditorCloseModalCoordinator::Startup()
{
	if (bStarted)
	{
		return;
	}

	if (FSlateApplication::IsInitialized())
	{
		ModalLoopTickHandle = FSlateApplication::Get().GetOnModalLoopTickEvent().AddRaw(
			this,
			&FBlueprintHelperEditorCloseModalCoordinator::OnModalLoopTick);
		bStarted = true;
	}
}

void FBlueprintHelperEditorCloseModalCoordinator::Shutdown()
{
	if (bStarted && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetOnModalLoopTickEvent().Remove(ModalLoopTickHandle);
	}

	ModalLoopTickHandle.Reset();
	bStarted = false;
	bHandlingModalTick = false;

	FScopeLock Lock(&StateCriticalSection);
	bPendingCloseRequest = false;
	PendingRequestId.Reset();
	PendingRequestTimeSeconds = 0.0;
}

void FBlueprintHelperEditorCloseModalCoordinator::NotifyCloseEditorRequestFromAnyThread(const FString& RequestId)
{
	FScopeLock Lock(&StateCriticalSection);
	bPendingCloseRequest = true;
	PendingRequestId = RequestId;
	PendingRequestTimeSeconds = FPlatformTime::Seconds();
}

int32 FBlueprintHelperEditorCloseModalCoordinator::PrepareForEditorCloseFromGameThread(const FString& RequestId)
{
	const FBlueprintHelperEditorModalDismissResult DismissResult = DismissActiveModalWindowsFromGameThread(RequestId);
	if (DismissResult.DismissedCount > 0)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[BlueprintHelper] close_editor request %s closed %d blocking modal window(s) before main frame shutdown."),
			RequestId.IsEmpty() ? TEXT("<unknown>") : *RequestId,
			DismissResult.DismissedCount);
	}
	return DismissResult.DismissedCount;
}

FBlueprintHelperEditorModalDismissResult FBlueprintHelperEditorCloseModalCoordinator::DismissActiveModalWindowsFromGameThread(
	const FString& RequestId)
{
	ClearPendingCloseRequest();
	FBlueprintHelperEditorModalDismissResult DismissResult = DismissActiveModalWindowsForClose();
	if (DismissResult.DismissedCount > 0)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[BlueprintHelper] editor modal dismiss request %s closed %d modal window(s)."),
			RequestId.IsEmpty() ? TEXT("<unknown>") : *RequestId,
			DismissResult.DismissedCount);
	}
	if (DismissResult.bHasRemainingModal)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[BlueprintHelper] editor modal dismiss request %s still has active modal window \"%s\"."),
			RequestId.IsEmpty() ? TEXT("<unknown>") : *RequestId,
			DismissResult.RemainingModalTitle.IsEmpty() ? TEXT("<untitled>") : *DismissResult.RemainingModalTitle);
	}
	return DismissResult;
}

void FBlueprintHelperEditorCloseModalCoordinator::OnModalLoopTick(float DeltaTime)
{
	if (bHandlingModalTick)
	{
		return;
	}

	FString RequestId;
	double RequestAgeSeconds = 0.0;
	if (!GetPendingCloseRequest(RequestId, RequestAgeSeconds))
	{
		return;
	}

	if (RequestAgeSeconds > PendingRequestTtlSeconds)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[BlueprintHelper] close_editor modal recovery request %s expired after %.2f seconds."),
			RequestId.IsEmpty() ? TEXT("<unknown>") : *RequestId,
			RequestAgeSeconds);
		ClearPendingCloseRequest();
		return;
	}

	TGuardValue<bool> HandlingGuard(bHandlingModalTick, true);
	const FBlueprintHelperEditorModalDismissResult DismissResult = DismissActiveModalWindowsForClose();
	if (DismissResult.DismissedCount > 0)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[BlueprintHelper] close_editor request %s closed %d active modal window(s)."),
			RequestId.IsEmpty() ? TEXT("<unknown>") : *RequestId,
			DismissResult.DismissedCount);
		ClearPendingCloseRequest();
	}
}

bool FBlueprintHelperEditorCloseModalCoordinator::GetPendingCloseRequest(FString& OutRequestId, double& OutAgeSeconds)
{
	FScopeLock Lock(&StateCriticalSection);
	if (!bPendingCloseRequest)
	{
		return false;
	}

	OutRequestId = PendingRequestId;
	OutAgeSeconds = FPlatformTime::Seconds() - PendingRequestTimeSeconds;
	return true;
}

void FBlueprintHelperEditorCloseModalCoordinator::ClearPendingCloseRequest()
{
	FScopeLock Lock(&StateCriticalSection);
	bPendingCloseRequest = false;
	PendingRequestId.Reset();
	PendingRequestTimeSeconds = 0.0;
}

FBlueprintHelperEditorModalDismissResult FBlueprintHelperEditorCloseModalCoordinator::DismissActiveModalWindowsForClose()
{
	FBlueprintHelperEditorModalDismissResult Result;
	if (!FSlateApplication::IsInitialized())
	{
		return Result;
	}

	FSlateApplication& SlateApplication = FSlateApplication::Get();
	SlateApplication.DismissAllMenus();

	while (Result.DismissedCount < MaxModalWindowsToDismissPerRequest)
	{
		TSharedPtr<SWindow> ActiveModalWindow = SlateApplication.GetActiveModalWindow();
		if (!ActiveModalWindow.IsValid())
		{
			break;
		}

		Result.bHadActiveModal = true;
		const FString ModalTitle = ActiveModalWindow->GetTitle().ToString();
		Result.DismissedModalTitles.Add(ModalTitle);
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[BlueprintHelper] close_editor modal recovery requesting close for modal window \"%s\"."),
			ModalTitle.IsEmpty() ? TEXT("<untitled>") : *ModalTitle);
		SlateApplication.DestroyWindowImmediately(ActiveModalWindow.ToSharedRef());
		++Result.DismissedCount;
	}

	const TSharedPtr<SWindow> RemainingModalWindow = SlateApplication.GetActiveModalWindow();
	if (RemainingModalWindow.IsValid())
	{
		Result.bHasRemainingModal = true;
		Result.RemainingModalTitle = RemainingModalWindow->GetTitle().ToString();
		if (Result.DismissedCount >= MaxModalWindowsToDismissPerRequest)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[BlueprintHelper] close_editor modal recovery reached the safety limit; another active modal window remains."));
		}
	}

	return Result;
}
