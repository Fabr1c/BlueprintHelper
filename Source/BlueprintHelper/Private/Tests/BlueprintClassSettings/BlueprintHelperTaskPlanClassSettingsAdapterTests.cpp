#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "TaskRuntime/BlueprintHelperTaskRuntimeService.h"
#include "TaskRuntime/TaskPlanAdapters/BlueprintClassSettings/BlueprintHelperClassSettingsTaskPlanAdapter.h"

namespace
{
	TSharedPtr<FJsonObject> MakeClassSettingsStep(const FString& OpName)
	{
		TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
		Step->SetStringField(TEXT("step_id"), TEXT("step_class_settings"));
		Step->SetStringField(TEXT("capability"), TEXT("blueprint_class_settings"));

		TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), TEXT("/Game/Blueprints/BP_Door"));
		Step->SetObjectField(TEXT("target"), Target);

		TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), OpName);

		if (OpName == TEXT("add_implemented_interfaces") || OpName == TEXT("remove_implemented_interfaces"))
		{
			TArray<TSharedPtr<FJsonValue>> InterfacePaths;
			InterfacePaths.Add(MakeShared<FJsonValueString>(TEXT("/Game/Interfaces/BPI_Interact")));
			InterfacePaths.Add(MakeShared<FJsonValueString>(TEXT("/Game/Interfaces/BPI_Usable")));
			Op->SetArrayField(TEXT("interface_paths"), InterfacePaths);
		}
		else if (OpName == TEXT("set_class_default_properties"))
		{
			TSharedPtr<FJsonObject> Setting = MakeShared<FJsonObject>();
			Setting->SetStringField(TEXT("property_path"), TEXT("OpenKickImpulse"));
			Setting->SetField(TEXT("value"), MakeShared<FJsonValueNumber>(1200.0));

			TArray<TSharedPtr<FJsonValue>> Settings;
			Settings.Add(MakeShared<FJsonValueObject>(Setting.ToSharedRef()));
			Op->SetArrayField(TEXT("settings"), Settings);
		}

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

		TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), TEXT("class_settings"));
		Write->SetArrayField(TEXT("ops"), Ops);
		Step->SetObjectField(TEXT("write"), Write);

		return Step;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanClassSettingsAdapterAddInterfacesTest,
	"BlueprintHelper.TaskPlan.ClassSettingsAdapter.AddImplementedInterfaces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanClassSettingsAdapterAddInterfacesTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Step = MakeClassSettingsStep(TEXT("add_implemented_interfaces"));

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperClassSettingsTaskPlanAdapter::TryLowerTaskPlanStep(
		TSharedPtr<FJsonObject>(),
		Step,
		true,
		LoweredStep,
		Error);

	TestTrue(TEXT("add_implemented_interfaces lowers successfully"), bLowered);
	TestEqual(TEXT("capability is blueprint_class_settings"), LoweredStep.Capability, FString(TEXT("blueprint_class_settings")));
	TestEqual(TEXT("runtime operation is blueprint_class_settings"), LoweredStep.RuntimeOperation, FString(TEXT("blueprint_class_settings")));
	TestEqual(TEXT("adapter operation is add_implemented_interfaces"), LoweredStep.AdapterOperation, FString(TEXT("add_implemented_interfaces")));
	TestFalse(TEXT("class settings adapter does not claim true dry-run support yet"), LoweredStep.bAdapterDryRunSupported);
	TestNotNull(TEXT("lowered payload exists"), LoweredStep.Payload.Get());
	if (!bLowered || !LoweredStep.Payload.IsValid())
	{
		return false;
	}

	FString AssetPath;
	TestTrue(TEXT("payload carries asset_path"), LoweredStep.Payload->TryGetStringField(TEXT("asset_path"), AssetPath));
	TestEqual(TEXT("payload asset_path matches target"), AssetPath, FString(TEXT("/Game/Blueprints/BP_Door")));

	const TArray<TSharedPtr<FJsonValue>>* InterfacePaths = nullptr;
	TestTrue(TEXT("payload carries interface_paths"), LoweredStep.Payload->TryGetArrayField(TEXT("interface_paths"), InterfacePaths));
	if (!InterfacePaths)
	{
		return false;
	}
	TestEqual(TEXT("payload keeps both interfaces"), InterfacePaths->Num(), 2);

	FString FirstInterface;
	TestTrue(TEXT("first interface is a string"), (*InterfacePaths)[0]->TryGetString(FirstInterface));
	TestEqual(TEXT("first interface path preserved"), FirstInterface, FString(TEXT("/Game/Interfaces/BPI_Interact")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanClassSettingsAdapterRemoveInterfacesTest,
	"BlueprintHelper.TaskPlan.ClassSettingsAdapter.RemoveImplementedInterfaces",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanClassSettingsAdapterRemoveInterfacesTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Step = MakeClassSettingsStep(TEXT("remove_implemented_interfaces"));

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperClassSettingsTaskPlanAdapter::TryLowerTaskPlanStep(
		TSharedPtr<FJsonObject>(),
		Step,
		false,
		LoweredStep,
		Error);

	TestTrue(TEXT("remove_implemented_interfaces lowers successfully"), bLowered);
	TestEqual(TEXT("adapter operation is remove_implemented_interfaces"), LoweredStep.AdapterOperation, FString(TEXT("remove_implemented_interfaces")));
	TestNotNull(TEXT("lowered payload exists"), LoweredStep.Payload.Get());
	if (!bLowered || !LoweredStep.Payload.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* InterfacePaths = nullptr;
	TestTrue(TEXT("payload carries interface_paths"), LoweredStep.Payload->TryGetArrayField(TEXT("interface_paths"), InterfacePaths));
	if (!InterfacePaths)
	{
		return false;
	}
	TestEqual(TEXT("payload keeps both interfaces"), InterfacePaths->Num(), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanClassSettingsAdapterSetDefaultsTest,
	"BlueprintHelper.TaskPlan.ClassSettingsAdapter.SetClassDefaultProperties",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanClassSettingsAdapterSetDefaultsTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Step = MakeClassSettingsStep(TEXT("set_class_default_properties"));

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperClassSettingsTaskPlanAdapter::TryLowerTaskPlanStep(
		TSharedPtr<FJsonObject>(),
		Step,
		false,
		LoweredStep,
		Error);

	TestTrue(TEXT("set_class_default_properties lowers successfully"), bLowered);
	TestEqual(TEXT("adapter operation is set_class_default_properties"), LoweredStep.AdapterOperation, FString(TEXT("set_class_default_properties")));
	TestNotNull(TEXT("lowered payload exists"), LoweredStep.Payload.Get());
	if (!bLowered || !LoweredStep.Payload.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Settings = nullptr;
	TestTrue(TEXT("payload carries settings"), LoweredStep.Payload->TryGetArrayField(TEXT("settings"), Settings));
	if (!Settings || Settings->Num() == 0)
	{
		return false;
	}
	TestEqual(TEXT("payload keeps one setting"), Settings->Num(), 1);

	const TSharedPtr<FJsonObject> Setting = (*Settings)[0]->AsObject();
	if (!Setting.IsValid())
	{
		return false;
	}
	FString PropertyPath;
	TestTrue(TEXT("setting carries property_path"), Setting->TryGetStringField(TEXT("property_path"), PropertyPath));
	TestEqual(TEXT("property_path preserved"), PropertyPath, FString(TEXT("OpenKickImpulse")));

	const TSharedPtr<FJsonValue>* Value = Setting->Values.Find(TEXT("value"));
	TestNotNull(TEXT("setting keeps value field"), Value ? Value->Get() : nullptr);
	if (!Value)
	{
		return false;
	}
	TestEqual(TEXT("numeric value preserved"), (*Value)->AsNumber(), 1200.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanClassSettingsAdapterRejectsParentClassTest,
	"BlueprintHelper.TaskPlan.ClassSettingsAdapter.RejectsParentClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanClassSettingsAdapterRejectsParentClassTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Step = MakeClassSettingsStep(TEXT("set_parent_class"));

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperClassSettingsTaskPlanAdapter::TryLowerTaskPlanStep(
		TSharedPtr<FJsonObject>(),
		Step,
		false,
		LoweredStep,
		Error);

	TestFalse(TEXT("parent class ops are not lowered"), bLowered);
	TestEqual(TEXT("parent class rejection code"), Error.Code, FString(TEXT("unsupported_class_settings_parent_class_op")));
	TestEqual(TEXT("parent class rejection stage"), Error.Stage, EBlueprintHelperToolStage::ParseInput);
	TestEqual(TEXT("parent class rejection field"), Error.Field, FString(TEXT("task_plan.steps[0].write.ops[0].op")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanClassSettingsAdapterRejectsOperationFieldTest,
	"BlueprintHelper.TaskPlan.ClassSettingsAdapter.RejectsOperationField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanClassSettingsAdapterRejectsOperationFieldTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Step = MakeClassSettingsStep(TEXT("add_implemented_interfaces"));
	Step->SetStringField(TEXT("operation"), TEXT("add_implemented_interfaces"));

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError Error;
	const bool bLowered = FBlueprintHelperClassSettingsTaskPlanAdapter::TryLowerTaskPlanStep(
		TSharedPtr<FJsonObject>(),
		Step,
		false,
		LoweredStep,
		Error);

	TestFalse(TEXT("IR step rejects adapter operation field"), bLowered);
	TestEqual(TEXT("operation field rejection code"), Error.Code, FString(TEXT("unsupported_class_settings_operation_field")));
	TestEqual(TEXT("operation field rejection field"), Error.Field, FString(TEXT("task_plan.steps[0].operation")));

	return true;
}

#endif
