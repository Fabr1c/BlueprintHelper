#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintComponent/BlueprintHelperComponentTaskPlanAdapter.h"

class FBlueprintHelperTaskPlanComponentAdapterTestsLocalUtils
{
public:
	static TSharedPtr<FJsonObject> MakeComponentTaskPlanStep(const FString& OpName)
	{
		TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
		Step->SetStringField(TEXT("step_id"), TEXT("step_component"));
		Step->SetStringField(TEXT("capability"), FBlueprintHelperComponentTaskPlanAdapter::CapabilityBlueprintComponent);

		TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), TEXT("/Game/Blueprints/BP_Door"));
		Step->SetObjectField(TEXT("target"), Target);

		TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), OpName);

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

		TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), FBlueprintHelperComponentTaskPlanAdapter::StrategyComponentTree);
		Write->SetArrayField(TEXT("ops"), Ops);
		Step->SetObjectField(TEXT("write"), Write);

		return Step;
	}

	static TSharedPtr<FJsonObject> GetFirstComponentOp(const TSharedPtr<FJsonObject>& Step)
	{
		const TSharedPtr<FJsonObject>* Write = nullptr;
		if (!Step->TryGetObjectField(TEXT("write"), Write) || !Write || !Write->IsValid())
		{
			return nullptr;
		}

		const TArray<TSharedPtr<FJsonValue>>* Ops = nullptr;
		if (!(*Write)->TryGetArrayField(TEXT("ops"), Ops) || !Ops || Ops->Num() == 0)
		{
			return nullptr;
		}

		return (*Ops)[0].IsValid() ? (*Ops)[0]->AsObject() : nullptr;
	}

};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanComponentAdapterAddComponentTest,
	"BlueprintHelper.TaskPlan.ComponentAdapter.AddComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanComponentAdapterAddComponentTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanComponentAdapterTestsLocalUtils::MakeComponentTaskPlanStep(FBlueprintHelperComponentTaskPlanAdapter::OpAddComponent);
	TSharedPtr<FJsonObject> Op = FBlueprintHelperTaskPlanComponentAdapterTestsLocalUtils::GetFirstComponentOp(Step);
	TestNotNull(TEXT("component op exists"), Op.Get());

	Op->SetStringField(TEXT("component_name"), TEXT("DoorMesh"));
	Op->SetStringField(TEXT("component_class"), TEXT("StaticMeshComponent"));
	Op->SetStringField(TEXT("parent_component"), TEXT("DefaultSceneRoot"));
	Op->SetStringField(TEXT("socket_name"), TEXT("DoorSocket"));
	Op->SetStringField(TEXT("attach_rule"), TEXT("keep_relative"));
	Op->SetStringField(TEXT("name_collision_policy"), TEXT("fail_if_exists"));

	FBlueprintHelperComponentTaskPlanPayload BuiltPayload;
	FBlueprintHelperToolError Error;
	const bool bBuilt = FBlueprintHelperComponentTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
		Step,
		true,
		BuiltPayload,
		Error);

	TestTrue(TEXT("add_component lowers successfully"), bBuilt);
	TestEqual(TEXT("capability preserved"), BuiltPayload.Capability, FString(FBlueprintHelperComponentTaskPlanAdapter::CapabilityBlueprintComponent));
	TestEqual(TEXT("runtime operation reports component capability"), BuiltPayload.RuntimeOperation, FString(FBlueprintHelperComponentTaskPlanAdapter::RuntimeOperationBlueprintComponent));
	TestEqual(TEXT("adapter operation is add_component"), BuiltPayload.AdapterOperation, FString(FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationAddComponent));
	TestTrue(TEXT("component adapter supports true service dry-run"), BuiltPayload.bAdapterDryRunSupported);
	TestNotNull(TEXT("payload exists"), BuiltPayload.Payload.Get());

	FString AssetPath;
	FString ComponentName;
	FString ComponentClass;
	FString ParentComponent;
	FString SocketName;
	FString AttachRule;
	FString NameCollisionPolicy;
	bool bDryRun = false;

	TestTrue(TEXT("payload carries asset_path"), BuiltPayload.Payload->TryGetStringField(TEXT("asset_path"), AssetPath));
	TestTrue(TEXT("payload carries component_name"), BuiltPayload.Payload->TryGetStringField(TEXT("component_name"), ComponentName));
	TestTrue(TEXT("payload carries component_class"), BuiltPayload.Payload->TryGetStringField(TEXT("component_class"), ComponentClass));
	TestTrue(TEXT("payload carries parent_component"), BuiltPayload.Payload->TryGetStringField(TEXT("parent_component"), ParentComponent));
	TestTrue(TEXT("payload carries socket_name"), BuiltPayload.Payload->TryGetStringField(TEXT("socket_name"), SocketName));
	TestTrue(TEXT("payload carries attach_rule"), BuiltPayload.Payload->TryGetStringField(TEXT("attach_rule"), AttachRule));
	TestTrue(TEXT("payload carries name_collision_policy"), BuiltPayload.Payload->TryGetStringField(TEXT("name_collision_policy"), NameCollisionPolicy));
	TestTrue(TEXT("payload carries requested dry_run flag"), BuiltPayload.Payload->TryGetBoolField(TEXT("dry_run"), bDryRun));

	TestEqual(TEXT("asset_path matches target"), AssetPath, FString(TEXT("/Game/Blueprints/BP_Door")));
	TestEqual(TEXT("component_name preserved"), ComponentName, FString(TEXT("DoorMesh")));
	TestEqual(TEXT("component_class preserved"), ComponentClass, FString(TEXT("StaticMeshComponent")));
	TestEqual(TEXT("parent_component preserved"), ParentComponent, FString(TEXT("DefaultSceneRoot")));
	TestEqual(TEXT("socket_name preserved"), SocketName, FString(TEXT("DoorSocket")));
	TestEqual(TEXT("attach_rule preserved"), AttachRule, FString(TEXT("keep_relative")));
	TestEqual(TEXT("name_collision_policy preserved"), NameCollisionPolicy, FString(TEXT("fail_if_exists")));
	TestTrue(TEXT("dry_run request is recorded in payload"), bDryRun);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanComponentAdapterSetPropertiesTest,
	"BlueprintHelper.TaskPlan.ComponentAdapter.SetComponentProperties",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanComponentAdapterSetPropertiesTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanComponentAdapterTestsLocalUtils::MakeComponentTaskPlanStep(FBlueprintHelperComponentTaskPlanAdapter::OpSetComponentProperties);
	TSharedPtr<FJsonObject> Op = FBlueprintHelperTaskPlanComponentAdapterTestsLocalUtils::GetFirstComponentOp(Step);
	TestNotNull(TEXT("component op exists"), Op.Get());

	Op->SetStringField(TEXT("component_name"), TEXT("DoorMesh"));

	TSharedPtr<FJsonObject> Setting = MakeShared<FJsonObject>();
	Setting->SetStringField(TEXT("property_path"), TEXT("Mobility"));
	Setting->SetStringField(TEXT("value"), TEXT("Movable"));

	TArray<TSharedPtr<FJsonValue>> Settings;
	Settings.Add(MakeShared<FJsonValueObject>(Setting.ToSharedRef()));
	Op->SetArrayField(TEXT("settings"), Settings);

	FBlueprintHelperComponentTaskPlanPayload BuiltPayload;
	FBlueprintHelperToolError Error;
	const bool bBuilt = FBlueprintHelperComponentTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
		Step,
		false,
		BuiltPayload,
		Error);

	TestTrue(TEXT("set_component_properties lowers successfully"), bBuilt);
	TestEqual(TEXT("adapter operation is set_component_properties"), BuiltPayload.AdapterOperation, FString(FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationSetComponentProperties));
	TestTrue(TEXT("set_component_properties adapter supports true service dry-run"), BuiltPayload.bAdapterDryRunSupported);
	TestNotNull(TEXT("payload exists"), BuiltPayload.Payload.Get());

	FString AssetPath;
	FString ComponentName;
	bool bDryRun = true;
	const TArray<TSharedPtr<FJsonValue>>* PayloadSettings = nullptr;
	TestTrue(TEXT("payload carries asset_path"), BuiltPayload.Payload->TryGetStringField(TEXT("asset_path"), AssetPath));
	TestTrue(TEXT("payload carries component_name"), BuiltPayload.Payload->TryGetStringField(TEXT("component_name"), ComponentName));
	TestTrue(TEXT("payload carries settings"), BuiltPayload.Payload->TryGetArrayField(TEXT("settings"), PayloadSettings));
	TestTrue(TEXT("payload carries dry_run flag"), BuiltPayload.Payload->TryGetBoolField(TEXT("dry_run"), bDryRun));

	TestEqual(TEXT("asset_path matches target"), AssetPath, FString(TEXT("/Game/Blueprints/BP_Door")));
	TestEqual(TEXT("component_name preserved"), ComponentName, FString(TEXT("DoorMesh")));
	TestEqual(TEXT("one setting preserved"), PayloadSettings ? PayloadSettings->Num() : 0, 1);
	TestFalse(TEXT("execute request records dry_run=false"), bDryRun);

	const TSharedPtr<FJsonObject> PayloadSetting = PayloadSettings && PayloadSettings->Num() > 0
		? (*PayloadSettings)[0]->AsObject()
		: nullptr;
	TestNotNull(TEXT("setting object exists"), PayloadSetting.Get());
	FString PropertyPath;
	FString Value;
	TestTrue(TEXT("setting carries property_path"), PayloadSetting->TryGetStringField(TEXT("property_path"), PropertyPath));
	TestTrue(TEXT("setting carries value"), PayloadSetting->TryGetStringField(TEXT("value"), Value));
	TestEqual(TEXT("property_path preserved"), PropertyPath, FString(TEXT("Mobility")));
	TestEqual(TEXT("value preserved"), Value, FString(TEXT("Movable")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanComponentAdapterRemoveComponentTest,
	"BlueprintHelper.TaskPlan.ComponentAdapter.RemoveComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanComponentAdapterRemoveComponentTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanComponentAdapterTestsLocalUtils::MakeComponentTaskPlanStep(FBlueprintHelperComponentTaskPlanAdapter::OpRemoveComponent);
	TSharedPtr<FJsonObject> Op = FBlueprintHelperTaskPlanComponentAdapterTestsLocalUtils::GetFirstComponentOp(Step);
	TestNotNull(TEXT("component op exists"), Op.Get());
	Op->SetStringField(TEXT("component_name"), TEXT("DoorMesh"));

	FBlueprintHelperComponentTaskPlanPayload BuiltPayload;
	FBlueprintHelperToolError Error;
	const bool bBuilt = FBlueprintHelperComponentTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
		Step,
		true,
		BuiltPayload,
		Error);

	TestTrue(TEXT("remove_component lowers successfully"), bBuilt);
	TestEqual(TEXT("adapter operation is remove_component"), BuiltPayload.AdapterOperation, FString(FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationRemoveComponent));
	TestTrue(TEXT("remove_component adapter supports true service dry-run"), BuiltPayload.bAdapterDryRunSupported);

	FString AssetPath;
	FString ComponentName;
	bool bDryRun = false;
	TestTrue(TEXT("payload carries asset_path"), BuiltPayload.Payload->TryGetStringField(TEXT("asset_path"), AssetPath));
	TestTrue(TEXT("payload carries component_name"), BuiltPayload.Payload->TryGetStringField(TEXT("component_name"), ComponentName));
	TestTrue(TEXT("payload carries dry_run flag"), BuiltPayload.Payload->TryGetBoolField(TEXT("dry_run"), bDryRun));
	TestEqual(TEXT("asset_path matches target"), AssetPath, FString(TEXT("/Game/Blueprints/BP_Door")));
	TestEqual(TEXT("component_name preserved"), ComponentName, FString(TEXT("DoorMesh")));
	TestTrue(TEXT("dry_run request is recorded in payload"), bDryRun);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanComponentAdapterRejectsOperationFieldTest,
	"BlueprintHelper.TaskPlan.ComponentAdapter.RejectsOperationField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanComponentAdapterRejectsOperationFieldTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanComponentAdapterTestsLocalUtils::MakeComponentTaskPlanStep(FBlueprintHelperComponentTaskPlanAdapter::OpAddComponent);
	Step->SetStringField(TEXT("operation"), TEXT("add_component"));

	FBlueprintHelperComponentTaskPlanPayload BuiltPayload;
	FBlueprintHelperToolError Error;
	const bool bBuilt = FBlueprintHelperComponentTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
		Step,
		true,
		BuiltPayload,
		Error);

	TestFalse(TEXT("blueprint_component IR rejects adapter operation field"), bBuilt);
	TestEqual(TEXT("operation field error code"), Error.Code, FString(TEXT("unsupported_blueprint_component_operation_field")));
	TestEqual(TEXT("operation field points at TaskPlan step operation"), Error.Field, FString(TEXT("task_plan.steps[0].operation")));

	return true;
}

#endif
