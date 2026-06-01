#pragma once

#include "CoreMinimal.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutPreviewTypes.h"

namespace BlueprintHelper::GraphLayout
{
struct FGraphLayoutPreviewRequest
{
	ESemanticScene Scene = ESemanticScene::LinearExecChain;
	FString RuleSetJson;
};

DECLARE_DELEGATE_OneParam(FGraphLayoutPreviewBuildCompleted, const FGraphLayoutPreviewBuildResult&);

class BLUEPRINTHELPER_API FGraphLayoutPreviewService
{
public:
	FGraphLayoutPreviewService();

	uint64 StartPreviewBuild(
		const FGraphLayoutPreviewRequest& Request,
		FGraphLayoutPreviewBuildCompleted OnCompleted = FGraphLayoutPreviewBuildCompleted());
	void Cancel(uint64 JobId);
	void CancelAll();

	bool BuildPreviewDataForTest(const FGraphLayoutPreviewRequest& Request, FGraphLayoutPreviewBuildResult& OutResult) const;
	bool IsJobCancelledForTest(uint64 JobId) const;

private:
	struct FSharedState
	{
		uint64 NextJobId = 1;
		TSet<uint64> CancelledJobs;
		mutable FCriticalSection CancelledJobsLock;
	};

	static bool IsJobCancelled(const TSharedRef<FSharedState, ESPMode::ThreadSafe>& State, uint64 JobId);
	static bool BuildPreviewData(const FGraphLayoutPreviewRequest& Request, FGraphLayoutPreviewBuildResult& OutResult);

	TSharedRef<FSharedState, ESPMode::ThreadSafe> SharedState;
};
}
