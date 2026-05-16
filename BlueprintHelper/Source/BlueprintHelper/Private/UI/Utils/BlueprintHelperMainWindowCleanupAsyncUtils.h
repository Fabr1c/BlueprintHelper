// BlueprintHelper main window cleanup async task utilities.

#pragma once

#include "Async/Future.h"
#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"

class FBlueprintHelperMainWindowCleanupAsyncUtils
{
public:
	static bool IsShutdownRequested();
	static void TrackCleanupTask(TFuture<void>&& Future);
	static void FlushCleanupTasks();
	static void ShutdownCleanupTasks();

private:
	static void FlushCleanupTasksInternal(bool bShutdown);

	static FCriticalSection TaskCriticalSection;
	static TArray<TFuture<void>> CleanupTasks;
	static bool bShutdownRequested;
};
