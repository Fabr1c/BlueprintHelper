#if WITH_DEV_AUTOMATION_TESTS

#include "Runtime/TaskRuntime/TaskPlanAdapters/UMGWidget/BlueprintHelperWidgetTaskPlanAdapter.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ExpandableArea.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Shared/BlueprintHelperWidgetVersionCompat.h"
#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetService.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"

class FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils
{
public:
static FString MakeWidgetServiceTestObjectName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UPackage* MakeWidgetServiceTestPackage(const FString& Prefix)
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperSafety/%s"),
		*MakeWidgetServiceTestObjectName(Prefix)));
	Package->SetDirtyFlag(false);
	return Package;
}

struct FWidgetServiceDryRunFixture
{
	UPackage* Package = nullptr;
	UWidgetBlueprint* Blueprint = nullptr;
	UCanvasPanel* Root = nullptr;
	UTextBlock* ExistingText = nullptr;
	UTextBlock* SecondText = nullptr;
	UExpandableArea* NamedSlotHost = nullptr;
};

static FWidgetServiceDryRunFixture MakeWidgetServiceDryRunFixture(const FString& Prefix)
{
	FWidgetServiceDryRunFixture Fixture;
	Fixture.Package = MakeWidgetServiceTestPackage(Prefix);
	Fixture.Blueprint = NewObject<UWidgetBlueprint>(
		Fixture.Package,
		*MakeWidgetServiceTestObjectName(TEXT("WBP_WidgetDryRun")),
		RF_Public | RF_Standalone | RF_Transactional);
	Fixture.Blueprint->WidgetTree = NewObject<UWidgetTree>(
		Fixture.Blueprint,
		TEXT("WidgetTree"),
		RF_Transactional);

	Fixture.Root = Fixture.Blueprint->WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("RootCanvas"));
	Fixture.Blueprint->WidgetTree->RootWidget = Fixture.Root;
	FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(Fixture.Blueprint, Fixture.Root);

	Fixture.ExistingText = Fixture.Blueprint->WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("ExistingText"));
	Fixture.ExistingText->SetRenderOpacity(1.0f);
	Fixture.Root->AddChild(Fixture.ExistingText);
	FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(Fixture.Blueprint, Fixture.ExistingText);

	Fixture.SecondText = Fixture.Blueprint->WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("SecondText"));
	Fixture.Root->AddChild(Fixture.SecondText);
	FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(Fixture.Blueprint, Fixture.SecondText);

	Fixture.Package->SetDirtyFlag(false);
	return Fixture;
}

static bool IsWidgetVariableRegistered(UWidgetBlueprint* Blueprint, UWidget* Widget)
{
#if WITH_EDITORONLY_DATA
	if (!Blueprint || !Widget)
	{
		return false;
	}
#if BLUEPRINTHELPER_UE_HAS_WIDGET_VARIABLE_GUID_EVENTS
	return Blueprint->WidgetVariableNameToGuidMap.Contains(Widget->GetFName());
#else
	return Widget->bIsVariable;
#endif
#else
	return false;
#endif
}

static TSharedPtr<FJsonObject> MakeWidgetTaskPlanStep(
	const TArray<TSharedPtr<FJsonValue>>& Ops,
	const TCHAR* Strategy = FBlueprintHelperWidgetTaskPlan::Strategy::WidgetTreeEdit)
{
	TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
	Step->SetStringField(TEXT("step_id"), TEXT("step_widget"));
	Step->SetStringField(TEXT("capability"), FBlueprintHelperWidgetTaskPlan::Capability::UMGWidget);

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

static TSharedPtr<FJsonObject> MakeWidgetTaskPlan(const TSharedPtr<FJsonObject>& Step)
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

};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetTaskPlanAddWidgetLoweringTest,
	"BlueprintHelper.TaskPlan.WidgetAdapter.AddWidgetLowering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetTaskPlanAddWidgetLoweringTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
	Op->SetStringField(TEXT("op"), FBlueprintHelperWidgetTaskPlan::Op::AddWidget);
	Op->SetStringField(TEXT("parent_name"), TEXT("CanvasRoot"));
	Op->SetStringField(TEXT("widget_class"), TEXT("TextBlock"));
	Op->SetStringField(TEXT("widget_name"), TEXT("TitleText"));
	Op->SetNumberField(TEXT("virtual_index"), 1);
	Op->SetStringField(TEXT("expected_parent_name"), TEXT("CanvasRoot"));

	TArray<TSharedPtr<FJsonValue>> Ops;
	Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

	const TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetTaskPlanStep(Ops);
	const TSharedPtr<FJsonObject> TaskPlan = FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetTaskPlan(Step);

	FBlueprintHelperWidgetTaskPlanLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperWidgetTaskPlanAdapter::TryLowerTaskPlanStep(
		TaskPlan,
		Step,
		true,
		LoweredStep,
		Error);

	TestTrue(TEXT("umg_widget add_widget lowers successfully"), bLowered);
	TestEqual(TEXT("step capability preserved"), LoweredStep.Capability, FString(FBlueprintHelperWidgetTaskPlan::Capability::UMGWidget));
	TestEqual(TEXT("runtime operation is capability"), LoweredStep.RuntimeOperation, FString(FBlueprintHelperWidgetTaskPlan::Capability::UMGWidget));
	TestTrue(TEXT("widget service adapter supports true dry-run"), LoweredStep.bAdapterDryRunSupported);
	TestEqual(TEXT("one lowered widget op emitted"), LoweredStep.Ops.Num(), 1);

	if (LoweredStep.Ops.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperWidgetTaskPlanLoweredOp& LoweredOp = LoweredStep.Ops[0];
	TestEqual(TEXT("add op lowers to current service adapter name"), LoweredOp.AdapterOperation, FString(FBlueprintHelperWidgetTaskPlan::AdapterOperation::AddWidget));
	TestNotNull(TEXT("add payload exists"), LoweredOp.Payload.Get());

	FString AssetPath;
	TestTrue(TEXT("payload carries asset_path"), LoweredOp.Payload->TryGetStringField(TEXT("asset_path"), AssetPath));
	TestEqual(TEXT("asset_path matches step target"), AssetPath, FString(TEXT("/Game/UI/WBP_MainMenu")));

	FString ParentName;
	TestTrue(TEXT("payload carries parent_name for current service"), LoweredOp.Payload->TryGetStringField(TEXT("parent_name"), ParentName));
	TestEqual(TEXT("parent_name preserved"), ParentName, FString(TEXT("CanvasRoot")));

	double VirtualIndex = -1.0;
	TestTrue(TEXT("payload carries virtual_index"), LoweredOp.Payload->TryGetNumberField(TEXT("virtual_index"), VirtualIndex));
	TestEqual(TEXT("virtual_index preserved"), static_cast<int32>(FMath::RoundToInt(VirtualIndex)), 1);

	FString ExpectedParentName;
	TestTrue(TEXT("payload carries expected_parent_name"), LoweredOp.Payload->TryGetStringField(TEXT("expected_parent_name"), ExpectedParentName));
	TestEqual(TEXT("expected_parent_name preserved"), ExpectedParentName, FString(TEXT("CanvasRoot")));

	FString WidgetClass;
	TestTrue(TEXT("payload carries widget_class"), LoweredOp.Payload->TryGetStringField(TEXT("widget_class"), WidgetClass));
	TestEqual(TEXT("widget_class preserved"), WidgetClass, FString(TEXT("TextBlock")));

	FString WidgetName;
	TestTrue(TEXT("payload carries widget_name"), LoweredOp.Payload->TryGetStringField(TEXT("widget_name"), WidgetName));
	TestEqual(TEXT("widget_name preserved"), WidgetName, FString(TEXT("TitleText")));

	bool bPayloadDryRun = false;
	TestTrue(TEXT("payload carries dry_run"), LoweredOp.Payload->TryGetBoolField(TEXT("dry_run"), bPayloadDryRun));
	TestTrue(TEXT("preview payload preserves dry_run=true"), bPayloadDryRun);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetTaskPlanMoveWidgetLoweringTest,
	"BlueprintHelper.TaskPlan.WidgetAdapter.MoveWidgetLowering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetTaskPlanMoveWidgetLoweringTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
	Op->SetStringField(TEXT("op"), FBlueprintHelperWidgetTaskPlan::Op::MoveWidget);
	Op->SetStringField(TEXT("widget_name"), TEXT("TitleText"));
	Op->SetStringField(TEXT("new_parent_name"), TEXT("CanvasRoot"));
	Op->SetNumberField(TEXT("virtual_index"), 1);
	Op->SetStringField(TEXT("expected_parent_name"), TEXT("OldRoot"));
	Op->SetNumberField(TEXT("expected_virtual_index"), 0);

	TArray<TSharedPtr<FJsonValue>> Ops;
	Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

	const TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetTaskPlanStep(Ops);
	const TSharedPtr<FJsonObject> TaskPlan = FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetTaskPlan(Step);

	FBlueprintHelperWidgetTaskPlanLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperWidgetTaskPlanAdapter::TryLowerTaskPlanStep(
		TaskPlan,
		Step,
		false,
		LoweredStep,
		Error);

	TestTrue(TEXT("umg_widget move_widget lowers successfully"), bLowered);
	TestEqual(TEXT("one lowered widget op emitted"), LoweredStep.Ops.Num(), 1);
	if (LoweredStep.Ops.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperWidgetTaskPlanLoweredOp& LoweredOp = LoweredStep.Ops[0];
	TestEqual(TEXT("move op lowers to service adapter name"), LoweredOp.AdapterOperation, FString(FBlueprintHelperWidgetTaskPlan::AdapterOperation::MoveWidget));

	double VirtualIndex = -1.0;
	TestTrue(TEXT("payload carries virtual_index"), LoweredOp.Payload->TryGetNumberField(TEXT("virtual_index"), VirtualIndex));
	TestEqual(TEXT("virtual_index preserved"), static_cast<int32>(FMath::RoundToInt(VirtualIndex)), 1);

	double ExpectedVirtualIndex = -1.0;
	TestTrue(TEXT("payload carries expected_virtual_index"), LoweredOp.Payload->TryGetNumberField(TEXT("expected_virtual_index"), ExpectedVirtualIndex));
	TestEqual(TEXT("expected_virtual_index preserved"), static_cast<int32>(FMath::RoundToInt(ExpectedVirtualIndex)), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetTaskPlanSetNamedSlotContentLoweringTest,
	"BlueprintHelper.TaskPlan.WidgetAdapter.SetNamedSlotContentLowering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetTaskPlanSetNamedSlotContentLoweringTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
	Op->SetStringField(TEXT("op"), FBlueprintHelperWidgetTaskPlan::Op::SetNamedSlotContent);
	Op->SetStringField(TEXT("host_widget_name"), TEXT("DialogShell"));
	Op->SetStringField(TEXT("slot_name"), TEXT("Body"));
	Op->SetStringField(TEXT("widget_class"), TEXT("TextBlock"));
	Op->SetStringField(TEXT("widget_name"), TEXT("BodyText"));
	Op->SetNumberField(TEXT("virtual_index"), 0);
	Op->SetBoolField(TEXT("replace_existing"), true);
	Op->SetStringField(TEXT("expected_content_widget_name"), TEXT("OldBody"));

	TArray<TSharedPtr<FJsonValue>> Ops;
	Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

	const TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetTaskPlanStep(Ops);
	const TSharedPtr<FJsonObject> TaskPlan = FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetTaskPlan(Step);

	FBlueprintHelperWidgetTaskPlanLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperWidgetTaskPlanAdapter::TryLowerTaskPlanStep(
		TaskPlan,
		Step,
		false,
		LoweredStep,
		Error);

	TestTrue(TEXT("umg_widget set_named_slot_content lowers successfully"), bLowered);
	TestEqual(TEXT("one lowered widget op emitted"), LoweredStep.Ops.Num(), 1);
	if (LoweredStep.Ops.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperWidgetTaskPlanLoweredOp& LoweredOp = LoweredStep.Ops[0];
	TestEqual(TEXT("named slot op lowers to service adapter name"), LoweredOp.AdapterOperation, FString(FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetNamedSlotContent));

	FString HostWidgetName;
	TestTrue(TEXT("payload carries host_widget_name"), LoweredOp.Payload->TryGetStringField(TEXT("host_widget_name"), HostWidgetName));
	TestEqual(TEXT("host widget preserved"), HostWidgetName, FString(TEXT("DialogShell")));

	FString SlotName;
	TestTrue(TEXT("payload carries slot_name"), LoweredOp.Payload->TryGetStringField(TEXT("slot_name"), SlotName));
	TestEqual(TEXT("slot name preserved"), SlotName, FString(TEXT("Body")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetTaskPlanInsertIndexRejectedTest,
	"BlueprintHelper.TaskPlan.WidgetAdapter.InsertIndexRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetTaskPlanInsertIndexRejectedTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
	Op->SetStringField(TEXT("op"), FBlueprintHelperWidgetTaskPlan::Op::MoveWidget);
	Op->SetStringField(TEXT("widget_name"), TEXT("TitleText"));
	Op->SetStringField(TEXT("new_parent_name"), TEXT("CanvasRoot"));
	Op->SetNumberField(TEXT("insert_index"), 0);

	TArray<TSharedPtr<FJsonValue>> Ops;
	Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

	const TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetTaskPlanStep(Ops);
	const TSharedPtr<FJsonObject> TaskPlan = FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetTaskPlan(Step);

	FBlueprintHelperWidgetTaskPlanLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperWidgetTaskPlanAdapter::TryLowerTaskPlanStep(
		TaskPlan,
		Step,
		false,
		LoweredStep,
		Error);

	TestFalse(TEXT("insert_index is rejected"), bLowered);
	TestEqual(TEXT("insert_index reports stable error code"), Error.Code, FString(TEXT("unsupported_umg_widget_legacy_position_field")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetTaskPlanSetWidgetPropertyLoweringTest,
	"BlueprintHelper.TaskPlan.WidgetAdapter.SetWidgetPropertyLowering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetTaskPlanSetWidgetPropertyLoweringTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
	Op->SetStringField(TEXT("op"), FBlueprintHelperWidgetTaskPlan::Op::SetWidgetProperty);
	Op->SetStringField(TEXT("widget_name"), TEXT("TitleText"));
	Op->SetStringField(TEXT("property_path"), TEXT("Text"));
	Op->SetStringField(TEXT("value"), TEXT("Start Game"));

	TArray<TSharedPtr<FJsonValue>> Ops;
	Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

	const TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetTaskPlanStep(
		Ops,
		FBlueprintHelperWidgetTaskPlan::Strategy::WidgetPropertyEdit);
	const TSharedPtr<FJsonObject> TaskPlan = FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetTaskPlan(Step);

	FBlueprintHelperWidgetTaskPlanLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperWidgetTaskPlanAdapter::TryLowerTaskPlanStep(
		TaskPlan,
		Step,
		false,
		LoweredStep,
		Error);

	TestTrue(TEXT("umg_widget set_widget_property lowers successfully"), bLowered);
	TestTrue(TEXT("widget property adapter supports true dry-run"), LoweredStep.bAdapterDryRunSupported);
	TestEqual(TEXT("one lowered widget op emitted"), LoweredStep.Ops.Num(), 1);

	if (LoweredStep.Ops.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperWidgetTaskPlanLoweredOp& LoweredOp = LoweredStep.Ops[0];
	TestEqual(TEXT("property op lowers to current service adapter name"), LoweredOp.AdapterOperation, FString(FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetWidgetProperty));

	FString PropertyName;
	TestTrue(TEXT("payload carries property_name for current service"), LoweredOp.Payload->TryGetStringField(TEXT("property_name"), PropertyName));
	TestEqual(TEXT("property_path lowers to property_name"), PropertyName, FString(TEXT("Text")));

	FString Value;
	TestTrue(TEXT("payload carries string value for current service"), LoweredOp.Payload->TryGetStringField(TEXT("value"), Value));
	TestEqual(TEXT("value preserved as import text"), Value, FString(TEXT("Start Game")));

	bool bPayloadDryRun = true;
	TestTrue(TEXT("payload carries dry_run"), LoweredOp.Payload->TryGetBoolField(TEXT("dry_run"), bPayloadDryRun));
	TestFalse(TEXT("execute payload preserves dry_run=false"), bPayloadDryRun);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetTaskPlanRemoveWidgetLoweringTest,
	"BlueprintHelper.TaskPlan.WidgetAdapter.RemoveWidgetLowering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetTaskPlanRemoveWidgetLoweringTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
	Op->SetStringField(TEXT("op"), FBlueprintHelperWidgetTaskPlan::Op::RemoveWidget);
	Op->SetStringField(TEXT("widget_name"), TEXT("OldButton"));

	TArray<TSharedPtr<FJsonValue>> Ops;
	Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

	const TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetTaskPlanStep(Ops);
	const TSharedPtr<FJsonObject> TaskPlan = FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetTaskPlan(Step);

	FBlueprintHelperWidgetTaskPlanLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperWidgetTaskPlanAdapter::TryLowerTaskPlanStep(
		TaskPlan,
		Step,
		false,
		LoweredStep,
		Error);

	TestTrue(TEXT("umg_widget remove_widget lowers successfully"), bLowered);
	TestTrue(TEXT("widget remove adapter supports true dry-run"), LoweredStep.bAdapterDryRunSupported);
	TestEqual(TEXT("one lowered widget op emitted"), LoweredStep.Ops.Num(), 1);

	if (LoweredStep.Ops.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperWidgetTaskPlanLoweredOp& LoweredOp = LoweredStep.Ops[0];
	TestEqual(TEXT("remove op lowers to current service adapter name"), LoweredOp.AdapterOperation, FString(FBlueprintHelperWidgetTaskPlan::AdapterOperation::RemoveWidget));

	FString WidgetName;
	TestTrue(TEXT("payload carries widget_name"), LoweredOp.Payload->TryGetStringField(TEXT("widget_name"), WidgetName));
	TestEqual(TEXT("widget_name preserved"), WidgetName, FString(TEXT("OldButton")));

	bool bPayloadDryRun = true;
	TestTrue(TEXT("payload carries dry_run"), LoweredOp.Payload->TryGetBoolField(TEXT("dry_run"), bPayloadDryRun));
	TestFalse(TEXT("execute payload preserves dry_run=false"), bPayloadDryRun);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetTaskPlanSetSlotPropertyLoweringTest,
	"BlueprintHelper.TaskPlan.WidgetAdapter.SetSlotPropertyLowering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetTaskPlanSetSlotPropertyLoweringTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
	Op->SetStringField(TEXT("op"), FBlueprintHelperWidgetTaskPlan::Op::SetSlotProperty);
	Op->SetStringField(TEXT("widget_name"), TEXT("ExistingText"));
	Op->SetStringField(TEXT("property_path"), TEXT("LayoutData.Offsets.Left"));
	Op->SetStringField(TEXT("value"), TEXT("24"));
	Op->SetStringField(TEXT("expected_slot_class_path"), TEXT("/Script/UMG.CanvasPanelSlot"));

	TArray<TSharedPtr<FJsonValue>> Ops;
	Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

	const TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetTaskPlanStep(
		Ops,
		FBlueprintHelperWidgetTaskPlan::Strategy::WidgetPropertyEdit);
	const TSharedPtr<FJsonObject> TaskPlan = FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetTaskPlan(Step);

	FBlueprintHelperWidgetTaskPlanLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperWidgetTaskPlanAdapter::TryLowerTaskPlanStep(
		TaskPlan,
		Step,
		false,
		LoweredStep,
		Error);

	TestTrue(TEXT("umg_widget set_slot_property lowers successfully"), bLowered);
	TestTrue(TEXT("slot property adapter supports true dry-run"), LoweredStep.bAdapterDryRunSupported);
	TestEqual(TEXT("one lowered widget op emitted"), LoweredStep.Ops.Num(), 1);

	if (LoweredStep.Ops.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperWidgetTaskPlanLoweredOp& LoweredOp = LoweredStep.Ops[0];
	TestEqual(TEXT("slot property op lowers to service adapter name"), LoweredOp.AdapterOperation, FString(FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetSlotProperty));

	FString PropertyPath;
	TestTrue(TEXT("payload carries property_path"), LoweredOp.Payload->TryGetStringField(TEXT("property_path"), PropertyPath));
	TestEqual(TEXT("property_path preserved"), PropertyPath, FString(TEXT("LayoutData.Offsets.Left")));

	FString ExpectedSlotClassPath;
	TestTrue(TEXT("payload carries expected_slot_class_path"), LoweredOp.Payload->TryGetStringField(TEXT("expected_slot_class_path"), ExpectedSlotClassPath));
	TestEqual(TEXT("expected slot class path preserved"), ExpectedSlotClassPath, FString(TEXT("/Script/UMG.CanvasPanelSlot")));

	FString Value;
	TestTrue(TEXT("payload carries import value"), LoweredOp.Payload->TryGetStringField(TEXT("value"), Value));
	TestEqual(TEXT("value preserved as import text"), Value, FString(TEXT("24")));

	bool bPayloadDryRun = true;
	TestTrue(TEXT("payload carries dry_run"), LoweredOp.Payload->TryGetBoolField(TEXT("dry_run"), bPayloadDryRun));
	TestFalse(TEXT("execute payload preserves dry_run=false"), bPayloadDryRun);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetTaskPlanSetWidgetAsVariableLoweringTest,
	"BlueprintHelper.TaskPlan.WidgetAdapter.SetWidgetAsVariableLowering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetTaskPlanSetWidgetAsVariableLoweringTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
	Op->SetStringField(TEXT("op"), FBlueprintHelperWidgetTaskPlan::Op::SetWidgetAsVariable);
	Op->SetStringField(TEXT("widget_name"), TEXT("StartButton"));
	Op->SetBoolField(TEXT("is_variable"), true);
	Op->SetStringField(TEXT("expected_widget_class_path"), TEXT("/Script/UMG.Button"));

	TArray<TSharedPtr<FJsonValue>> Ops;
	Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

	const TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetTaskPlanStep(
		Ops,
		FBlueprintHelperWidgetTaskPlan::Strategy::WidgetTreeEdit);
	const TSharedPtr<FJsonObject> TaskPlan = FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetTaskPlan(Step);

	FBlueprintHelperWidgetTaskPlanLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperWidgetTaskPlanAdapter::TryLowerTaskPlanStep(
		TaskPlan,
		Step,
		false,
		LoweredStep,
		Error);

	TestTrue(TEXT("umg_widget set_widget_as_variable lowers successfully"), bLowered);
	TestTrue(TEXT("widget variable adapter supports true dry-run"), LoweredStep.bAdapterDryRunSupported);
	TestEqual(TEXT("one lowered widget op emitted"), LoweredStep.Ops.Num(), 1);

	if (LoweredStep.Ops.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperWidgetTaskPlanLoweredOp& LoweredOp = LoweredStep.Ops[0];
	TestEqual(TEXT("widget variable op lowers to service adapter name"), LoweredOp.AdapterOperation, FString(FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetWidgetAsVariable));

	FString WidgetName;
	TestTrue(TEXT("payload carries widget_name"), LoweredOp.Payload->TryGetStringField(TEXT("widget_name"), WidgetName));
	TestEqual(TEXT("widget_name preserved"), WidgetName, FString(TEXT("StartButton")));

	bool bIsVariable = false;
	TestTrue(TEXT("payload carries is_variable"), LoweredOp.Payload->TryGetBoolField(TEXT("is_variable"), bIsVariable));
	TestTrue(TEXT("is_variable preserved"), bIsVariable);

	FString ExpectedWidgetClassPath;
	TestTrue(TEXT("payload carries expected_widget_class_path"), LoweredOp.Payload->TryGetStringField(TEXT("expected_widget_class_path"), ExpectedWidgetClassPath));
	TestEqual(TEXT("expected widget class path preserved"), ExpectedWidgetClassPath, FString(TEXT("/Script/UMG.Button")));

	bool bPayloadDryRun = true;
	TestTrue(TEXT("payload carries dry_run"), LoweredOp.Payload->TryGetBoolField(TEXT("dry_run"), bPayloadDryRun));
	TestFalse(TEXT("execute payload preserves dry_run=false"), bPayloadDryRun);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetServiceAddWidgetDryRunDoesNotCreateWidgetTest,
	"BlueprintHelper.UMGWidget.Service.AddWidgetDryRunDoesNotCreateWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetServiceAddWidgetDryRunDoesNotCreateWidgetTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::FWidgetServiceDryRunFixture Fixture = FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetServiceDryRunFixture(TEXT("WidgetAddDryRun"));
	TestNotNull(TEXT("WidgetBlueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("root canvas is created"), Fixture.Root);

	const int32 InitialChildCount = Fixture.Root->GetChildrenCount();

	FBlueprintHelperWidgetService Service;
	const FBlueprintHelperWidgetMutationResult Result = Service.AddWidget(
		Fixture.Blueprint->GetPathName(),
		TEXT("RootCanvas"),
		TEXT("TextBlock"),
		TEXT("DryRunText"),
		true);

	TestTrue(TEXT("dry-run add succeeds"), Result.bSuccess);
	TestTrue(TEXT("mutation result records dry-run"), Result.bDryRun);
	TestEqual(TEXT("affected widget records requested name"), Result.AffectedWidget, FString(TEXT("DryRunText")));
	TestEqual(TEXT("dry-run add does not change child count"), Fixture.Root->GetChildrenCount(), InitialChildCount);
	TestNull(TEXT("dry-run add does not create requested widget"), Fixture.Blueprint->WidgetTree->FindWidget(FName(TEXT("DryRunText"))));
	TestFalse(TEXT("dry-run add does not dirty package"), Fixture.Package->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetServiceAddWidgetUsesVirtualIndexTest,
	"BlueprintHelper.UMGWidget.Service.AddWidgetUsesVirtualIndex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetServiceAddWidgetUsesVirtualIndexTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::FWidgetServiceDryRunFixture Fixture =
		FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetServiceDryRunFixture(TEXT("WidgetAddVirtualIndex"));
	TestNotNull(TEXT("WidgetBlueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("root canvas is created"), Fixture.Root);

	FBlueprintHelperAddWidgetRequest Request;
	Request.AssetPath = Fixture.Blueprint->GetPathName();
	Request.ParentName = TEXT("RootCanvas");
	Request.WidgetClass = TEXT("TextBlock");
	Request.WidgetName = TEXT("InsertedText");
	Request.VirtualIndex = 1;

	FBlueprintHelperWidgetService Service;
	const FBlueprintHelperWidgetMutationResult Result = Service.AddWidget(Request);

	TestTrue(TEXT("add by virtual_index succeeds"), Result.bSuccess);
	TestEqual(TEXT("affected widget records inserted name"), Result.AffectedWidget, FString(TEXT("InsertedText")));
	TestEqual(TEXT("root child count includes inserted widget"), Fixture.Root->GetChildrenCount(), 3);
	TestEqual(TEXT("inserted widget lands at requested virtual_index"),
		Fixture.Root->GetChildAt(1)->GetName(),
		FString(TEXT("InsertedText")));
	TestTrue(TEXT("mutation result includes readback context"), Result.ReadbackContext.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetServiceSetWidgetPropertyDryRunDoesNotModifyPropertyTest,
	"BlueprintHelper.UMGWidget.Service.SetWidgetPropertyDryRunDoesNotModifyProperty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetServiceSetWidgetPropertyDryRunDoesNotModifyPropertyTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::FWidgetServiceDryRunFixture Fixture = FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetServiceDryRunFixture(TEXT("WidgetSetPropertyDryRun"));
	TestNotNull(TEXT("WidgetBlueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("existing text widget is created"), Fixture.ExistingText);

	const float InitialRenderOpacity = Fixture.ExistingText->GetRenderOpacity();

	FBlueprintHelperWidgetService Service;
	const FBlueprintHelperWidgetMutationResult Result = Service.SetWidgetProperty(
		Fixture.Blueprint->GetPathName(),
		TEXT("ExistingText"),
		TEXT("RenderOpacity"),
		TEXT("0.25"),
		true);

	TestTrue(TEXT("dry-run property set succeeds"), Result.bSuccess);
	TestTrue(TEXT("mutation result records dry-run"), Result.bDryRun);
	TestEqual(TEXT("affected widget records target widget"), Result.AffectedWidget, FString(TEXT("ExistingText")));
	TestEqual(TEXT("dry-run set does not change render opacity"), Fixture.ExistingText->GetRenderOpacity(), InitialRenderOpacity);
	TestFalse(TEXT("dry-run set does not dirty package"), Fixture.Package->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetServiceSetSlotPropertyTest,
	"BlueprintHelper.UMGWidget.Service.SetSlotProperty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetServiceSetSlotPropertyTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::FWidgetServiceDryRunFixture Fixture =
		FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetServiceDryRunFixture(TEXT("WidgetSetSlotProperty"));
	TestNotNull(TEXT("WidgetBlueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("existing text widget is created"), Fixture.ExistingText);

	UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Fixture.ExistingText->Slot);
	TestNotNull(TEXT("existing text widget has canvas slot"), Slot);
	if (!Slot)
	{
		return false;
	}

	FAnchorData Layout = Slot->GetLayout();
	Layout.Offsets.Left = 0.0f;
	Slot->SetLayout(Layout);

	FBlueprintHelperSetSlotPropertyRequest Request;
	Request.AssetPath = Fixture.Blueprint->GetPathName();
	Request.WidgetName = TEXT("ExistingText");
	Request.PropertyPath = TEXT("LayoutData.Offsets.Left");
	Request.Value = TEXT("24");
	Request.ExpectedSlotClassPath = UCanvasPanelSlot::StaticClass()->GetPathName();

	FBlueprintHelperWidgetService Service;
	const FBlueprintHelperWidgetMutationResult Result = Service.SetSlotProperty(Request);

	TestTrue(TEXT("set slot property updates CanvasPanelSlot LayoutData Offsets.Left"), Result.bSuccess);
	TestEqual(TEXT("affected widget records target widget"), Result.AffectedWidget, FString(TEXT("ExistingText")));
	TestEqual(TEXT("slot property readback left"), Slot->GetLayout().Offsets.Left, 24.0f);
	TestTrue(TEXT("slot property result includes readback context"), Result.ReadbackContext.IsValid());
	return Result.bSuccess;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetServiceSetWidgetAsVariableTest,
	"BlueprintHelper.UMGWidget.Service.SetWidgetAsVariable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetServiceSetWidgetAsVariableTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::FWidgetServiceDryRunFixture Fixture =
		FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetServiceDryRunFixture(TEXT("WidgetSetVariable"));
	TestNotNull(TEXT("WidgetBlueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("root canvas is created"), Fixture.Root);

	UButton* StartButton = Fixture.Blueprint->WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(),
		TEXT("StartButton"));
	Fixture.Root->AddChild(StartButton);
	StartButton->bIsVariable = false;

	FBlueprintHelperSetWidgetAsVariableRequest Request;
	Request.AssetPath = Fixture.Blueprint->GetPathName();
	Request.WidgetName = TEXT("StartButton");
	Request.bIsVariable = true;
	Request.ExpectedWidgetClassPath = UButton::StaticClass()->GetPathName();

	FBlueprintHelperWidgetService Service;
	const FBlueprintHelperWidgetMutationResult Result = Service.SetWidgetAsVariable(Request);

	TestTrue(TEXT("set widget as variable true succeeds"), Result.bSuccess);
	TestEqual(TEXT("affected widget records target widget"), Result.AffectedWidget, FString(TEXT("StartButton")));
	TestTrue(TEXT("widget bIsVariable true"), StartButton->bIsVariable);
	TestTrue(TEXT("widget variable registration is updated"),
		FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::IsWidgetVariableRegistered(Fixture.Blueprint, StartButton));
	TestTrue(TEXT("widget variable result includes readback context"), Result.ReadbackContext.IsValid());
	return Result.bSuccess;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetTaskPlanP2DescriptorOpsLoweringTest,
	"BlueprintHelper.TaskPlan.WidgetAdapter.P2DescriptorOpsLowering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetTaskPlanP2DescriptorOpsLoweringTest::RunTest(const FString& Parameters)
{
	struct FCase
	{
		const TCHAR* Op;
		const TCHAR* Strategy;
		const TCHAR* AdapterOperation;
		TFunction<void(const TSharedRef<FJsonObject>&)> Fill;
		TFunction<void(FAutomationTestBase&, const TSharedPtr<FJsonObject>&)> AssertPayload;
	};

	TArray<FCase> Cases;
	Cases.Add({
		FBlueprintHelperWidgetTaskPlan::Op::RenameWidget,
		FBlueprintHelperWidgetTaskPlan::Strategy::WidgetTreeEdit,
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::RenameWidget,
		[](const TSharedRef<FJsonObject>& Op)
		{
			Op->SetStringField(TEXT("widget_name"), TEXT("OldButton"));
			Op->SetStringField(TEXT("new_widget_name"), TEXT("StartButton"));
			Op->SetStringField(TEXT("expected_widget_class_path"), TEXT("/Script/UMG.Button"));
		},
		[](FAutomationTestBase& Test, const TSharedPtr<FJsonObject>& Payload)
		{
			FString NewWidgetName;
			Test.TestTrue(TEXT("rename payload carries new_widget_name"), Payload->TryGetStringField(TEXT("new_widget_name"), NewWidgetName));
			Test.TestEqual(TEXT("new_widget_name preserved"), NewWidgetName, FString(TEXT("StartButton")));
		}
	});
	Cases.Add({
		FBlueprintHelperWidgetTaskPlan::Op::RemoveRootWidget,
		FBlueprintHelperWidgetTaskPlan::Strategy::WidgetTreeEdit,
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::RemoveRootWidget,
		[](const TSharedRef<FJsonObject>& Op)
		{
			Op->SetStringField(TEXT("root_widget_name"), TEXT("RootCanvas"));
			Op->SetStringField(TEXT("replacement_policy"), TEXT("replace_with_empty_root"));
			Op->SetStringField(TEXT("replacement_widget_class"), TEXT("CanvasPanel"));
			Op->SetStringField(TEXT("replacement_widget_name"), TEXT("RootCanvas"));
			Op->SetStringField(TEXT("expected_root_class_path"), TEXT("/Script/UMG.CanvasPanel"));
		},
		[](FAutomationTestBase& Test, const TSharedPtr<FJsonObject>& Payload)
		{
			FString ExpectedRootClassPath;
			Test.TestTrue(TEXT("remove root payload carries expected_root_class_path"), Payload->TryGetStringField(TEXT("expected_root_class_path"), ExpectedRootClassPath));
			Test.TestEqual(TEXT("expected root class path preserved"), ExpectedRootClassPath, FString(TEXT("/Script/UMG.CanvasPanel")));
		}
	});
	Cases.Add({
		FBlueprintHelperWidgetTaskPlan::Op::ReparentWidgetBlueprint,
		FBlueprintHelperWidgetTaskPlan::Strategy::WidgetBlueprintClassEdit,
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::ReparentWidgetBlueprint,
		[](const TSharedRef<FJsonObject>& Op)
		{
			Op->SetStringField(TEXT("new_parent_class"), TEXT("/Script/UMG.UserWidget"));
			Op->SetStringField(TEXT("expected_parent_class"), TEXT("/Script/UMG.UserWidget"));
		},
		[](FAutomationTestBase& Test, const TSharedPtr<FJsonObject>& Payload)
		{
			FString NewParentClass;
			Test.TestTrue(TEXT("reparent payload carries new_parent_class"), Payload->TryGetStringField(TEXT("new_parent_class"), NewParentClass));
			Test.TestEqual(TEXT("new parent class preserved"), NewParentClass, FString(TEXT("/Script/UMG.UserWidget")));
		}
	});
	Cases.Add({
		FBlueprintHelperWidgetTaskPlan::Op::DuplicateWidgetSubtree,
		FBlueprintHelperWidgetTaskPlan::Strategy::WidgetTreeEdit,
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::DuplicateWidgetSubtree,
		[](const TSharedRef<FJsonObject>& Op)
		{
			Op->SetStringField(TEXT("source_widget_name"), TEXT("SourcePanel"));
			Op->SetStringField(TEXT("target_parent_name"), TEXT("RootCanvas"));
			TSharedRef<FJsonObject> NameMapping = MakeShared<FJsonObject>();
			NameMapping->SetStringField(TEXT("SourcePanel"), TEXT("SourcePanelCopy"));
			Op->SetObjectField(TEXT("name_mapping"), NameMapping);
			Op->SetNumberField(TEXT("virtual_index"), 1);
		},
		[](FAutomationTestBase& Test, const TSharedPtr<FJsonObject>& Payload)
		{
			const TSharedPtr<FJsonObject>* NameMapping = nullptr;
			Test.TestTrue(TEXT("duplicate payload carries name_mapping"), Payload->TryGetObjectField(TEXT("name_mapping"), NameMapping));
			Test.TestTrue(TEXT("duplicate name_mapping object is valid"), NameMapping && NameMapping->IsValid());
		}
	});
	Cases.Add({
		FBlueprintHelperWidgetTaskPlan::Op::WrapWidget,
		FBlueprintHelperWidgetTaskPlan::Strategy::WidgetTreeEdit,
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::WrapWidget,
		[](const TSharedRef<FJsonObject>& Op)
		{
			Op->SetStringField(TEXT("widget_name"), TEXT("StartButton"));
			Op->SetStringField(TEXT("wrapper_class"), TEXT("CanvasPanel"));
			Op->SetStringField(TEXT("wrapper_name"), TEXT("StartButtonWrapper"));
		},
		[](FAutomationTestBase& Test, const TSharedPtr<FJsonObject>& Payload)
		{
			FString WrapperName;
			Test.TestTrue(TEXT("wrap payload carries wrapper_name"), Payload->TryGetStringField(TEXT("wrapper_name"), WrapperName));
			Test.TestEqual(TEXT("wrapper name preserved"), WrapperName, FString(TEXT("StartButtonWrapper")));
		}
	});
	Cases.Add({
		FBlueprintHelperWidgetTaskPlan::Op::ReplaceWidgetClass,
		FBlueprintHelperWidgetTaskPlan::Strategy::WidgetTreeEdit,
		FBlueprintHelperWidgetTaskPlan::AdapterOperation::ReplaceWidgetClass,
		[](const TSharedRef<FJsonObject>& Op)
		{
			Op->SetStringField(TEXT("widget_name"), TEXT("StartButton"));
			Op->SetStringField(TEXT("new_widget_class"), TEXT("TextBlock"));
			Op->SetStringField(TEXT("expected_widget_class_path"), TEXT("/Script/UMG.Button"));
			Op->SetBoolField(TEXT("preserve_children"), false);
			Op->SetBoolField(TEXT("preserve_slot"), true);
		},
		[](FAutomationTestBase& Test, const TSharedPtr<FJsonObject>& Payload)
		{
			bool bPreserveSlot = false;
			Test.TestTrue(TEXT("replace payload carries preserve_slot"), Payload->TryGetBoolField(TEXT("preserve_slot"), bPreserveSlot));
			Test.TestTrue(TEXT("preserve_slot preserved"), bPreserveSlot);
		}
	});

	for (const FCase& Case : Cases)
	{
		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), Case.Op);
		Case.Fill(Op);

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op));
		const TSharedPtr<FJsonObject> Step =
			FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetTaskPlanStep(Ops, Case.Strategy);
		const TSharedPtr<FJsonObject> TaskPlan =
			FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetTaskPlan(Step);

		FBlueprintHelperWidgetTaskPlanLoweredStep LoweredStep;
		FBlueprintHelperToolError Error;
		const bool bLowered = FBlueprintHelperWidgetTaskPlanAdapter::TryLowerTaskPlanStep(
			TaskPlan,
			Step,
			false,
			LoweredStep,
			Error);

		TestTrue(FString::Printf(TEXT("%s lowers successfully"), Case.Op), bLowered);
		TestEqual(FString::Printf(TEXT("%s emits one op"), Case.Op), LoweredStep.Ops.Num(), 1);
		if (!bLowered || LoweredStep.Ops.Num() != 1)
		{
			AddError(FString::Printf(TEXT("P2 op %s failed with error %s"), Case.Op, *Error.Code));
			continue;
		}

		const FBlueprintHelperWidgetTaskPlanLoweredOp& LoweredOp = LoweredStep.Ops[0];
		TestEqual(FString::Printf(TEXT("%s adapter operation"), Case.Op), LoweredOp.AdapterOperation, FString(Case.AdapterOperation));
		TestNotNull(FString::Printf(TEXT("%s payload exists"), Case.Op), LoweredOp.Payload.Get());
		Case.AssertPayload(*this, LoweredOp.Payload);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetServiceRemoveWidgetDryRunDoesNotDeleteWidgetTest,
	"BlueprintHelper.UMGWidget.Service.RemoveWidgetDryRunDoesNotDeleteWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetServiceRemoveWidgetDryRunDoesNotDeleteWidgetTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::FWidgetServiceDryRunFixture Fixture = FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetServiceDryRunFixture(TEXT("WidgetRemoveDryRun"));
	TestNotNull(TEXT("WidgetBlueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("existing text widget is created"), Fixture.ExistingText);

	const int32 InitialChildCount = Fixture.Root->GetChildrenCount();

	FBlueprintHelperWidgetService Service;
	const FBlueprintHelperWidgetMutationResult Result = Service.RemoveWidget(
		Fixture.Blueprint->GetPathName(),
		TEXT("ExistingText"),
		true);

	TestTrue(TEXT("dry-run remove succeeds"), Result.bSuccess);
	TestTrue(TEXT("mutation result records dry-run"), Result.bDryRun);
	TestEqual(TEXT("affected widget records target widget"), Result.AffectedWidget, FString(TEXT("ExistingText")));
	TestEqual(TEXT("dry-run remove does not change child count"), Fixture.Root->GetChildrenCount(), InitialChildCount);
	TestEqual(TEXT("dry-run remove keeps widget in tree"), Fixture.Blueprint->WidgetTree->FindWidget(FName(TEXT("ExistingText"))), Cast<UWidget>(Fixture.ExistingText));
	TestFalse(TEXT("dry-run remove does not dirty package"), Fixture.Package->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetServiceMoveWidgetUsesVirtualIndexTest,
	"BlueprintHelper.UMGWidget.Service.MoveWidgetUsesVirtualIndex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetServiceMoveWidgetUsesVirtualIndexTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::FWidgetServiceDryRunFixture Fixture =
		FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetServiceDryRunFixture(TEXT("WidgetMoveVirtualIndex"));
	TestNotNull(TEXT("WidgetBlueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("root canvas is created"), Fixture.Root);

	FBlueprintHelperMoveWidgetRequest Request;
	Request.AssetPath = Fixture.Blueprint->GetPathName();
	Request.WidgetName = TEXT("ExistingText");
	Request.NewParentName = TEXT("RootCanvas");
	Request.VirtualIndex = 1;
	Request.ExpectedParentName = TEXT("RootCanvas");
	Request.ExpectedVirtualIndex = 0;

	FBlueprintHelperWidgetService Service;
	const FBlueprintHelperWidgetMutationResult Result = Service.MoveWidget(Request);

	TestTrue(TEXT("move by virtual_index succeeds"), Result.bSuccess);
	TestEqual(TEXT("moved widget lands at requested virtual_index"),
		Fixture.Root->GetChildAt(1)->GetName(),
		FString(TEXT("ExistingText")));
	TestTrue(TEXT("move result includes readback context"), Result.ReadbackContext.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetServiceMoveWidgetToNamedSlotDetachesOldParentTest,
	"BlueprintHelper.UMGWidget.Service.MoveWidgetToNamedSlotDetachesOldParent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetServiceMoveWidgetToNamedSlotDetachesOldParentTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::FWidgetServiceDryRunFixture Fixture =
		FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetServiceDryRunFixture(TEXT("WidgetMoveNamedSlotDetach"));
	TestNotNull(TEXT("WidgetBlueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("root canvas is created"), Fixture.Root);

	Fixture.NamedSlotHost = Fixture.Blueprint->WidgetTree->ConstructWidget<UExpandableArea>(
		UExpandableArea::StaticClass(),
		TEXT("DialogShell"));
	Fixture.Root->AddChild(Fixture.NamedSlotHost);
	FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(Fixture.Blueprint, Fixture.NamedSlotHost);

	FBlueprintHelperMoveWidgetRequest Request;
	Request.AssetPath = Fixture.Blueprint->GetPathName();
	Request.WidgetName = TEXT("ExistingText");
	Request.NewParentName = TEXT("DialogShell");
	Request.SlotName = TEXT("Body");
	Request.VirtualIndex = 0;
	Request.ExpectedParentName = TEXT("RootCanvas");
	Request.ExpectedVirtualIndex = 0;

	FBlueprintHelperWidgetService Service;
	const FBlueprintHelperWidgetMutationResult Result = Service.MoveWidget(Request);

	TestTrue(TEXT("move to named slot succeeds"), Result.bSuccess);
	TestEqual(
		TEXT("named slot receives moved widget"),
		Fixture.NamedSlotHost->GetContentForSlot(FName(TEXT("Body"))),
		Cast<UWidget>(Fixture.ExistingText));
	for (int32 ChildIndex = 0; ChildIndex < Fixture.Root->GetChildrenCount(); ++ChildIndex)
	{
		TestNotEqual(
			TEXT("old panel parent no longer owns moved widget"),
			Fixture.Root->GetChildAt(ChildIndex),
			Cast<UWidget>(Fixture.ExistingText));
	}
	return Result.bSuccess;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetServiceMoveWidgetToNamedSlotClearsOldNamedSlotTest,
	"BlueprintHelper.UMGWidget.Service.MoveWidgetToNamedSlotClearsOldNamedSlot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetServiceMoveWidgetToNamedSlotClearsOldNamedSlotTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::FWidgetServiceDryRunFixture Fixture =
		FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetServiceDryRunFixture(TEXT("WidgetMoveOldNamedSlotDetach"));
	TestNotNull(TEXT("WidgetBlueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("root canvas is created"), Fixture.Root);

	Fixture.NamedSlotHost = Fixture.Blueprint->WidgetTree->ConstructWidget<UExpandableArea>(
		UExpandableArea::StaticClass(),
		TEXT("SourceShell"));
	Fixture.Root->AddChild(Fixture.NamedSlotHost);
	FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(Fixture.Blueprint, Fixture.NamedSlotHost);

	UExpandableArea* TargetHost = Fixture.Blueprint->WidgetTree->ConstructWidget<UExpandableArea>(
		UExpandableArea::StaticClass(),
		TEXT("TargetShell"));
	Fixture.Root->AddChild(TargetHost);
	FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(Fixture.Blueprint, TargetHost);

	UTextBlock* SlottedText = Fixture.Blueprint->WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("SlottedText"));
	FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(Fixture.Blueprint, SlottedText);
	Fixture.NamedSlotHost->SetContentForSlot(FName(TEXT("Body")), SlottedText);

	FBlueprintHelperMoveWidgetRequest Request;
	Request.AssetPath = Fixture.Blueprint->GetPathName();
	Request.WidgetName = TEXT("SlottedText");
	Request.NewParentName = TEXT("TargetShell");
	Request.SlotName = TEXT("Body");
	Request.VirtualIndex = 0;
	Request.ExpectedParentName = TEXT("SourceShell");
	Request.ExpectedVirtualIndex = 0;

	FBlueprintHelperWidgetService Service;
	const FBlueprintHelperWidgetMutationResult Result = Service.MoveWidget(Request);

	TestTrue(TEXT("move from old named slot to new named slot succeeds"), Result.bSuccess);
	TestNull(
		TEXT("old named slot is cleared"),
		Fixture.NamedSlotHost->GetContentForSlot(FName(TEXT("Body"))));
	TestEqual(
		TEXT("target named slot receives moved widget"),
		TargetHost->GetContentForSlot(FName(TEXT("Body"))),
		Cast<UWidget>(SlottedText));
	return Result.bSuccess;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetServiceSetNamedSlotContentTest,
	"BlueprintHelper.UMGWidget.Service.SetNamedSlotContent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetServiceSetNamedSlotContentTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::FWidgetServiceDryRunFixture Fixture =
		FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetServiceDryRunFixture(TEXT("WidgetNamedSlotContent"));
	TestNotNull(TEXT("WidgetBlueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("root canvas is created"), Fixture.Root);

	Fixture.NamedSlotHost = Fixture.Blueprint->WidgetTree->ConstructWidget<UExpandableArea>(
		UExpandableArea::StaticClass(),
		TEXT("DialogShell"));
	Fixture.Root->AddChild(Fixture.NamedSlotHost);
	FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(Fixture.Blueprint, Fixture.NamedSlotHost);

	FBlueprintHelperSetNamedSlotContentRequest Request;
	Request.AssetPath = Fixture.Blueprint->GetPathName();
	Request.HostWidgetName = TEXT("DialogShell");
	Request.SlotName = TEXT("Body");
	Request.WidgetClass = TEXT("TextBlock");
	Request.WidgetName = TEXT("BodyText");
	Request.VirtualIndex = 0;

	FBlueprintHelperWidgetService Service;
	const FBlueprintHelperWidgetMutationResult Result = Service.SetNamedSlotContent(Request);

	TestTrue(TEXT("set named slot content succeeds"), Result.bSuccess);
	TestEqual(TEXT("affected widget records slot content"), Result.AffectedWidget, FString(TEXT("BodyText")));
	TestNotNull(TEXT("named slot body has content"),
		Fixture.NamedSlotHost->GetContentForSlot(FName(TEXT("Body"))));
	TestEqual(TEXT("named slot body content has requested name"),
		Fixture.NamedSlotHost->GetContentForSlot(FName(TEXT("Body")))->GetName(),
		FString(TEXT("BodyText")));
	TestTrue(TEXT("named slot result includes readback context"), Result.ReadbackContext.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetServiceAddWidgetRejectsDuplicateNameTest,
	"BlueprintHelper.UMGWidget.Service.AddWidgetRejectsDuplicateName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetServiceAddWidgetRejectsDuplicateNameTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::FWidgetServiceDryRunFixture Fixture =
		FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetServiceDryRunFixture(TEXT("WidgetAddDuplicateName"));
	TestNotNull(TEXT("WidgetBlueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("root canvas is created"), Fixture.Root);

	const int32 InitialChildCount = Fixture.Root ? Fixture.Root->GetChildrenCount() : 0;
	FBlueprintHelperAddWidgetRequest Request;
	Request.AssetPath = Fixture.Blueprint->GetPathName();
	Request.ParentName = TEXT("RootCanvas");
	Request.WidgetClass = TEXT("TextBlock");
	Request.WidgetName = TEXT("ExistingText");
	Request.VirtualIndex = 1;

	FBlueprintHelperWidgetService Service;
	const FBlueprintHelperWidgetMutationResult Result = Service.AddWidget(Request);

	TestFalse(TEXT("duplicate add is rejected"), Result.bSuccess);
	TestEqual(TEXT("duplicate add reports stable error"), Result.ErrorMessage, FString(TEXT("widget_name_already_exists")));
	TestEqual(TEXT("duplicate add does not change child count"), Fixture.Root->GetChildrenCount(), InitialChildCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetServiceSetNamedSlotContentRejectsDuplicateNameTest,
	"BlueprintHelper.UMGWidget.Service.SetNamedSlotContentRejectsDuplicateName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetServiceSetNamedSlotContentRejectsDuplicateNameTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::FWidgetServiceDryRunFixture Fixture =
		FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetServiceDryRunFixture(TEXT("WidgetNamedSlotDuplicateName"));
	TestNotNull(TEXT("WidgetBlueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("root canvas is created"), Fixture.Root);

	Fixture.NamedSlotHost = Fixture.Blueprint->WidgetTree->ConstructWidget<UExpandableArea>(
		UExpandableArea::StaticClass(),
		TEXT("DialogShell"));
	Fixture.Root->AddChild(Fixture.NamedSlotHost);
	FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(Fixture.Blueprint, Fixture.NamedSlotHost);

	FBlueprintHelperSetNamedSlotContentRequest Request;
	Request.AssetPath = Fixture.Blueprint->GetPathName();
	Request.HostWidgetName = TEXT("DialogShell");
	Request.SlotName = TEXT("Body");
	Request.WidgetClass = TEXT("TextBlock");
	Request.WidgetName = TEXT("ExistingText");
	Request.VirtualIndex = 0;

	FBlueprintHelperWidgetService Service;
	const FBlueprintHelperWidgetMutationResult Result = Service.SetNamedSlotContent(Request);

	TestFalse(TEXT("duplicate named slot content is rejected"), Result.bSuccess);
	TestEqual(TEXT("duplicate named slot reports stable error"), Result.ErrorMessage, FString(TEXT("widget_name_already_exists")));
	TestNull(TEXT("named slot remains empty"), Fixture.NamedSlotHost->GetContentForSlot(FName(TEXT("Body"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetServiceSetNamedSlotContentReplaceUnregistersOldWidgetTest,
	"BlueprintHelper.UMGWidget.Service.SetNamedSlotContentReplaceUnregistersOldWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetServiceSetNamedSlotContentReplaceUnregistersOldWidgetTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::FWidgetServiceDryRunFixture Fixture =
		FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetServiceDryRunFixture(TEXT("WidgetNamedSlotReplace"));
	TestNotNull(TEXT("WidgetBlueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("root canvas is created"), Fixture.Root);

	Fixture.NamedSlotHost = Fixture.Blueprint->WidgetTree->ConstructWidget<UExpandableArea>(
		UExpandableArea::StaticClass(),
		TEXT("DialogShell"));
	Fixture.Root->AddChild(Fixture.NamedSlotHost);
	FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(Fixture.Blueprint, Fixture.NamedSlotHost);

	UTextBlock* OldBody = Fixture.Blueprint->WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("OldBody"));
	FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(Fixture.Blueprint, OldBody);
	Fixture.NamedSlotHost->SetContentForSlot(FName(TEXT("Body")), OldBody);
	TestTrue(
		TEXT("old body starts registered"),
		FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::IsWidgetVariableRegistered(Fixture.Blueprint, OldBody));

	FBlueprintHelperSetNamedSlotContentRequest Request;
	Request.AssetPath = Fixture.Blueprint->GetPathName();
	Request.HostWidgetName = TEXT("DialogShell");
	Request.SlotName = TEXT("Body");
	Request.WidgetClass = TEXT("TextBlock");
	Request.WidgetName = TEXT("NewBody");
	Request.VirtualIndex = 0;
	Request.ExpectedContentWidgetName = TEXT("OldBody");
	Request.bReplaceExisting = true;

	FBlueprintHelperWidgetService Service;
	const FBlueprintHelperWidgetMutationResult Result = Service.SetNamedSlotContent(Request);
	UWidget* NewBody = Fixture.NamedSlotHost->GetContentForSlot(FName(TEXT("Body")));

	TestTrue(TEXT("replace named slot succeeds"), Result.bSuccess);
	TestNotNull(TEXT("new named slot content exists"), NewBody);
	TestFalse(
		TEXT("old named slot content variable is unregistered"),
		FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::IsWidgetVariableRegistered(Fixture.Blueprint, OldBody));
	TestTrue(
		TEXT("new named slot content variable is registered"),
		FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::IsWidgetVariableRegistered(Fixture.Blueprint, NewBody));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetServiceSetNamedSlotContentReusesRetiringSubtreeNameTest,
	"BlueprintHelper.UMGWidget.Service.SetNamedSlotContentReusesRetiringSubtreeName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetServiceSetNamedSlotContentReusesRetiringSubtreeNameTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::FWidgetServiceDryRunFixture Fixture =
		FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetServiceDryRunFixture(TEXT("WidgetNamedSlotRetiringSubtreeName"));
	TestNotNull(TEXT("WidgetBlueprint is created"), Fixture.Blueprint);
	TestNotNull(TEXT("root canvas is created"), Fixture.Root);

	Fixture.NamedSlotHost = Fixture.Blueprint->WidgetTree->ConstructWidget<UExpandableArea>(
		UExpandableArea::StaticClass(),
		TEXT("DialogShell"));
	Fixture.Root->AddChild(Fixture.NamedSlotHost);
	FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(Fixture.Blueprint, Fixture.NamedSlotHost);

	UCanvasPanel* OldBody = Fixture.Blueprint->WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("OldBodyPanel"));
	FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(Fixture.Blueprint, OldBody);
	UTextBlock* RetiringChild = Fixture.Blueprint->WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("ReusableName"));
	OldBody->AddChild(RetiringChild);
	FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(Fixture.Blueprint, RetiringChild);
	Fixture.NamedSlotHost->SetContentForSlot(FName(TEXT("Body")), OldBody);

	FBlueprintHelperSetNamedSlotContentRequest Request;
	Request.AssetPath = Fixture.Blueprint->GetPathName();
	Request.HostWidgetName = TEXT("DialogShell");
	Request.SlotName = TEXT("Body");
	Request.WidgetClass = TEXT("TextBlock");
	Request.WidgetName = TEXT("ReusableName");
	Request.VirtualIndex = 0;
	Request.ExpectedContentWidgetName = TEXT("OldBodyPanel");
	Request.bReplaceExisting = true;

	FBlueprintHelperWidgetService Service;
	const FBlueprintHelperWidgetMutationResult Result = Service.SetNamedSlotContent(Request);
	UWidget* NewBody = Fixture.NamedSlotHost->GetContentForSlot(FName(TEXT("Body")));

	TestTrue(TEXT("replace can reuse name from retiring subtree"), Result.bSuccess);
	TestNotNull(TEXT("new named slot content exists"), NewBody);
	TestEqual(TEXT("new named slot content uses requested name"), NewBody ? NewBody->GetName() : FString(), FString(TEXT("ReusableName")));
	TestEqual(
		TEXT("retiring child is moved out of the WidgetBlueprint package"),
		RetiringChild->GetOutermost(),
		GetTransientPackage());
	TestTrue(
		TEXT("new reused-name widget variable is registered"),
		FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::IsWidgetVariableRegistered(Fixture.Blueprint, NewBody));
	return Result.bSuccess;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperWidgetTaskPlanOperationFieldRejectedTest,
	"BlueprintHelper.TaskPlan.WidgetAdapter.OperationFieldRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperWidgetTaskPlanOperationFieldRejectedTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
	Op->SetStringField(TEXT("op"), FBlueprintHelperWidgetTaskPlan::Op::AddWidget);
	Op->SetStringField(TEXT("widget_class"), TEXT("TextBlock"));
	Op->SetStringField(TEXT("widget_name"), TEXT("TitleText"));

	TArray<TSharedPtr<FJsonValue>> Ops;
	Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

	const TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetTaskPlanStep(Ops);
	Step->SetStringField(TEXT("operation"), TEXT("add_widget_to_tree"));

	const TSharedPtr<FJsonObject> TaskPlan = FBlueprintHelperTaskPlanWidgetAdapterTestsLocalUtils::MakeWidgetTaskPlan(Step);

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
