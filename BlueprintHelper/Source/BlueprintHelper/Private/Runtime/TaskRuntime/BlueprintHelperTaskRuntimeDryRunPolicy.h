// BlueprintHelper TaskRuntime dry-run policy.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;

enum class EBlueprintHelperTaskRuntimeDryRunMode : uint8
{
	Full,
	Quick,
	None
};

class FBlueprintHelperTaskRuntimeDryRunPolicy
{
public:
	static FBlueprintHelperTaskRuntimeDryRunPolicy FromTaskPlan(const TSharedRef<FJsonObject>& TaskPlanObject);

	EBlueprintHelperTaskRuntimeDryRunMode GetMode() const;
	bool ShouldRunFullPreview() const;
	bool ShouldRunQuickPreview() const;
	bool ShouldAllowNoPreview(bool bHasValidatedPreviewToken) const;
	FString ToDiagnosticString() const;

private:
	EBlueprintHelperTaskRuntimeDryRunMode Mode = EBlueprintHelperTaskRuntimeDryRunMode::Full;
	FString DiagnosticString = TEXT("full");
};

