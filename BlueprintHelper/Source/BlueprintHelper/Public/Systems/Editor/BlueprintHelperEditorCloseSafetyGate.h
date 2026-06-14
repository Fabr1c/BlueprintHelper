#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

struct FBlueprintHelperSourceControlResult;

struct BLUEPRINTHELPER_API FBlueprintHelperEditorCloseSafetyGateResult
{
	bool bCanProceed = true;
	FString Code;
	FString Message;
	TSharedPtr<FJsonObject> SourceControlJson;

	TSharedRef<FJsonObject> ToJson() const;
};

class BLUEPRINTHELPER_API FBlueprintHelperEditorCloseSafetyGate
{
public:
	bool ShouldAttemptAutoCheckout(const FBlueprintHelperSourceControlResult& SourceControlResult) const;

	FBlueprintHelperEditorCloseSafetyGateResult EvaluateDirtyPackageStatus(
		const FBlueprintHelperSourceControlResult& SourceControlResult) const;
};
