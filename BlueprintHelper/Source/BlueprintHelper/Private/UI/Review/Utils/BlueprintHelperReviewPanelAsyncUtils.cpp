// BlueprintHelper Review panel async task utilities implementation.

#include "UI/Review/Utils/BlueprintHelperReviewPanelAsyncUtils.h"

FCriticalSection FBlueprintHelperReviewPanelAsyncUtils::TaskCriticalSection;
TArray<TFuture<void>> FBlueprintHelperReviewPanelAsyncUtils::Tasks;
bool FBlueprintHelperReviewPanelAsyncUtils::bShutdownRequested = false;

bool FBlueprintHelperReviewPanelAsyncUtils::IsShutdownRequested()
{
	FScopeLock Lock(&TaskCriticalSection);
	return bShutdownRequested;
}

void FBlueprintHelperReviewPanelAsyncUtils::TrackTask(TFuture<void>&& Future)
{
	bool bWaitImmediately = false;
	{
		FScopeLock Lock(&TaskCriticalSection);
		for (int32 Index = Tasks.Num() - 1; Index >= 0; --Index)
		{
			if (Tasks[Index].IsReady())
			{
				Tasks.RemoveAtSwap(Index, 1, EAllowShrinking::No);
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

void FBlueprintHelperReviewPanelAsyncUtils::FlushTasks()
{
	FlushTasksInternal(false);
}

void FBlueprintHelperReviewPanelAsyncUtils::ShutdownTasks()
{
	FlushTasksInternal(true);
}

void FBlueprintHelperReviewPanelAsyncUtils::FlushTasksInternal(bool bShutdown)
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
