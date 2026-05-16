// BlueprintHelper Review debug bundle utility functions implementation.

#include "UI/Review/Utils/BlueprintHelperReviewDebugBundleUtils.h"

FCriticalSection FBlueprintHelperReviewDebugBundleUtils::WriteCriticalSection;
FCriticalSection FBlueprintHelperReviewDebugBundleUtils::TaskCriticalSection;
TArray<TFuture<void>> FBlueprintHelperReviewDebugBundleUtils::WriteTasks;
bool FBlueprintHelperReviewDebugBundleUtils::bShutdownRequested = false;

FCriticalSection& FBlueprintHelperReviewDebugBundleUtils::GetWriteCriticalSection()
{
	return WriteCriticalSection;
}

bool FBlueprintHelperReviewDebugBundleUtils::IsShutdownRequested()
{
	FScopeLock Lock(&TaskCriticalSection);
	return bShutdownRequested;
}

void FBlueprintHelperReviewDebugBundleUtils::TrackWriteTask(TFuture<void>&& Future)
{
	bool bWaitImmediately = false;
	{
		FScopeLock Lock(&TaskCriticalSection);
		for (int32 Index = WriteTasks.Num() - 1; Index >= 0; --Index)
		{
			if (WriteTasks[Index].IsReady())
			{
				WriteTasks.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			}
		}
		if (bShutdownRequested)
		{
			bWaitImmediately = true;
		}
		else
		{
			WriteTasks.Add(MoveTemp(Future));
			return;
		}
	}
	if (bWaitImmediately)
	{
		Future.Wait();
	}
}

void FBlueprintHelperReviewDebugBundleUtils::FlushWriteTasks()
{
	FlushWriteTasksInternal(false);
}

void FBlueprintHelperReviewDebugBundleUtils::ShutdownWriteTasks()
{
	FlushWriteTasksInternal(true);
}

FString FBlueprintHelperReviewDebugBundleUtils::SanitizeText(FString Text)
{
	Text.ReplaceInline(TEXT("\r\n"), TEXT("\\n"), ESearchCase::CaseSensitive);
	Text.ReplaceInline(TEXT("\r"), TEXT("\\n"), ESearchCase::CaseSensitive);
	Text.ReplaceInline(TEXT("\n"), TEXT("\\n"), ESearchCase::CaseSensitive);
	return Text;
}

void FBlueprintHelperReviewDebugBundleUtils::FlushWriteTasksInternal(bool bShutdown)
{
	TArray<TFuture<void>> Tasks;
	{
		FScopeLock Lock(&TaskCriticalSection);
		bShutdownRequested = bShutdownRequested || bShutdown;
		Tasks = MoveTemp(WriteTasks);
	}
	for (TFuture<void>& Task : Tasks)
	{
		Task.Wait();
	}
}
