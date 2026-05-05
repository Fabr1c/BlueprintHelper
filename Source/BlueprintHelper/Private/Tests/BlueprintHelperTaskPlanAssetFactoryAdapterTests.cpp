#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "TaskRuntime/TaskPlanAdapters/BlueprintHelperAssetFactoryTaskPlanAdapter.h"

namespace
{
	TSharedPtr<FJsonObject> MakeAssetFactoryCreateAssetStep()
	{
		TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
		Step->SetStringField(TEXT("step_id"), TEXT("step_asset_factory"));
		Step->SetStringField(TEXT("capability"), FBlueprintHelperAssetFactoryTaskPlanAdapter::SupportedCapability);

		TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), TEXT("/Game/Input/IA_Interact"));
		Step->SetObjectField(TEXT("target"), Target);

		TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), FBlueprintHelperAssetFactoryTaskPlanAdapter::SupportedOp);
		Op->SetStringField(TEXT("asset_type"), TEXT("input_action"));
		Op->SetStringField(TEXT("value_type"), TEXT("bool"));
		Op->SetStringField(TEXT("collision"), TEXT("reuse_if_exists"));

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

		TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), FBlueprintHelperAssetFactoryTaskPlanAdapter::SupportedStrategy);
		Write->SetArrayField(TEXT("ops"), Ops);
		Step->SetObjectField(TEXT("write"), Write);

		return Step;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanAssetFactoryAdapterBuildsCreateAssetPayloadTest,
	"BlueprintHelper.TaskPlan.AssetFactoryAdapter.BuildsCreateAssetPayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanAssetFactoryAdapterBuildsCreateAssetPayloadTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
	const TSharedPtr<FJsonObject> Step = MakeAssetFactoryCreateAssetStep();

	TSharedPtr<FJsonObject> Payload;
	FBlueprintHelperToolError Error;
	const bool bBuilt = FBlueprintHelperAssetFactoryTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
		TaskPlan,
		Step,
		true,
		Payload,
		Error);

	TestTrue(TEXT("asset_factory create_asset step lowers"), bBuilt);
	TestNotNull(TEXT("lowered payload exists"), Payload.Get());
	if (!Payload.IsValid())
	{
		return false;
	}

	FString AssetPath;
	FString AssetType;
	FString ValueType;
	FString Collision;
	bool bDryRun = false;
	TestTrue(TEXT("payload carries asset_path"), Payload->TryGetStringField(TEXT("asset_path"), AssetPath));
	TestTrue(TEXT("payload carries asset_type"), Payload->TryGetStringField(TEXT("asset_type"), AssetType));
	TestTrue(TEXT("payload carries value_type"), Payload->TryGetStringField(TEXT("value_type"), ValueType));
	TestTrue(TEXT("payload carries collision"), Payload->TryGetStringField(TEXT("collision"), Collision));
	TestTrue(TEXT("payload carries dry_run"), Payload->TryGetBoolField(TEXT("dry_run"), bDryRun));

	TestEqual(TEXT("asset_path comes from step target"), AssetPath, FString(TEXT("/Game/Input/IA_Interact")));
	TestEqual(TEXT("asset_type comes from create op"), AssetType, FString(TEXT("input_action")));
	TestEqual(TEXT("value_type is preserved"), ValueType, FString(TEXT("bool")));
	TestEqual(TEXT("existing collision field name is preserved"), Collision, FString(TEXT("reuse_if_exists")));
	TestTrue(TEXT("preview dry_run is preserved"), bDryRun);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanAssetFactoryAdapterRejectsOperationFieldTest,
	"BlueprintHelper.TaskPlan.AssetFactoryAdapter.RejectsOperationField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanAssetFactoryAdapterRejectsOperationFieldTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
	const TSharedPtr<FJsonObject> Step = MakeAssetFactoryCreateAssetStep();
	Step->SetStringField(TEXT("operation"), TEXT("create_asset"));

	TSharedPtr<FJsonObject> Payload;
	FBlueprintHelperToolError Error;
	const bool bBuilt = FBlueprintHelperAssetFactoryTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
		TaskPlan,
		Step,
		false,
		Payload,
		Error);

	TestFalse(TEXT("asset_factory IR rejects adapter operation compatibility field"), bBuilt);
	TestEqual(TEXT("operation field error code"), Error.Code, FString(TEXT("unsupported_asset_factory_operation_field")));
	TestEqual(TEXT("operation field error stage"), Error.Stage, EBlueprintHelperToolStage::ParseInput);
	TestEqual(TEXT("operation field error path"), Error.Field, FString(TEXT("task_plan.steps[0].operation")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanAssetFactoryAdapterRejectsMissingAssetTypeTest,
	"BlueprintHelper.TaskPlan.AssetFactoryAdapter.RejectsMissingAssetType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanAssetFactoryAdapterRejectsMissingAssetTypeTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
	const TSharedPtr<FJsonObject> Step = MakeAssetFactoryCreateAssetStep();

	const TSharedPtr<FJsonObject>* Write = nullptr;
	TestTrue(TEXT("test step has write object"), Step->TryGetObjectField(TEXT("write"), Write));

	const TArray<TSharedPtr<FJsonValue>>* Ops = nullptr;
	TestTrue(TEXT("test step has ops array"), (*Write)->TryGetArrayField(TEXT("ops"), Ops));

	const TSharedPtr<FJsonObject> Op = (*Ops)[0]->AsObject();
	Op->RemoveField(TEXT("asset_type"));

	TSharedPtr<FJsonObject> Payload;
	FBlueprintHelperToolError Error;
	const bool bBuilt = FBlueprintHelperAssetFactoryTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
		TaskPlan,
		Step,
		false,
		Payload,
		Error);

	TestFalse(TEXT("asset_type is required"), bBuilt);
	TestEqual(TEXT("missing asset_type error code"), Error.Code, FString(TEXT("invalid_asset_factory_create_asset_op")));
	TestEqual(TEXT("missing asset_type error path"), Error.Field, FString(TEXT("task_plan.steps[0].write.ops[0].asset_type")));

	return true;
}

#endif
