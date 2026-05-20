#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Runtime/TaskRuntime/PostOperations/BlueprintHelperTaskRuntimeAssetStateService.h"
#include "Runtime/TaskRuntime/PostOperations/BlueprintHelperTaskRuntimePostOperationExecutor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeAssetStateService_NormalizesPackageNames,
	"BlueprintHelper.TaskRuntime.PostOperation.AssetStateNormalizesPackageNames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskRuntimeAssetStateService_NormalizesPackageNames::RunTest(const FString&)
{
	TestEqual(
		TEXT("package from object path"),
		FBlueprintHelperTaskRuntimeAssetStateService::NormalizePackageName(TEXT("/Game/Test/BP_A.BP_A")),
		FString(TEXT("/Game/Test/BP_A")));
	TestEqual(
		TEXT("package from package path"),
		FBlueprintHelperTaskRuntimeAssetStateService::NormalizePackageName(TEXT("/Game/Test/BP_A")),
		FString(TEXT("/Game/Test/BP_A")));
	TestEqual(
		TEXT("object path from package path"),
		FBlueprintHelperTaskRuntimeAssetStateService::BuildObjectPath(TEXT("/Game/Test/BP_A")),
		FString(TEXT("/Game/Test/BP_A.BP_A")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimePostOperationExecutor_SkipsSaveWhenPackageClean,
	"BlueprintHelper.TaskRuntime.PostOperation.ExecutorSkipsCleanSave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskRuntimePostOperationExecutor_SkipsSaveWhenPackageClean::RunTest(const FString&)
{
	FBlueprintHelperTaskRuntimePostOperationPlan Plan;
	FBlueprintHelperTaskRuntimePostOperationPlanItem Item;
	Item.Kind = EBlueprintHelperTaskRuntimePostOperationKind::Save;
	Item.Operation = TEXT("save_asset");
	Item.AssetPath = TEXT("/Game/BlueprintHelperMissing/P6_CleanPackage");
	Plan.Items.Add(Item);

	FBlueprintHelperTaskRuntimePostOperationExecutor Executor;
	const FBlueprintHelperTaskRuntimePostOperationExecutionResult Result =
		Executor.Execute(Plan, nullptr);

	TestTrue(TEXT("executor result ok"), Result.bOk);
	TestEqual(TEXT("one record"), Result.Records.Num(), 1);
	TestEqual(TEXT("save skipped"), Result.Records[0].Status, EBlueprintHelperTaskRuntimePostOperationStatus::Skipped);
	TestEqual(TEXT("skip reason"), Result.Records[0].Reason, FString(TEXT("package_not_loaded_or_clean")));
	TestEqual(TEXT("tool status"), Result.Records[0].Result.Status, EBlueprintHelperToolStatus::Skipped);
	return true;
}

#endif
