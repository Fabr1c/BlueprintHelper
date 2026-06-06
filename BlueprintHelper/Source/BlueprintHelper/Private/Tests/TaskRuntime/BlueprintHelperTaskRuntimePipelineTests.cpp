#include "Runtime/TaskRuntime/Pipeline/BlueprintHelperTaskRuntimePipeline.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimePipeline_DefaultStageOrderIsStable,
	"BlueprintHelper.TaskRuntime.Pipeline.DefaultStageOrderIsStable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimePipeline_DefaultStageOrderIsStable::RunTest(const FString& Parameters)
{
	const TArray<EBlueprintHelperTaskRuntimePipelineStage> Stages =
		FBlueprintHelperTaskRuntimePipeline::GetDefaultStageOrder();

	TestEqual(TEXT("default stage count"), Stages.Num(), 9);
	TestEqual(TEXT("first stage is prepare"),
		static_cast<int32>(Stages[0]),
		static_cast<int32>(EBlueprintHelperTaskRuntimePipelineStage::Prepare));
	TestEqual(TEXT("final stage is finalize_result"),
		static_cast<int32>(Stages.Last()),
		static_cast<int32>(EBlueprintHelperTaskRuntimePipelineStage::FinalizeResult));

	FBlueprintHelperTaskRuntimePipelineContext Context;
	for (const EBlueprintHelperTaskRuntimePipelineStage Stage : Stages)
	{
		Context.RecordStage(Stage);
	}

	TestTrue(TEXT("trace records execute stage"),
		Context.HasStage(EBlueprintHelperTaskRuntimePipelineStage::ExecuteSteps));
	TestTrue(TEXT("trace records review evidence stage"),
		Context.HasStage(EBlueprintHelperTaskRuntimePipelineStage::BuildReviewEvidence));
	TestEqual(TEXT("trace name is stable"),
		Context.GetStageTrace()[0].Name,
		FString(TEXT("prepare")));
	return true;
}

#endif
