// BlueprintHelper Review debug bundle utility functions.

#pragma once

#include "Async/Future.h"
#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"

class FBlueprintHelperReviewDebugBundleUtils
{
public:
	static FCriticalSection& GetWriteCriticalSection();
	static bool IsShutdownRequested();
	static void TrackWriteTask(TFuture<void>&& Future);
	static void FlushWriteTasks();
	static void ShutdownWriteTasks();
	static FString SanitizeText(FString Text);

private:
	static void FlushWriteTasksInternal(bool bShutdown);

	static FCriticalSection WriteCriticalSection;
	static FCriticalSection TaskCriticalSection;
	static TArray<TFuture<void>> WriteTasks;
	static bool bShutdownRequested;
};
