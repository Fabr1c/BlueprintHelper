#include "Runtime/TaskRuntime/Pipeline/BlueprintHelperTaskRuntimePipeline.h"
#include "Runtime/TaskRuntime/Pipeline/BlueprintHelperTaskRuntimePipelineExecutors.h"

#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

static bool LoadBlueprintHelperTaskRuntimePipelineTestSource(
	FAutomationTestBase& Test,
	const FString& RelativePath,
	FString& OutSource)
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BlueprintHelper"));
	const FString Path = Plugin.IsValid()
		? FPaths::Combine(Plugin->GetBaseDir(), RelativePath)
		: FString();
	if (Path.IsEmpty() || !FFileHelper::LoadFileToString(OutSource, *Path))
	{
		Test.AddError(FString::Printf(TEXT("Unable to load source file for TaskRuntime pipeline guard: %s"), *RelativePath));
		return false;
	}
	return true;
}

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimePipeline_ServiceDoesNotOwnPipelineStages,
	"BlueprintHelper.TaskRuntime.Pipeline.ServiceDoesNotOwnPipelineStages",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimePipeline_ServiceDoesNotOwnPipelineStages::RunTest(const FString& Parameters)
{
	FString Source;
	if (!LoadBlueprintHelperTaskRuntimePipelineTestSource(
		*this,
		TEXT("Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp"),
		Source))
	{
		return false;
	}

	TestFalse(
		TEXT("TaskRuntimeService owns direct review evidence call"),
		Source.Contains(TEXT("ClusterHub->BuildReviewEvidence(")));
	TestFalse(
		TEXT("TaskRuntimeService owns final runtime data construction"),
		Source.Contains(TEXT("BuildRuntimeDataForSteps(")));
	TestFalse(
		TEXT("TaskRuntimeService owns pipeline stage recording"),
		Source.Contains(TEXT("RecordPipelineStage(")));
	TestFalse(
		TEXT("TaskRuntimeService owns direct post-operation execution"),
		Source.Contains(TEXT("FBlueprintHelperTaskRuntimePostOperationExecutor")));
	TestFalse(
		TEXT("TaskRuntimeService owns post-operation planning"),
		Source.Contains(TEXT("FBlueprintHelperTaskRuntimePostOperationPlanner")));
	TestFalse(
		TEXT("TaskRuntimeService bypasses pipeline batch execution"),
		Source.Contains(TEXT("PipelineRunner.ExecuteStage(")));
	TestTrue(
		TEXT("TaskRuntimeService routes cluster execution through the pipeline executor"),
		Source.Contains(TEXT("FBlueprintHelperTaskRuntimeClusterExecuteStageExecutor")));
	TestTrue(
		TEXT("TaskRuntimeService routes post operations through the pipeline sequence executor"),
		Source.Contains(TEXT("FBlueprintHelperTaskRuntimePostOperationSequenceStageExecutor")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimePipeline_ExecutesStageBatchInOrder,
	"BlueprintHelper.TaskRuntime.Pipeline.ExecutesStageBatchInOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimePipeline_ExecutesStageBatchInOrder::RunTest(const FString& Parameters)
{
	TArray<FString> Executed;
	TArray<TUniquePtr<IBlueprintHelperTaskRuntimePipelineStageExecutor>> Executors;
	Executors.Add(MakeUnique<FBlueprintHelperTaskRuntimeCallbackStageExecutor>(
		EBlueprintHelperTaskRuntimePipelineStage::ValidateCompiledPlanContract,
		[&](FBlueprintHelperTaskRuntimePipelineExecutionContext& Context)
		{
			Executed.Add(TEXT("validate"));
			Context.TaskRunId = TEXT("pipeline_batch_test_mutated");
			return FBlueprintHelperToolResultBuilder::Applied(
				TEXT("validate_stage"),
				FBlueprintHelperToolResultBuilder::GenerateTraceId());
		}));
	Executors.Add(MakeUnique<FBlueprintHelperTaskRuntimeCallbackStageExecutor>(
		EBlueprintHelperTaskRuntimePipelineStage::FinalizeBridgeResponse,
		[&](FBlueprintHelperTaskRuntimePipelineExecutionContext& Context)
		{
			Executed.Add(Context.TaskRunId);
			return FBlueprintHelperToolResultBuilder::Applied(
				TEXT("finalize_stage"),
				FBlueprintHelperToolResultBuilder::GenerateTraceId());
		}));

	FBlueprintHelperTaskRuntimePipeline Pipeline(MoveTemp(Executors));
	FBlueprintHelperTaskRuntimePipelineRunner Runner(
		MakeShared<FJsonObject>(),
		TEXT("pipeline_batch_test"),
		true);
	FBlueprintHelperTaskRuntimePipelineExecutionContext Context = Runner.MakeExecutionContext();
	const FBlueprintHelperToolResultBase Result = Pipeline.Execute(Runner, Context);

	TestTrue(TEXT("pipeline batch succeeds"), Result.bOk);
	TestEqual(TEXT("stage batch count"), Executed.Num(), 2);
	TestEqual(TEXT("first stage executes first"), Executed[0], FString(TEXT("validate")));
	TestEqual(
		TEXT("second stage observes mutated context"),
		Executed[1],
		FString(TEXT("pipeline_batch_test_mutated")));
	TestTrue(
		TEXT("runner trace records first executor stage"),
		Runner.HasStage(EBlueprintHelperTaskRuntimePipelineStage::ValidateCompiledPlanContract));
	TestTrue(
		TEXT("runner trace records second executor stage"),
		Runner.HasStage(EBlueprintHelperTaskRuntimePipelineStage::FinalizeBridgeResponse));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimePipeline_StageExecutorInterfaceIsPresent,
	"BlueprintHelper.TaskRuntime.Pipeline.StageExecutorInterfaceIsPresent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimePipeline_StageExecutorInterfaceIsPresent::RunTest(const FString& Parameters)
{
	FString HeaderSource;
	if (!LoadBlueprintHelperTaskRuntimePipelineTestSource(
		*this,
		TEXT("Source/BlueprintHelper/Public/Runtime/TaskRuntime/Pipeline/BlueprintHelperTaskRuntimePipeline.h"),
		HeaderSource))
	{
		return false;
	}

	TestTrue(
		TEXT("pipeline defines a shared execution context"),
		HeaderSource.Contains(TEXT("FBlueprintHelperTaskRuntimePipelineExecutionContext")));
	TestTrue(
		TEXT("pipeline defines a stage executor interface"),
		HeaderSource.Contains(TEXT("IBlueprintHelperTaskRuntimePipelineStageExecutor")));
	TestTrue(
		TEXT("pipeline runner executes stage executors"),
		HeaderSource.Contains(TEXT("ExecuteStage(")));

	FString ExecutorSource;
	if (!LoadBlueprintHelperTaskRuntimePipelineTestSource(
		*this,
		TEXT("Source/BlueprintHelper/Private/Runtime/TaskRuntime/Pipeline/BlueprintHelperTaskRuntimePipelineExecutors.cpp"),
		ExecutorSource))
	{
		return false;
	}

	TestTrue(
		TEXT("post-operation stage executor owns post-operation execution"),
		ExecutorSource.Contains(TEXT("FBlueprintHelperTaskRuntimePostOperationExecutor Executor")));
	TestTrue(
		TEXT("cluster stage executor owns commit-service execution"),
		ExecutorSource.Contains(TEXT("CommitService.ExecuteStep(")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimePipeline_RuntimeEvidenceDoesNotShortCircuitAfterPreEvidence,
	"BlueprintHelper.TaskRuntime.Pipeline.RuntimeEvidenceDoesNotShortCircuitAfterPreEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimePipeline_RuntimeEvidenceDoesNotShortCircuitAfterPreEvidence::RunTest(const FString& Parameters)
{
	FString Source;
	if (!LoadBlueprintHelperTaskRuntimePipelineTestSource(
		*this,
		TEXT("Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp"),
		Source))
	{
		return false;
	}

	TestFalse(
		TEXT("runtime review evidence builder must not be skipped when pre-step evidence exists"),
		Source.Contains(TEXT("bHasPreStepReviewEvidence ||")));
	TestTrue(
		TEXT("runtime review evidence is preferred before fallback evidence"),
		Source.Contains(TEXT("bHasRuntimeReviewEvidence")));
	TestTrue(
		TEXT("pre-step before snapshots are applied to runtime evidence"),
		Source.Contains(TEXT("ApplyCachedTaskRuntimeReviewTargetSnapshots")));
	TestTrue(
		TEXT("UMG pre-step evidence uses the cluster-specific builder to keep target keys aligned"),
		Source.Contains(TEXT("MakeTaskRuntimePreStepReviewResult")));
	TestTrue(
		TEXT("UMG pre-step evidence is routed through the pipeline runner"),
		Source.Contains(TEXT("PipelineRunner.BuildReviewEvidence")));
	return true;
}

#endif
