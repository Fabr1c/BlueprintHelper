#if WITH_DEV_AUTOMATION_TESTS

#include "Runtime/TaskRuntime/Clusters/UMGWidget/BlueprintHelperWidgetTreeReviewEvidenceBuilder.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/UMGWidget/BlueprintHelperWidgetTaskPlanAdapter.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Shared/BlueprintHelperServiceTypes.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

class FBlueprintHelperUMGWidgetReviewEvidenceTestsLocalUtils
{
public:
	static FBlueprintHelperWidgetTreeReviewEvidenceBuildInput MakeInput(
		const FString& Operation,
		const TSharedRef<FJsonObject>& Payload)
	{
		FBlueprintHelperWidgetTreeReviewEvidenceBuildInput Input;
		Input.ArchiveSessionId = TEXT("archive_umg_widget_review");
		Input.TaskRunId = TEXT("task_umg_widget_review");
		Input.StepIndex = 3;
		Input.LoweredStep.StepId = TEXT("step_umg_widget");
		Input.LoweredStep.Capability = FBlueprintHelperWidgetTaskPlan::Capability::UMGWidget;
		Input.LoweredStep.RuntimeOperation = FBlueprintHelperWidgetTaskPlan::Capability::UMGWidget;
		Input.LoweredStep.AdapterOperation = Operation;
		Input.LoweredStep.Payload = Payload;
		Input.StepResult.bOk = true;
		Input.StepResult.Data = MakeShared<FJsonObject>();
		Input.StepResult.Data->SetStringField(TEXT("schema"), TEXT("WidgetMutation.v1"));
		return Input;
	}

	static TSharedRef<FJsonObject> MakeDiagnosticJson(
		const FString& Code,
		const FString& Message)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("severity"), TEXT("error"));
		Json->SetStringField(TEXT("code"), Code);
		Json->SetStringField(TEXT("message"), Message);
		return Json;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperUMGWidgetReviewEvidenceNamedSlotContentTest,
	"BlueprintHelper.TaskRuntime.UMGWidgetReviewEvidence.NamedSlotContent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperUMGWidgetReviewEvidenceNamedSlotContentTest::RunTest(const FString& Parameters)
{
	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("asset_path"), TEXT("/Game/UI/WBP_Menu"));
	Payload->SetStringField(TEXT("host_widget_name"), TEXT("DialogShell"));
	Payload->SetStringField(TEXT("slot_name"), TEXT("Body"));
	Payload->SetStringField(TEXT("widget_name"), TEXT("InventoryPanel"));
	Payload->SetStringField(TEXT("widget_class"), TEXT("/Game/UI/WBP_InventoryPanel"));
	Payload->SetNumberField(TEXT("virtual_index"), 0);

	FBlueprintHelperWidgetTreeReviewEvidenceBuildInput Input =
		FBlueprintHelperUMGWidgetReviewEvidenceTestsLocalUtils::MakeInput(
			FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetNamedSlotContent,
			Payload);
	Input.StepResult.Data->SetStringField(TEXT("widget_name"), TEXT("InventoryPanel"));

	FBlueprintHelperWriteReviewEvidence Evidence;
	const bool bBuilt = FBlueprintHelperWidgetTreeReviewEvidenceBuilder::Build(Input, Evidence);
	TestTrue(TEXT("named-slot evidence builds"), bBuilt);
	TestEqual(TEXT("one atomic target"), Evidence.AtomicTargets.Num(), 1);
	if (Evidence.AtomicTargets.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewAtomicTarget& Target = Evidence.AtomicTargets[0];
	TestEqual(TEXT("target kind is widget tree"), Target.TargetKind, FString(TEXT("umg_widget_tree")));
	TestEqual(TEXT("target subkind is named slot content"), Target.TargetSubKind, FString(TEXT("named_slot_content")));
	TestEqual(TEXT("target key uses host and slot"), Target.TargetKey, FString(TEXT("umg_widget_tree:DialogShell:slot:Body")));
	TestEqual(TEXT("slot parent lifecycle is host widget"), Target.LifecycleParentKey, FString(TEXT("widget:dialogshell")));
	TestTrue(TEXT("anchor carries readback facts"), Target.AnchorJson.Contains(TEXT("virtual_index")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperUMGWidgetReviewEvidenceDiagnosticsTest,
	"BlueprintHelper.TaskRuntime.UMGWidgetReviewEvidence.Diagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperUMGWidgetReviewEvidenceDiagnosticsTest::RunTest(const FString& Parameters)
{
	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("asset_path"), TEXT("/Game/UI/WBP_Menu"));
	Payload->SetStringField(TEXT("widget_name"), TEXT("Button_Start"));
	Payload->SetStringField(TEXT("new_parent_name"), TEXT("VerticalBox_Menu"));
	Payload->SetNumberField(TEXT("virtual_index"), 1);

	FBlueprintHelperWidgetTreeReviewEvidenceBuildInput Input =
		FBlueprintHelperUMGWidgetReviewEvidenceTestsLocalUtils::MakeInput(
			FBlueprintHelperWidgetTaskPlan::AdapterOperation::MoveWidget,
			Payload);

	TArray<TSharedPtr<FJsonValue>> Diagnostics;
	Diagnostics.Add(MakeShared<FJsonValueObject>(
		FBlueprintHelperUMGWidgetReviewEvidenceTestsLocalUtils::MakeDiagnosticJson(
			TEXT("widget_compile_failed"),
			TEXT("BindWidget mismatch"))));
	Input.StepResult.Data->SetArrayField(TEXT("diagnostics"), Diagnostics);

	FBlueprintHelperWriteReviewEvidence Evidence;
	const bool bBuilt = FBlueprintHelperWidgetTreeReviewEvidenceBuilder::Build(Input, Evidence);
	TestTrue(TEXT("move evidence builds"), bBuilt);
	TestEqual(TEXT("one diagnostic on evidence"), Evidence.Diagnostics.Num(), 1);
	TestEqual(TEXT("one atomic target"), Evidence.AtomicTargets.Num(), 1);
	if (Evidence.AtomicTargets.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewAtomicTarget& Target = Evidence.AtomicTargets[0];
	TestEqual(TEXT("widget target key"), Target.TargetKey, FString(TEXT("umg_widget:Button_Start")));
	TestEqual(TEXT("virtual index fingerprint"), Target.ReadbackFingerprintAfter, FString(TEXT("virtual_index:1")));
	TestEqual(TEXT("diagnostic correlated to target"), Target.Diagnostics.Num(), 1);
	if (Target.Diagnostics.Num() == 1)
	{
		TestEqual(TEXT("diagnostic target key"), Target.Diagnostics[0].TargetKey, Target.TargetKey);
		TestEqual(TEXT("diagnostic widget name"), Target.Diagnostics[0].NodeName, FString(TEXT("Button_Start")));
	}
	return true;
}

#endif
