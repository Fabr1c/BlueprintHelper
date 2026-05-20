#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Runtime/TaskRuntime/PostOperations/BlueprintHelperTaskRuntimePostOperationPlanner.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimePostOperationPlanner_DedupesTargetAssets,
	"BlueprintHelper.TaskRuntime.PostOperation.PlannerDedupesTargetAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskRuntimePostOperationPlanner_DedupesTargetAssets::RunTest(const FString&)
{
	TSharedRef<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> ExecutionPolicy = MakeShared<FJsonObject>();
	ExecutionPolicy->SetBoolField(TEXT("should_compile"), true);
	ExecutionPolicy->SetBoolField(TEXT("should_save"), true);
	TaskPlan->SetObjectField(TEXT("execution_policy"), ExecutionPolicy);

	TArray<TSharedPtr<FJsonValue>> TargetAssets;
	TargetAssets.Add(MakeShared<FJsonValueString>(TEXT("/Game/Test/BP_A")));
	TargetAssets.Add(MakeShared<FJsonValueString>(TEXT("/Game/Test/BP_A.BP_A")));
	TargetAssets.Add(MakeShared<FJsonValueString>(TEXT("/Game/Test/BP_B")));
	TaskPlan->SetArrayField(TEXT("target_assets"), TargetAssets);

	const FBlueprintHelperTaskRuntimePostOperationPlan Plan =
		FBlueprintHelperTaskRuntimePostOperationPlanner::BuildPlan(TaskPlan, false);

	TestEqual(TEXT("deduped compile/save count"), Plan.Items.Num(), 4);
	TestEqual(TEXT("first compile op"), Plan.Items[0].Operation, FString(TEXT("compile_blueprint_asset")));
	TestEqual(TEXT("first normalized asset"), Plan.Items[0].AssetPath, FString(TEXT("/Game/Test/BP_A")));
	TestEqual(TEXT("second compile asset"), Plan.Items[1].AssetPath, FString(TEXT("/Game/Test/BP_B")));
	TestEqual(TEXT("first save op"), Plan.Items[2].Operation, FString(TEXT("save_asset")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimePostOperationPlanner_DryRunReturnsNoItems,
	"BlueprintHelper.TaskRuntime.PostOperation.PlannerDryRunReturnsNoItems",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskRuntimePostOperationPlanner_DryRunReturnsNoItems::RunTest(const FString&)
{
	TSharedRef<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> ExecutionPolicy = MakeShared<FJsonObject>();
	ExecutionPolicy->SetBoolField(TEXT("should_compile"), true);
	ExecutionPolicy->SetBoolField(TEXT("should_save"), true);
	TaskPlan->SetObjectField(TEXT("execution_policy"), ExecutionPolicy);

	const FBlueprintHelperTaskRuntimePostOperationPlan Plan =
		FBlueprintHelperTaskRuntimePostOperationPlanner::BuildPlan(TaskPlan, true);

	TestEqual(TEXT("dry run has no post ops"), Plan.Items.Num(), 0);
	return true;
}

#endif
