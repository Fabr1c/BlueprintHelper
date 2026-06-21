#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewService.h"

#include "Async/Async.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewOverlayProjector.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSampleFactory.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewSolverInput.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutRuleSetJson.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutSolver.h"

namespace BlueprintHelper::GraphLayout
{
FGraphLayoutPreviewService::FGraphLayoutPreviewService()
	: SharedState(MakeShared<FSharedState, ESPMode::ThreadSafe>())
{
}

uint64 FGraphLayoutPreviewService::StartPreviewBuild(
	const FGraphLayoutPreviewRequest& Request,
	FGraphLayoutPreviewBuildCompleted OnCompleted)
{
	uint64 JobId = 0;
	{
		FScopeLock Lock(&SharedState->CancelledJobsLock);
		JobId = SharedState->NextJobId++;
	}

	FGraphLayoutPreviewRequest RequestCopy = Request;
	TSharedRef<FSharedState, ESPMode::ThreadSafe> State = SharedState;
	Async(EAsyncExecution::ThreadPool, [State, JobId, RequestCopy, OnCompleted]() mutable
	{
		FGraphLayoutPreviewBuildResult Result;
		Result.JobId = JobId;
		FGraphLayoutPreviewService::BuildPreviewData(RequestCopy, Result);

		AsyncTask(ENamedThreads::GameThread, [State, JobId, Result, OnCompleted]() mutable
		{
			if (FGraphLayoutPreviewService::IsJobCancelled(State, JobId))
			{
				return;
			}

			if (OnCompleted.IsBound())
			{
				OnCompleted.Execute(Result);
			}
		});
	});
	return JobId;
}

void FGraphLayoutPreviewService::Cancel(const uint64 JobId)
{
	FScopeLock Lock(&SharedState->CancelledJobsLock);
	SharedState->CancelledJobs.Add(JobId);
}

void FGraphLayoutPreviewService::CancelAll()
{
	FScopeLock Lock(&SharedState->CancelledJobsLock);
	for (uint64 JobId = 1; JobId < SharedState->NextJobId; ++JobId)
	{
		SharedState->CancelledJobs.Add(JobId);
	}
}

bool FGraphLayoutPreviewService::BuildPreviewDataForTest(
	const FGraphLayoutPreviewRequest& Request,
	FGraphLayoutPreviewBuildResult& OutResult) const
{
	return BuildPreviewData(Request, OutResult);
}

bool FGraphLayoutPreviewService::BuildPreviewData(
	const FGraphLayoutPreviewRequest& Request,
	FGraphLayoutPreviewBuildResult& OutResult)
{
	OutResult = FGraphLayoutPreviewBuildResult();

	FRuleSet RuleSet;
	FValidationResult Validation;
	if (!FRuleSetJson::ImportString(Request.RuleSetJson, RuleSet, Validation))
	{
		OutResult.bSuccess = false;
		OutResult.Error = Validation.Errors.Num() > 0
			? FString::Join(Validation.Errors, TEXT(" "))
			: TEXT("Preview RuleSet JSON import failed.");
		return false;
	}

	FString SampleError;
	if (!FGraphLayoutPreviewSampleFactory::BuildSample(Request.Scene, OutResult.Sample, SampleError))
	{
		OutResult.bSuccess = false;
		OutResult.Error = SampleError.IsEmpty() ? TEXT("Preview sample build failed.") : SampleError;
		return false;
	}

	const FGraphSnapshot SolverSnapshot = FGraphLayoutPreviewSolverInput::BuildSolverSnapshot(OutResult.Sample);
	OutResult.LayoutPlan = FSolver::Solve(SolverSnapshot, RuleSet);
	FGraphLayoutPreviewOverlayProjector::AppendOverlays(OutResult.Sample, RuleSet, OutResult.LayoutPlan);

	if (OutResult.LayoutPlan.Placements.Num() == 0)
	{
		OutResult.bSuccess = false;
		OutResult.Error = TEXT("Preview layout produced no placements.");
		return false;
	}

	OutResult.bSuccess = true;
	return true;
}

bool FGraphLayoutPreviewService::IsJobCancelledForTest(const uint64 JobId) const
{
	return IsJobCancelled(SharedState, JobId);
}

bool FGraphLayoutPreviewService::IsJobCancelled(
	const TSharedRef<FSharedState, ESPMode::ThreadSafe>& State,
	const uint64 JobId)
{
	FScopeLock Lock(&State->CancelledJobsLock);
	return State->CancelledJobs.Contains(JobId);
}
}
