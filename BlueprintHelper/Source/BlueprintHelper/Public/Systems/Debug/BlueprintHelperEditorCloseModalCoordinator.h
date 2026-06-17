// BlueprintHelper editor close modal recovery coordinator.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"

struct BLUEPRINTHELPER_API FBlueprintHelperEditorModalDismissResult
{
	int32 DismissedCount = 0;
	bool bHadActiveModal = false;
	bool bHasRemainingModal = false;
	FString RemainingModalTitle;
	TArray<FString> DismissedModalTitles;
};

class BLUEPRINTHELPER_API FBlueprintHelperEditorCloseModalCoordinator
{
public:
	static FBlueprintHelperEditorCloseModalCoordinator& Get();

	void Startup();
	void Shutdown();

	void NotifyCloseEditorRequestFromAnyThread(const FString& RequestId);
	int32 PrepareForEditorCloseFromGameThread(const FString& RequestId);
	FBlueprintHelperEditorModalDismissResult DismissActiveModalWindowsFromGameThread(const FString& RequestId);

private:
	void OnModalLoopTick(float DeltaTime);
	bool GetPendingCloseRequest(FString& OutRequestId, double& OutAgeSeconds);
	void ClearPendingCloseRequest();
	FBlueprintHelperEditorModalDismissResult DismissActiveModalWindowsForClose();

	FCriticalSection StateCriticalSection;
	FDelegateHandle ModalLoopTickHandle;
	FString PendingRequestId;
	double PendingRequestTimeSeconds = 0.0;
	bool bPendingCloseRequest = false;
	bool bStarted = false;
	bool bHandlingModalTick = false;

	static constexpr double PendingRequestTtlSeconds = 30.0;
	static constexpr int32 MaxModalWindowsToDismissPerRequest = 16;
};
