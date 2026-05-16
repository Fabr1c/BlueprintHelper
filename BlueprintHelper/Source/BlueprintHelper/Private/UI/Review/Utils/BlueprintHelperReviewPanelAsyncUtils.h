// BlueprintHelper Review panel async task utilities.

#pragma once

#include "Async/Future.h"
#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"

class FBlueprintHelperReviewPanelAsyncUtils
{
public:
	static bool IsShutdownRequested();
	static void TrackTask(TFuture<void>&& Future);
	static void FlushTasks();
	static void ShutdownTasks();

private:
	static void FlushTasksInternal(bool bShutdown);

	static FCriticalSection TaskCriticalSection;
	static TArray<TFuture<void>> Tasks;
	static bool bShutdownRequested;
};
