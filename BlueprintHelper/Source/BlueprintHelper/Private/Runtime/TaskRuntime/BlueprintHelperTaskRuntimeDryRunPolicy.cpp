// BlueprintHelper TaskRuntime dry-run policy implementation.

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeDryRunPolicy.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeSettingsResolver.h"

#include "Dom/JsonObject.h"

FBlueprintHelperTaskRuntimeDryRunPolicy FBlueprintHelperTaskRuntimeDryRunPolicy::FromTaskPlan(
	const TSharedRef<FJsonObject>& TaskPlanObject)
{
	FBlueprintHelperTaskRuntimeDryRunPolicy Policy;

	const TSharedPtr<FJsonObject>* ExecutionPolicyPtr = nullptr;
	FString RawMode = FBlueprintHelperTaskRuntimeSettingsResolver::LoadExecutionPolicy().DryRunMode;
	if (TaskPlanObject->TryGetObjectField(TEXT("execution_policy"), ExecutionPolicyPtr) &&
		ExecutionPolicyPtr && ExecutionPolicyPtr->IsValid())
	{
		(*ExecutionPolicyPtr)->TryGetStringField(TEXT("dry_run_mode"), RawMode);
	}

	RawMode.TrimStartAndEndInline();
	const FString NormalizedMode = RawMode.ToLower();
	if (NormalizedMode == TEXT("quick"))
	{
		Policy.Mode = EBlueprintHelperTaskRuntimeDryRunMode::Quick;
		Policy.DiagnosticString = TEXT("quick");
	}
	else if (NormalizedMode == TEXT("none"))
	{
		Policy.Mode = EBlueprintHelperTaskRuntimeDryRunMode::None;
		Policy.DiagnosticString = TEXT("none");
	}
	else
	{
		Policy.Mode = EBlueprintHelperTaskRuntimeDryRunMode::Full;
		Policy.DiagnosticString = TEXT("full");
	}

	return Policy;
}

EBlueprintHelperTaskRuntimeDryRunMode FBlueprintHelperTaskRuntimeDryRunPolicy::GetMode() const
{
	return Mode;
}

bool FBlueprintHelperTaskRuntimeDryRunPolicy::ShouldRunFullPreview() const
{
	return Mode == EBlueprintHelperTaskRuntimeDryRunMode::Full;
}

bool FBlueprintHelperTaskRuntimeDryRunPolicy::ShouldRunQuickPreview() const
{
	return Mode == EBlueprintHelperTaskRuntimeDryRunMode::Quick;
}

bool FBlueprintHelperTaskRuntimeDryRunPolicy::ShouldAllowNoPreview(
	bool bHasValidatedPreviewToken) const
{
	return Mode == EBlueprintHelperTaskRuntimeDryRunMode::None && bHasValidatedPreviewToken;
}

FString FBlueprintHelperTaskRuntimeDryRunPolicy::ToDiagnosticString() const
{
	return DiagnosticString;
}
