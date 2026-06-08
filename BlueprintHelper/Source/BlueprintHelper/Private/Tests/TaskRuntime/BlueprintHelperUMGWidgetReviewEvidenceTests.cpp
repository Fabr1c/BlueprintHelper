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
	FBlueprintHelperUMGWidgetReviewEvidenceSlotPropertyTest,
	"BlueprintHelper.TaskRuntime.UMGWidgetReviewEvidence.SlotProperty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperUMGWidgetReviewEvidenceSlotPropertyTest::RunTest(const FString& Parameters)
{
	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("asset_path"), TEXT("/Game/UI/WBP_Menu"));
	Payload->SetStringField(TEXT("widget_name"), TEXT("StartButton"));
	Payload->SetStringField(TEXT("property_path"), TEXT("LayoutData.Offsets.Left"));
	Payload->SetStringField(TEXT("value"), TEXT("24"));
	Payload->SetStringField(TEXT("expected_slot_class_path"), TEXT("/Script/UMG.CanvasPanelSlot"));

	FBlueprintHelperWidgetTreeReviewEvidenceBuildInput Input =
		FBlueprintHelperUMGWidgetReviewEvidenceTestsLocalUtils::MakeInput(
			FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetSlotProperty,
			Payload);

	TSharedRef<FJsonObject> Mutation = MakeShared<FJsonObject>();
	Mutation->SetStringField(TEXT("target_kind"), TEXT("slot_property"));
	Mutation->SetStringField(TEXT("widget_name"), TEXT("StartButton"));
	Mutation->SetStringField(TEXT("slot_class_path"), TEXT("/Script/UMG.CanvasPanelSlot"));
	Mutation->SetStringField(TEXT("property_path"), TEXT("LayoutData.Offsets.Left"));
	Mutation->SetStringField(TEXT("before_value"), TEXT("0"));
	Mutation->SetStringField(TEXT("after_value"), TEXT("24"));
	Input.StepResult.Data->SetObjectField(TEXT("readback_context"), Mutation);

	FBlueprintHelperWriteReviewEvidence Evidence;
	const bool bBuilt = FBlueprintHelperWidgetTreeReviewEvidenceBuilder::Build(Input, Evidence);
	TestTrue(TEXT("slot-property evidence builds"), bBuilt);
	TestEqual(TEXT("one atomic target"), Evidence.AtomicTargets.Num(), 1);
	if (Evidence.AtomicTargets.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewAtomicTarget& Target = Evidence.AtomicTargets[0];
	TestEqual(TEXT("target kind is slot_property"), Target.TargetKind, FString(TEXT("slot_property")));
	TestEqual(TEXT("target subkind is slot_property"), Target.TargetSubKind, FString(TEXT("slot_property")));
	TestEqual(TEXT("target key uses widget and property path"), Target.TargetKey, FString(TEXT("slot_property:StartButton.LayoutData.Offsets.Left")));
	TestEqual(TEXT("slot class path is carried"), Target.ComponentPath, FString(TEXT("/Script/UMG.CanvasPanelSlot")));
	TestEqual(TEXT("before value fingerprint"), Target.ReadbackFingerprintBefore, FString(TEXT("slot_property:0")));
	TestEqual(TEXT("after value fingerprint"), Target.ReadbackFingerprintAfter, FString(TEXT("slot_property:24")));
	TestTrue(TEXT("changed properties carries before value"), Target.ChangedPropertiesJson.Contains(TEXT("\"before_value\":\"0\"")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperUMGWidgetReviewEvidenceWidgetVariableTest,
	"BlueprintHelper.TaskRuntime.UMGWidgetReviewEvidence.WidgetVariable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperUMGWidgetReviewEvidenceWidgetVariableTest::RunTest(const FString& Parameters)
{
	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("asset_path"), TEXT("/Game/UI/WBP_Menu"));
	Payload->SetStringField(TEXT("widget_name"), TEXT("StartButton"));
	Payload->SetBoolField(TEXT("is_variable"), true);
	Payload->SetStringField(TEXT("expected_widget_class_path"), TEXT("/Script/UMG.Button"));

	FBlueprintHelperWidgetTreeReviewEvidenceBuildInput Input =
		FBlueprintHelperUMGWidgetReviewEvidenceTestsLocalUtils::MakeInput(
			FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetWidgetAsVariable,
			Payload);

	TSharedRef<FJsonObject> Mutation = MakeShared<FJsonObject>();
	Mutation->SetStringField(TEXT("target_kind"), TEXT("widget_variable"));
	Mutation->SetStringField(TEXT("widget_name"), TEXT("StartButton"));
	Mutation->SetBoolField(TEXT("before_is_variable"), false);
	Mutation->SetBoolField(TEXT("after_is_variable"), true);
	Mutation->SetStringField(TEXT("variable_guid_state"), TEXT("valid"));
	Input.StepResult.Data->SetObjectField(TEXT("readback_context"), Mutation);

	FBlueprintHelperWriteReviewEvidence Evidence;
	const bool bBuilt = FBlueprintHelperWidgetTreeReviewEvidenceBuilder::Build(Input, Evidence);
	TestTrue(TEXT("widget-variable evidence builds"), bBuilt);
	TestEqual(TEXT("one atomic target"), Evidence.AtomicTargets.Num(), 1);
	if (Evidence.AtomicTargets.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperReviewAtomicTarget& Target = Evidence.AtomicTargets[0];
	TestEqual(TEXT("target kind is widget_variable"), Target.TargetKind, FString(TEXT("widget_variable")));
	TestEqual(TEXT("target subkind is widget_variable"), Target.TargetSubKind, FString(TEXT("widget_variable")));
	TestEqual(TEXT("target key uses widget variable identity"), Target.TargetKey, FString(TEXT("widget_variable:StartButton")));
	TestEqual(TEXT("property path is variable flag"), Target.PropertyPath, FString(TEXT("is_variable")));
	TestEqual(TEXT("before variable fingerprint"), Target.ReadbackFingerprintBefore, FString(TEXT("is_variable:false")));
	TestEqual(TEXT("after variable fingerprint"), Target.ReadbackFingerprintAfter, FString(TEXT("is_variable:true")));
	TestEqual(TEXT("variable guid state carried"), Target.ComponentOrigin, FString(TEXT("valid")));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperUMGWidgetReviewEvidenceP2DescriptorSubKindsTest,
	"BlueprintHelper.TaskRuntime.UMGWidgetReviewEvidence.P2DescriptorSubKinds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperUMGWidgetReviewEvidenceP2DescriptorSubKindsTest::RunTest(const FString& Parameters)
{
	struct FCase
	{
		const TCHAR* Operation;
		const TCHAR* ExpectedSubKind;
		TFunction<void(const TSharedRef<FJsonObject>&)> FillPayload;
		const TCHAR* ExpectedTargetKeyFragment;
		const TCHAR* ResultWidgetName;
	};

	TArray<FCase> Cases;
	Cases.Add({
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::RenameWidget,
		TEXT("widget_rename"),
		[](const TSharedRef<FJsonObject>& Payload)
		{
			Payload->SetStringField(TEXT("widget_name"), TEXT("OldButton"));
			Payload->SetStringField(TEXT("new_widget_name"), TEXT("StartButton"));
		},
		TEXT("OldButton"),
		TEXT("StartButton")
	});
	Cases.Add({
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::RemoveRootWidget,
		TEXT("root_widget_removal"),
		[](const TSharedRef<FJsonObject>& Payload)
		{
			Payload->SetStringField(TEXT("root_widget_name"), TEXT("RootCanvas"));
			Payload->SetStringField(TEXT("replacement_policy"), TEXT("remove_empty_root"));
		},
		TEXT("RootCanvas"),
		TEXT("PromotedPanel")
	});
	Cases.Add({
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::ReparentWidgetBlueprint,
		TEXT("widget_blueprint_reparent"),
		[](const TSharedRef<FJsonObject>& Payload)
		{
			Payload->SetStringField(TEXT("new_parent_class"), TEXT("/Script/UMG.UserWidget"));
		},
		TEXT("widget_blueprint"),
		TEXT("widget_blueprint")
	});
	Cases.Add({
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::DuplicateWidgetSubtree,
		TEXT("widget_subtree_duplicate"),
		[](const TSharedRef<FJsonObject>& Payload)
		{
			Payload->SetStringField(TEXT("source_widget_name"), TEXT("SourcePanel"));
			Payload->SetStringField(TEXT("target_parent_name"), TEXT("RootCanvas"));
			TSharedRef<FJsonObject> NameMapping = MakeShared<FJsonObject>();
			NameMapping->SetStringField(TEXT("SourcePanel"), TEXT("SourcePanelCopy"));
			Payload->SetObjectField(TEXT("name_mapping"), NameMapping);
		},
		TEXT("SourcePanelCopy"),
		TEXT("SourcePanelCopy")
	});
	Cases.Add({
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::WrapWidget,
		TEXT("widget_wrap"),
		[](const TSharedRef<FJsonObject>& Payload)
		{
			Payload->SetStringField(TEXT("widget_name"), TEXT("StartButton"));
			Payload->SetStringField(TEXT("wrapper_name"), TEXT("StartButtonWrapper"));
		},
		TEXT("StartButtonWrapper"),
		TEXT("StartButtonWrapper")
	});
	Cases.Add({
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::ReplaceWidgetClass,
		TEXT("widget_class_replace"),
		[](const TSharedRef<FJsonObject>& Payload)
		{
			Payload->SetStringField(TEXT("widget_name"), TEXT("StartButton"));
			Payload->SetStringField(TEXT("new_widget_class"), TEXT("TextBlock"));
		},
		TEXT("StartButton"),
		TEXT("StartButton")
	});

	for (const FCase& Case : Cases)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), TEXT("/Game/UI/WBP_Menu"));
		Case.FillPayload(Payload);

		FBlueprintHelperWidgetTreeReviewEvidenceBuildInput Input =
			FBlueprintHelperUMGWidgetReviewEvidenceTestsLocalUtils::MakeInput(Case.Operation, Payload);
		Input.StepResult.Data->SetStringField(TEXT("widget_name"), Case.ResultWidgetName);

		FBlueprintHelperWriteReviewEvidence Evidence;
		const bool bBuilt = FBlueprintHelperWidgetTreeReviewEvidenceBuilder::Build(Input, Evidence);
		TestTrue(FString::Printf(TEXT("%s evidence builds"), Case.Operation), bBuilt);
		TestEqual(FString::Printf(TEXT("%s one target"), Case.Operation), Evidence.AtomicTargets.Num(), 1);
		if (!bBuilt || Evidence.AtomicTargets.Num() != 1)
		{
			continue;
		}

		const FBlueprintHelperReviewAtomicTarget& Target = Evidence.AtomicTargets[0];
		TestEqual(FString::Printf(TEXT("%s target subkind"), Case.Operation), Target.TargetSubKind, FString(Case.ExpectedSubKind));
		TestTrue(
			FString::Printf(TEXT("%s target key contains expected identity"), Case.Operation),
			Target.TargetKey.Contains(Case.ExpectedTargetKeyFragment));
		if (FString(Case.Operation) == FBlueprintHelperWidgetTaskPlan::AdapterOperation::RemoveRootWidget)
		{
			TestTrue(
				TEXT("root removal changed properties carry before root"),
				Target.ChangedPropertiesJson.Contains(TEXT("\"before_root_widget_name\":\"RootCanvas\"")));
			TestTrue(
				TEXT("root removal changed properties carry after root"),
				Target.ChangedPropertiesJson.Contains(TEXT("\"after_root_widget_name\":\"PromotedPanel\"")));
		}
	}

	return true;
}

#endif
