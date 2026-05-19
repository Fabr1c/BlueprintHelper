#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimePrepareService.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

class FBlueprintHelperTaskRuntimePrepareServiceTestsLocalUtils
{
public:
	static TSharedRef<FJsonObject> MakeAssetFactoryStep(
		const FString& StepId,
		const FString& AssetPath)
	{
		TSharedRef<FJsonObject> Step = MakeShared<FJsonObject>();
		Step->SetStringField(TEXT("step_id"), StepId);
		Step->SetStringField(TEXT("capability"), TEXT("asset_factory"));

		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);
		Step->SetObjectField(TEXT("target"), Target);

		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("create_asset"));
		Op->SetStringField(TEXT("asset_type"), TEXT("Blueprint"));
		Op->SetStringField(TEXT("parent_class"), TEXT("/Script/Engine.Actor"));

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op));

		TSharedRef<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), TEXT("asset_create"));
		Write->SetArrayField(TEXT("ops"), Ops);
		Step->SetObjectField(TEXT("write"), Write);
		return Step;
	}

	static TSharedRef<FJsonObject> MakePayloadWithSteps(
		const TArray<TSharedRef<FJsonObject>>& Steps)
	{
		TArray<TSharedPtr<FJsonValue>> StepValues;
		for (const TSharedRef<FJsonObject>& Step : Steps)
		{
			StepValues.Add(MakeShared<FJsonValueObject>(Step));
		}

		TArray<TSharedPtr<FJsonValue>> TargetAssets;
		TargetAssets.Add(MakeShared<FJsonValueString>(TEXT("/Game/BlueprintHelperPrepare/BP_A")));
		TargetAssets.Add(MakeShared<FJsonValueString>(TEXT("/Game/BlueprintHelperPrepare/BP_A")));

		TSharedRef<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
		TaskPlan->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.TaskPlan.v1"));
		TaskPlan->SetStringField(TEXT("task_type"), TEXT("create_blueprint_feature"));
		TaskPlan->SetArrayField(TEXT("target_assets"), TargetAssets);
		TaskPlan->SetArrayField(TEXT("steps"), StepValues);

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetObjectField(TEXT("task_plan"), TaskPlan);
		return Payload;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimePrepareService_PreparesPureDataTaskRun,
	"BlueprintHelper.TaskRuntime.Prepare.PreparesPureDataTaskRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimePrepareService_PreparesPureDataTaskRun::RunTest(const FString& Parameters)
{
	TSharedRef<FJsonObject> FirstStep =
		FBlueprintHelperTaskRuntimePrepareServiceTestsLocalUtils::MakeAssetFactoryStep(
			TEXT("create_asset"),
			TEXT("/Game/BlueprintHelperPrepare/BP_A"));
	TSharedRef<FJsonObject> SecondStep =
		FBlueprintHelperTaskRuntimePrepareServiceTestsLocalUtils::MakeAssetFactoryStep(
			TEXT("create_asset_again"),
			TEXT("/Game/BlueprintHelperPrepare/BP_A"));
	TArray<TSharedPtr<FJsonValue>> DependsOn;
	DependsOn.Add(MakeShared<FJsonValueString>(TEXT("create_asset")));
	SecondStep->SetArrayField(TEXT("depends_on"), DependsOn);

	const TSharedRef<FJsonObject> Payload =
		FBlueprintHelperTaskRuntimePrepareServiceTestsLocalUtils::MakePayloadWithSteps({FirstStep, SecondStep});

	FBlueprintHelperTaskRuntimePrepareService PrepareService;
	FBlueprintHelperTaskRuntimePreparedTaskRun PreparedRun;
	FBlueprintHelperToolError Error;
	TestTrue(TEXT("pure prepare succeeds without UObject fixtures"),
		PrepareService.Prepare(Payload, true, PreparedRun, Error));
	TestEqual(TEXT("prepare keeps two steps"), PreparedRun.Steps.Num(), 2);
	TestEqual(TEXT("target assets are unique"), PreparedRun.TargetAssets.Num(), 1);
	TestTrue(TEXT("preview prepare does not allocate task run id"), PreparedRun.TaskRunId.IsEmpty());
	TestEqual(TEXT("dependency is preserved"),
		PreparedRun.Steps[1].DependsOn.Num() == 1 ? PreparedRun.Steps[1].DependsOn[0] : FString(),
		FString(TEXT("create_asset")));
	TestEqual(TEXT("lowered adapter operation is produced"),
		PreparedRun.Steps[0].LoweredStep.AdapterOperation,
		FString(TEXT("create_asset")));
	TestTrue(TEXT("lowered payload is pure JSON"), PreparedRun.Steps[0].LoweredStep.Payload.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimePrepareService_BlocksDryRunNone,
	"BlueprintHelper.TaskRuntime.Prepare.BlocksDryRunNone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimePrepareService_BlocksDryRunNone::RunTest(const FString& Parameters)
{
	const TSharedRef<FJsonObject> Step =
		FBlueprintHelperTaskRuntimePrepareServiceTestsLocalUtils::MakeAssetFactoryStep(
			TEXT("create_asset"),
			TEXT("/Game/BlueprintHelperPrepare/BP_A"));
	const TSharedRef<FJsonObject> Payload =
		FBlueprintHelperTaskRuntimePrepareServiceTestsLocalUtils::MakePayloadWithSteps({Step});

	TSharedPtr<FJsonObject> TaskPlan = Payload->GetObjectField(TEXT("task_plan"));
	TSharedRef<FJsonObject> ExecutionPolicy = MakeShared<FJsonObject>();
	ExecutionPolicy->SetStringField(TEXT("dry_run_mode"), TEXT("none"));
	TaskPlan->SetObjectField(TEXT("execution_policy"), ExecutionPolicy);

	FBlueprintHelperTaskRuntimePrepareService PrepareService;
	FBlueprintHelperTaskRuntimePreparedTaskRun PreparedRun;
	FBlueprintHelperToolError Error;
	TestFalse(TEXT("dry_run none is blocked before UE work"),
		PrepareService.Prepare(Payload, true, PreparedRun, Error));
	TestEqual(TEXT("error code is preserved"),
		Error.Code,
		FString(TEXT("dry_run_mode_none_requires_preview_token")));
	TestEqual(TEXT("blocked policy is parsed"),
		PreparedRun.DryRunPolicy.ToDiagnosticString(),
		FString(TEXT("none")));
	return true;
}

#endif
