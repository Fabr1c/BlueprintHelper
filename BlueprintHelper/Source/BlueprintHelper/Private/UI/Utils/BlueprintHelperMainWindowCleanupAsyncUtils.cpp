// BlueprintHelper main window cleanup async task utilities implementation.

#include "UI/Utils/BlueprintHelperMainWindowCleanupAsyncUtils.h"

FCriticalSection FBlueprintHelperMainWindowCleanupAsyncUtils::TaskCriticalSection;
TArray<TFuture<void>> FBlueprintHelperMainWindowCleanupAsyncUtils::CleanupTasks;
bool FBlueprintHelperMainWindowCleanupAsyncUtils::bShutdownRequested = false;

bool FBlueprintHelperMainWindowCleanupAsyncUtils::IsShutdownRequested()
{
	FScopeLock Lock(&TaskCriticalSection);
	return bShutdownRequested;
}

void FBlueprintHelperMainWindowCleanupAsyncUtils::TrackCleanupTask(TFuture<void>&& Future)
{
	bool bWaitImmediately = false;
	{
		FScopeLock Lock(&TaskCriticalSection);
		for (int32 Index = CleanupTasks.Num() - 1; Index >= 0; --Index)
		{
			if (CleanupTasks[Index].IsReady())
			{
				CleanupTasks.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			}
		}
		if (bShutdownRequested)
		{
			bWaitImmediately = true;
		}
		else
		{
			CleanupTasks.Add(MoveTemp(Future));
			return;
		}
	}
	if (bWaitImmediately)
	{
		Future.Wait();
	}
}

void FBlueprintHelperMainWindowCleanupAsyncUtils::FlushCleanupTasks()
{
	FlushCleanupTasksInternal(false);
}

void FBlueprintHelperMainWindowCleanupAsyncUtils::ShutdownCleanupTasks()
{
	FlushCleanupTasksInternal(true);
}

void FBlueprintHelperMainWindowCleanupAsyncUtils::FlushCleanupTasksInternal(bool bShutdown)
{
	TArray<TFuture<void>> Tasks;
	{
		FScopeLock Lock(&TaskCriticalSection);
		bShutdownRequested = bShutdownRequested || bShutdown;
		Tasks = MoveTemp(CleanupTasks);
	}
	for (TFuture<void>& Task : Tasks)
	{
		Task.Wait();
	}
}
