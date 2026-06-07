#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Runtime/TaskRuntime/Pipeline/BlueprintHelperTaskRuntimePipeline.h"
#include "Runtime/TaskRuntime/Projection/BlueprintHelperTaskRuntimeResultProjection.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeResultProjectionAttachesPipelineTraceTest,
	"BlueprintHelper.TaskRuntime.ResultProjection.AttachesPipelineTrace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskRuntimeResultProjectionAttachesPipelineTraceTest::RunTest(const FString&)
{
	FBlueprintHelperTaskRuntimePipelineContext Context;
	Context.RecordStage(EBlueprintHelperTaskRuntimePipelineStage::ValidateCompiledPlanContract);
	Context.RecordStage(EBlueprintHelperTaskRuntimePipelineStage::ProjectMetricsAndResult);
	Context.RecordStage(EBlueprintHelperTaskRuntimePipelineStage::FinalizeBridgeResponse);

	TSharedPtr<FJsonObject> Data;
	FBlueprintHelperTaskRuntimeResultProjection::AttachPipelineTrace(Data, Context);

	TestTrue(TEXT("projection creates result data"), Data.IsValid());
	if (!Data.IsValid())
	{
		return false;
	}

	FString Owner;
	TestTrue(TEXT("projection owner is attached"), Data->TryGetStringField(TEXT("task_runtime_pipeline_owner"), Owner));
	TestEqual(TEXT("projection owner is stable"), Owner, FString(TEXT("TaskRuntimePipeline")));

	const TArray<TSharedPtr<FJsonValue>>* StageTrace = nullptr;
	TestTrue(
		TEXT("stage trace is attached"),
		Data->TryGetArrayField(TEXT("task_runtime_pipeline_stage_trace"), StageTrace) && StageTrace);
	TestEqual(TEXT("stage trace count"), StageTrace ? StageTrace->Num() : 0, 3);
	if (!StageTrace || StageTrace->Num() != 3)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> FirstStage = (*StageTrace)[0]->AsObject();
	const TSharedPtr<FJsonObject> MetricsStage = (*StageTrace)[1]->AsObject();
	const TSharedPtr<FJsonObject> FinalStage = (*StageTrace)[2]->AsObject();
	TestTrue(TEXT("first stage object exists"), FirstStage.IsValid());
	TestTrue(TEXT("metrics stage object exists"), MetricsStage.IsValid());
	TestTrue(TEXT("final stage object exists"), FinalStage.IsValid());
	if (!FirstStage.IsValid() || !MetricsStage.IsValid() || !FinalStage.IsValid())
	{
		return false;
	}

	TestEqual(
		TEXT("first stage name"),
		FirstStage->GetStringField(TEXT("stage")),
		FString(TEXT("validate_compiled_plan_contract")));
	TestEqual(
		TEXT("metrics stage name"),
		MetricsStage->GetStringField(TEXT("stage")),
		FString(TEXT("project_metrics_and_result")));
	TestEqual(
		TEXT("final stage name"),
		FinalStage->GetStringField(TEXT("stage")),
		FString(TEXT("finalize_bridge_response")));
	return true;
}

#endif
