// BlueprintHelper Metrics panel async task utilities implementation.

#include "UI/Metrics/Utils/BlueprintHelperMetricsPanelAsyncUtils.h"

#include "Shared/BlueprintHelperVersionCompat.h"

FCriticalSection FBlueprintHelperMetricsPanelAsyncUtils::TaskCriticalSection;
TArray<TFuture<void>> FBlueprintHelperMetricsPanelAsyncUtils::Tasks;
bool FBlueprintHelperMetricsPanelAsyncUtils::bShutdownRequested = false;

bool FBlueprintHelperMetricsPanelAsyncUtils::IsShutdownRequested()
{
	FScopeLock Lock(&TaskCriticalSection);
	return bShutdownRequested;
}

void FBlueprintHelperMetricsPanelAsyncUtils::TrackTask(TFuture<void>&& Future)
{
	bool bWaitImmediately = false;
	{
		FScopeLock Lock(&TaskCriticalSection);
		for (int32 Index = Tasks.Num() - 1; Index >= 0; --Index)
		{
			if (Tasks[Index].IsReady())
			{
				FBlueprintHelperVersionCompat::RemoveAtSwapNoShrink(Tasks, Index, 1);
			}
		}
		if (bShutdownRequested)
		{
			bWaitImmediately = true;
		}
		else
		{
			Tasks.Add(MoveTemp(Future));
			return;
		}
	}

	if (bWaitImmediately)
	{
		Future.Wait();
	}
}

void FBlueprintHelperMetricsPanelAsyncUtils::FlushTasks()
{
	FlushTasksInternal(false);
}

void FBlueprintHelperMetricsPanelAsyncUtils::ShutdownTasks()
{
	FlushTasksInternal(true);
}

void FBlueprintHelperMetricsPanelAsyncUtils::FlushTasksInternal(bool bShutdown)
{
	TArray<TFuture<void>> PendingTasks;
	{
		FScopeLock Lock(&TaskCriticalSection);
		bShutdownRequested = bShutdownRequested || bShutdown;
		PendingTasks = MoveTemp(Tasks);
	}

	for (TFuture<void>& Task : PendingTasks)
	{
		Task.Wait();
	}
}
