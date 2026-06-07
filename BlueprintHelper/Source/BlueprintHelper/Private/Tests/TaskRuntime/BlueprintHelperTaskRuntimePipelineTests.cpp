#include "Runtime/TaskRuntime/Pipeline/BlueprintHelperTaskRuntimePipeline.h"

#include "Dom/JsonObject.h"
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
	TestEqual(TEXT("first stage validates compiled plan"),
		static_cast<int32>(Stages[0]),
		static_cast<int32>(EBlueprintHelperTaskRuntimePipelineStage::ValidateCompiledPlanContract));
	TestEqual(TEXT("final stage finalizes bridge response"),
		static_cast<int32>(Stages.Last()),
		static_cast<int32>(EBlueprintHelperTaskRuntimePipelineStage::FinalizeBridgeResponse));

	FBlueprintHelperTaskRuntimePipelineContext Context;
	for (const EBlueprintHelperTaskRuntimePipelineStage Stage : Stages)
	{
		Context.RecordStage(Stage);
	}

	TestTrue(TEXT("trace records execute stage"),
		Context.HasStage(EBlueprintHelperTaskRuntimePipelineStage::ExecuteCluster));
	TestTrue(TEXT("trace records review evidence stage"),
		Context.HasStage(EBlueprintHelperTaskRuntimePipelineStage::BuildReviewEvidence));
	TestEqual(TEXT("trace name is stable"),
		Context.GetStageTrace()[0].Name,
		FString(TEXT("validate_compiled_plan_contract")));

	FBlueprintHelperTaskRuntimePipelineRunner Runner(
		MakeShared<FJsonObject>(),
		TEXT("task_run_pipeline_test"),
		true);
	Runner.RecordStageOnce(EBlueprintHelperTaskRuntimePipelineStage::ValidateCompiledPlanContract);
	Runner.RecordStageOnce(EBlueprintHelperTaskRuntimePipelineStage::ValidateCompiledPlanContract);
	Runner.RecordStageOnce(EBlueprintHelperTaskRuntimePipelineStage::FinalizeBridgeResponse);
	TestTrue(
		TEXT("runner owns recorded stages"),
		Runner.HasStage(EBlueprintHelperTaskRuntimePipelineStage::FinalizeBridgeResponse));
	TestEqual(
		TEXT("runner records stages once"),
		Runner.GetContext().GetStageTrace().Num(),
		2);
	return true;
}

#endif
