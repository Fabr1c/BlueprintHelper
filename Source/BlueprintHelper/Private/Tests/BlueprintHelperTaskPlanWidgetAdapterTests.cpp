#if WITH_DEV_AUTOMATION_TESTS

#include "TaskRuntime/TaskPlanAdapters/BlueprintHelperWidgetTaskPlanAdapter.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"

namespace
{
TSharedPtr<FJsonObject> MakeWidgetTaskPlanStep(
	const TArray<TSharedPtr<FJsonValue>>& Ops,
	const TCHAR* Strategy = BlueprintHelperWidgetTaskPlan::Strategy::WidgetTreeEdit)
{
	TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
	Step->SetStringField(TEXT("step_id"), TEXT("step_widget"));
	Step->SetStringField(TEXT("capability"), BlueprintHelperWidgetTaskPlan::Capability::UMGWidget);

	TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
	Target->SetStringField(TEXT("asset_path"), TEXT("/Game/UI/WBP_MainMenu"));
	Step->SetObjectField(TEXT("target"), Target);

	TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
	Write->SetStringField(TEXT("strategy"), Strategy);
	Write->SetArrayField(TEXT("ops"), Ops);
	Step->SetObjectField(TEXT("write"), Write);

	TSharedPtr<FJsonObject> Constraints = MakeShared<FJsonObject>();
	Constraints->SetBoolField(TEXT("allow_remove_referenced_widgets"), false);
	Step->SetObjectField(TEXT("constraints"), Constraints);

	return Step;
}

TSharedPtr<FJsonObject> MakeWidgetTaskPlan(const TSharedPtr<FJsonObject>& Step)
{
	TSharedPtr<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
	TaskPlan->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.TaskPlan.v1"));
	TaskPlan->SetStringField(TEXT("task_name"), TEXT("MainMenuWidgets"));
	TaskPlan->SetStringField(TEXT("task_type"), TEXT("edit_umg_widget"));

	TArray<TSharedPtr<FJsonValue>> TargetAssets;
	TargetAssets.Add(MakeShared<FJsonValueString>(TEXT("/Game/UI/WBP_MainMenu")));
	TaskPlan->SetArrayField(TEXT("target_assets"), TargetAssets);

	TSharedPtr<FJsonObject> ExecutionPolicy = MakeShared<FJsonObject>();
	ExecutionPolicy->SetStringField(TEXT("dry_run_mode"), TEXT("full"));
	ExecutionPolicy->SetBoolField(TEXT("should_compile"), true);
	ExecutionPolicy->SetBoolField(TEXT("should_save"), false);
	TaskPlan->SetObjectField(TEXT("execution_policy"), ExecutionPolicy);

	TArray<TSharedPtr<FJsonValue>> Steps;
	Steps.Add(MakeShared<FJsonValueObject>(Step.ToSharedRef()));
	TaskPlan->SetArrayField(TEXT("steps"), Steps);

	return TaskPlan;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetTaskPlanAddWidgetLoweringTest,
	"BlueprintHelper.TaskPlan.WidgetAdapter.AddWidgetLowering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetTaskPlanAddWidgetLoweringTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
	Op->SetStringField(TEXT("op"), BlueprintHelperWidgetTaskPlan::Op::AddWidget);
	Op->SetStringField(TEXT("parent_widget_name"), TEXT("CanvasRoot"));
	Op->SetStringField(TEXT("widget_class"), TEXT("TextBlock"));
	Op->SetStringField(TEXT("widget_name"), TEXT("TitleText"));

	TArray<TSharedPtr<FJsonValue>> Ops;
	Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

	const TSharedPtr<FJsonObject> Step = MakeWidgetTaskPlanStep(Ops);
	const TSharedPtr<FJsonObject> TaskPlan = MakeWidgetTaskPlan(Step);

	FBlueprintHelperWidgetTaskPlanLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperWidgetTaskPlanAdapter::TryLowerTaskPlanStep(
		TaskPlan,
		Step,
		true,
		LoweredStep,
		Error);

	TestTrue(TEXT("umg_widget add_widget lowers successfully"), bLowered);
	TestEqual(TEXT("step capability preserved"), LoweredStep.Capability, FString(BlueprintHelperWidgetTaskPlan::Capability::UMGWidget));
	TestEqual(TEXT("runtime operation is capability"), LoweredStep.RuntimeOperation, FString(BlueprintHelperWidgetTaskPlan::Capability::UMGWidget));
	TestFalse(TEXT("current widget service adapter has no dry-run support"), LoweredStep.bAdapterDryRunSupported);
	TestEqual(TEXT("one lowered widget op emitted"), LoweredStep.Ops.Num(), 1);

	if (LoweredStep.Ops.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperWidgetTaskPlanLoweredOp& LoweredOp = LoweredStep.Ops[0];
	TestEqual(TEXT("add op lowers to current service adapter name"), LoweredOp.AdapterOperation, FString(BlueprintHelperWidgetTaskPlan::AdapterOperation::AddWidget));
	TestNotNull(TEXT("add payload exists"), LoweredOp.Payload.Get());

	FString AssetPath;
	TestTrue(TEXT("payload carries asset_path"), LoweredOp.Payload->TryGetStringField(TEXT("asset_path"), AssetPath));
	TestEqual(TEXT("asset_path matches step target"), AssetPath, FString(TEXT("/Game/UI/WBP_MainMenu")));

	FString ParentName;
	TestTrue(TEXT("payload carries parent_name for current service"), LoweredOp.Payload->TryGetStringField(TEXT("parent_name"), ParentName));
	TestEqual(TEXT("parent_widget_name lowers to parent_name"), ParentName, FString(TEXT("CanvasRoot")));

	FString WidgetClass;
	TestTrue(TEXT("payload carries widget_class"), LoweredOp.Payload->TryGetStringField(TEXT("widget_class"), WidgetClass));
	TestEqual(TEXT("widget_class preserved"), WidgetClass, FString(TEXT("TextBlock")));

	FString WidgetName;
	TestTrue(TEXT("payload carries widget_name"), LoweredOp.Payload->TryGetStringField(TEXT("widget_name"), WidgetName));
	TestEqual(TEXT("widget_name preserved"), WidgetName, FString(TEXT("TitleText")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetTaskPlanSetWidgetPropertyLoweringTest,
	"BlueprintHelper.TaskPlan.WidgetAdapter.SetWidgetPropertyLowering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetTaskPlanSetWidgetPropertyLoweringTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
	Op->SetStringField(TEXT("op"), BlueprintHelperWidgetTaskPlan::Op::SetWidgetProperty);
	Op->SetStringField(TEXT("widget_name"), TEXT("TitleText"));
	Op->SetStringField(TEXT("property_path"), TEXT("Text"));
	Op->SetStringField(TEXT("value"), TEXT("Start Game"));

	TArray<TSharedPtr<FJsonValue>> Ops;
	Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

	const TSharedPtr<FJsonObject> Step = MakeWidgetTaskPlanStep(
		Ops,
		BlueprintHelperWidgetTaskPlan::Strategy::WidgetPropertyEdit);
	const TSharedPtr<FJsonObject> TaskPlan = MakeWidgetTaskPlan(Step);

	FBlueprintHelperWidgetTaskPlanLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperWidgetTaskPlanAdapter::TryLowerTaskPlanStep(
		TaskPlan,
		Step,
		false,
		LoweredStep,
		Error);

	TestTrue(TEXT("umg_widget set_widget_property lowers successfully"), bLowered);
	TestEqual(TEXT("one lowered widget op emitted"), LoweredStep.Ops.Num(), 1);

	if (LoweredStep.Ops.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperWidgetTaskPlanLoweredOp& LoweredOp = LoweredStep.Ops[0];
	TestEqual(TEXT("property op lowers to current service adapter name"), LoweredOp.AdapterOperation, FString(BlueprintHelperWidgetTaskPlan::AdapterOperation::SetWidgetProperty));

	FString PropertyName;
	TestTrue(TEXT("payload carries property_name for current service"), LoweredOp.Payload->TryGetStringField(TEXT("property_name"), PropertyName));
	TestEqual(TEXT("property_path lowers to property_name"), PropertyName, FString(TEXT("Text")));

	FString Value;
	TestTrue(TEXT("payload carries string value for current service"), LoweredOp.Payload->TryGetStringField(TEXT("value"), Value));
	TestEqual(TEXT("value preserved as import text"), Value, FString(TEXT("Start Game")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetTaskPlanRemoveWidgetLoweringTest,
	"BlueprintHelper.TaskPlan.WidgetAdapter.RemoveWidgetLowering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetTaskPlanRemoveWidgetLoweringTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
	Op->SetStringField(TEXT("op"), BlueprintHelperWidgetTaskPlan::Op::RemoveWidget);
	Op->SetStringField(TEXT("widget_name"), TEXT("OldButton"));

	TArray<TSharedPtr<FJsonValue>> Ops;
	Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

	const TSharedPtr<FJsonObject> Step = MakeWidgetTaskPlanStep(Ops);
	const TSharedPtr<FJsonObject> TaskPlan = MakeWidgetTaskPlan(Step);

	FBlueprintHelperWidgetTaskPlanLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperWidgetTaskPlanAdapter::TryLowerTaskPlanStep(
		TaskPlan,
		Step,
		false,
		LoweredStep,
		Error);

	TestTrue(TEXT("umg_widget remove_widget lowers successfully"), bLowered);
	TestEqual(TEXT("one lowered widget op emitted"), LoweredStep.Ops.Num(), 1);

	if (LoweredStep.Ops.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperWidgetTaskPlanLoweredOp& LoweredOp = LoweredStep.Ops[0];
	TestEqual(TEXT("remove op lowers to current service adapter name"), LoweredOp.AdapterOperation, FString(BlueprintHelperWidgetTaskPlan::AdapterOperation::RemoveWidget));

	FString WidgetName;
	TestTrue(TEXT("payload carries widget_name"), LoweredOp.Payload->TryGetStringField(TEXT("widget_name"), WidgetName));
	TestEqual(TEXT("widget_name preserved"), WidgetName, FString(TEXT("OldButton")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetTaskPlanOperationFieldRejectedTest,
	"BlueprintHelper.TaskPlan.WidgetAdapter.OperationFieldRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetTaskPlanOperationFieldRejectedTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
	Op->SetStringField(TEXT("op"), BlueprintHelperWidgetTaskPlan::Op::AddWidget);
	Op->SetStringField(TEXT("widget_class"), TEXT("TextBlock"));
	Op->SetStringField(TEXT("widget_name"), TEXT("TitleText"));

	TArray<TSharedPtr<FJsonValue>> Ops;
	Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

	const TSharedPtr<FJsonObject> Step = MakeWidgetTaskPlanStep(Ops);
	Step->SetStringField(TEXT("operation"), TEXT("add_widget_to_tree"));

	const TSharedPtr<FJsonObject> TaskPlan = MakeWidgetTaskPlan(Step);

	FBlueprintHelperWidgetTaskPlanLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperWidgetTaskPlanAdapter::TryLowerTaskPlanStep(
		TaskPlan,
		Step,
		true,
		LoweredStep,
		Error);

	TestFalse(TEXT("umg_widget IR rejects adapter operation field"), bLowered);
	TestEqual(TEXT("operation field reports UMG error code"), Error.Code, FString(TEXT("unsupported_umg_widget_operation_field")));
	TestEqual(TEXT("operation field reports parse_input stage"), Error.Stage, EBlueprintHelperToolStage::ParseInput);
	TestEqual(TEXT("operation field points at operation field"), Error.Field, FString(TEXT("task_plan.steps[0].operation")));

	return true;
}

#endif
