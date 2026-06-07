// BlueprintHelper TaskRuntime - UMGWidget static cluster.

#include "Runtime/TaskRuntime/Clusters/UMGWidget/BlueprintHelperUMGWidgetTaskRuntimeCluster.h"

#include "Runtime/TaskRuntime/Clusters/UMGWidget/BlueprintHelperWidgetTreeReviewEvidenceBuilder.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/UMGWidget/BlueprintHelperWidgetTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/Utils/BlueprintHelperTaskRuntimeClusterExecutionUtils.h"

FBlueprintHelperUMGWidgetTaskRuntimeCluster::FBlueprintHelperUMGWidgetTaskRuntimeCluster(
	const FBlueprintHelperWidgetService& InWidgetService)
	: WidgetService(InWidgetService)
{
}

bool FBlueprintHelperUMGWidgetTaskRuntimeCluster::CanExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
{
	return LoweredStep.Capability == FBlueprintHelperWidgetTaskPlan::Capability::UMGWidget;
}

bool FBlueprintHelperUMGWidgetTaskRuntimeCluster::BuildReviewEvidence(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
	const FBlueprintHelperToolResultBase& StepResult,
	const FString& ArchiveSessionId,
	const FString& TaskRunId,
	int32 StepIndex,
	FBlueprintHelperWriteReviewEvidence& OutEvidence)
{
	FBlueprintHelperWidgetTreeReviewEvidenceBuildInput Input;
	Input.LoweredStep = LoweredStep;
	Input.StepResult = StepResult;
	Input.ArchiveSessionId = ArchiveSessionId;
	Input.TaskRunId = TaskRunId;
	Input.StepIndex = StepIndex;
	return FBlueprintHelperWidgetTreeReviewEvidenceBuilder::Build(Input, OutEvidence);
}

FBlueprintHelperToolResultBase FBlueprintHelperUMGWidgetTaskRuntimeCluster::ExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) const
{
	return FBlueprintHelperTaskRuntimeClusterExecutionUtils::ExecuteWidgetTaskPlanStep(
		WidgetService,
		LoweredStep.AdapterOperation,
		LoweredStep.Payload);
}
