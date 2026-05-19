#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeDryRunPolicy.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

class FBlueprintHelperTaskRuntimeDryRunPolicyTestsLocalUtils
{
public:
	static TSharedRef<FJsonObject> MakeTaskPlanWithMode(const FString& Mode)
	{
		TSharedRef<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> ExecutionPolicy = MakeShared<FJsonObject>();
		if (!Mode.IsEmpty())
		{
			ExecutionPolicy->SetStringField(TEXT("dry_run_mode"), Mode);
		}
		TaskPlan->SetObjectField(TEXT("execution_policy"), ExecutionPolicy);
		return TaskPlan;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeDryRunPolicy_ParsesSupportedModes,
	"BlueprintHelper.TaskRuntime.DryRunPolicy.ParsesSupportedModes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimeDryRunPolicy_ParsesSupportedModes::RunTest(const FString& Parameters)
{
	const FBlueprintHelperTaskRuntimeDryRunPolicy FullPolicy =
		FBlueprintHelperTaskRuntimeDryRunPolicy::FromTaskPlan(
			FBlueprintHelperTaskRuntimeDryRunPolicyTestsLocalUtils::MakeTaskPlanWithMode(TEXT("full")));
	TestTrue(TEXT("full runs full preview"), FullPolicy.ShouldRunFullPreview());
	TestEqual(TEXT("full diagnostic"), FullPolicy.ToDiagnosticString(), FString(TEXT("full")));

	const FBlueprintHelperTaskRuntimeDryRunPolicy QuickPolicy =
		FBlueprintHelperTaskRuntimeDryRunPolicy::FromTaskPlan(
			FBlueprintHelperTaskRuntimeDryRunPolicyTestsLocalUtils::MakeTaskPlanWithMode(TEXT("quick")));
	TestTrue(TEXT("quick runs quick preview"), QuickPolicy.ShouldRunQuickPreview());
	TestEqual(TEXT("quick diagnostic"), QuickPolicy.ToDiagnosticString(), FString(TEXT("quick")));

	const FBlueprintHelperTaskRuntimeDryRunPolicy NonePolicy =
		FBlueprintHelperTaskRuntimeDryRunPolicy::FromTaskPlan(
			FBlueprintHelperTaskRuntimeDryRunPolicyTestsLocalUtils::MakeTaskPlanWithMode(TEXT("none")));
	TestTrue(TEXT("none allows no preview only with validated token"), NonePolicy.ShouldAllowNoPreview(true));
	TestFalse(TEXT("none without token is not allowed"), NonePolicy.ShouldAllowNoPreview(false));
	TestEqual(TEXT("none diagnostic"), NonePolicy.ToDiagnosticString(), FString(TEXT("none")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeDryRunPolicy_DefaultsToFull,
	"BlueprintHelper.TaskRuntime.DryRunPolicy.DefaultsToFull",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperTaskRuntimeDryRunPolicy_DefaultsToFull::RunTest(const FString& Parameters)
{
	const FBlueprintHelperTaskRuntimeDryRunPolicy Policy =
		FBlueprintHelperTaskRuntimeDryRunPolicy::FromTaskPlan(
			FBlueprintHelperTaskRuntimeDryRunPolicyTestsLocalUtils::MakeTaskPlanWithMode(TEXT("")));
	TestTrue(TEXT("missing dry_run_mode defaults to full"), Policy.ShouldRunFullPreview());
	TestEqual(TEXT("default diagnostic"), Policy.ToDiagnosticString(), FString(TEXT("full")));
	return true;
}

#endif

