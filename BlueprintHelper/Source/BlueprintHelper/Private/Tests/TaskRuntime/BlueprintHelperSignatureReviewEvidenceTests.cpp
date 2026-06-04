#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintSignature/BlueprintHelperSignatureTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/Utils/BlueprintHelperTaskRuntimeClusterExecutionUtils.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSignatureReviewEvidenceSubkindTest,
	"BlueprintHelper.TaskRuntime.SignatureReviewEvidence.Subkind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSignatureReviewEvidenceSubkindTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	LoweredStep.Capability = FBlueprintHelperSignatureTaskPlanAdapter::CapabilityName;
	LoweredStep.AdapterOperation = FBlueprintHelperSignatureTaskPlanAdapter::AdapterOperationEnsureFunction;
	LoweredStep.Payload = MakeShared<FJsonObject>();
	LoweredStep.Payload->SetStringField(TEXT("asset_path"), TEXT("/Game/Test/BP_Signature.BP_Signature"));
	LoweredStep.Payload->SetStringField(TEXT("function_name"), TEXT("ComputeScore"));

	FBlueprintHelperWriteReviewEvidence Evidence;
	const bool bBuilt = FBlueprintHelperTaskRuntimeClusterExecutionUtils::TryBuildTaskRuntimeReviewEvidence(
		LoweredStep,
		TEXT("archive"),
		TEXT("task_run"),
		0,
		Evidence);

	TestTrue(TEXT("signature evidence builds"), bBuilt);
	TestEqual(TEXT("one target"), Evidence.AtomicTargets.Num(), 1);
	if (Evidence.AtomicTargets.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewAtomicTarget& Target = Evidence.AtomicTargets[0];
	TestEqual(TEXT("primary target kind remains signature"),
		Target.TargetKind,
		FString(TEXT("signature")));
	TestEqual(TEXT("signature subkind is function"),
		Target.TargetSubKind,
		FString(TEXT("function")));
	TestEqual(TEXT("signature visual group includes subkind"),
		Target.VisualGroupKey,
		FString(TEXT("signature:function:ComputeScore")));
	TestEqual(TEXT("signature evidence id includes subkind"),
		Target.SignatureEvidenceId,
		FString(TEXT("signature:function:ComputeScore")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSignatureReviewEvidenceDependencyGroupingTest,
	"BlueprintHelper.TaskRuntime.SignatureReviewEvidence.DependencyGrouping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSignatureReviewEvidenceDependencyGroupingTest::RunTest(const FString& Parameters)
{
	const FString SignatureEvidenceId = TEXT("signature:custom_event:HandleOpened");

	TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
	Entry->SetStringField(TEXT("kind"), TEXT("custom_event"));
	Entry->SetStringField(TEXT("name"), TEXT("HandleOpened"));
	Entry->SetStringField(TEXT("signature_evidence_id"), SignatureEvidenceId);

	TSharedRef<FJsonObject> LogicSpec = MakeShared<FJsonObject>();
	LogicSpec->SetObjectField(TEXT("entry"), Entry);

	TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
	Target->SetStringField(TEXT("asset_path"), TEXT("/Game/Test/BP_Signature.BP_Signature"));
	Target->SetStringField(TEXT("graph"), TEXT("HandleOpened"));

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	LoweredStep.StepId = TEXT("step_graph_body");
	LoweredStep.DependsOn.Add(TEXT("step_signature_custom_event"));
	LoweredStep.Capability = TEXT("graph_write");
	LoweredStep.AdapterOperation = TEXT("append_blueprint_graph");
	LoweredStep.Payload = MakeShared<FJsonObject>();
	LoweredStep.Payload->SetObjectField(TEXT("target"), Target);
	LoweredStep.Payload->SetObjectField(TEXT("logic_spec"), LogicSpec);

	FBlueprintHelperToolResultBase StepResult;
	StepResult.bOk = true;
	StepResult.Status = EBlueprintHelperToolStatus::Applied;
	StepResult.Data = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> BlockRefs;
	BlockRefs.Add(MakeShared<FJsonValueString>(TEXT("HandleOpened_entry")));
	StepResult.Data->SetArrayField(TEXT("block_refs"), BlockRefs);

	FBlueprintHelperWriteReviewEvidence Evidence;
	const bool bBuilt = FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence(
		LoweredStep,
		StepResult,
		TEXT("archive"),
		TEXT("task_run"),
		1,
		Evidence);

	TestTrue(TEXT("graphwrite evidence builds"), bBuilt);
	TestEqual(TEXT("one graph target"), Evidence.AtomicTargets.Num(), 1);
	if (Evidence.AtomicTargets.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewAtomicTarget& GraphTarget = Evidence.AtomicTargets[0];
	TestEqual(TEXT("graph target references signature role"),
		GraphTarget.SignatureRole,
		FString(TEXT("dependency")));
	TestEqual(TEXT("graph target references signature evidence id"),
		GraphTarget.SignatureEvidenceId,
		SignatureEvidenceId);
	TestEqual(TEXT("dependency owner step id is graph body"),
		GraphTarget.DependencyOwnerStepId,
		FString(TEXT("step_graph_body")));
	TestEqual(TEXT("dependent step id is signature step"),
		GraphTarget.DependentStepId,
		FString(TEXT("step_signature_custom_event")));
	return true;
}

#endif
