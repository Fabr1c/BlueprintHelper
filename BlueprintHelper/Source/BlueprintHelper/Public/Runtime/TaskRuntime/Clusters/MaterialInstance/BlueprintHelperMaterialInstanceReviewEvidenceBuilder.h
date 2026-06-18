// BlueprintHelper TaskRuntime - MaterialInstance review evidence builder.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeTypes.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperMaterialInstanceReviewEvidenceBuilder
{
public:
	static bool BuildEvidence(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		const FBlueprintHelperToolResultBase& StepResult,
		const FString& ArchiveSessionId,
		const FString& TaskRunId,
		int32 StepIndex,
		FBlueprintHelperWriteReviewEvidence& OutEvidence);
};
