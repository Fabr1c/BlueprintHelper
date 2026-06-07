#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeTypes.h"

enum class EBlueprintHelperTaskRuntimeCluster : uint8;
struct FBlueprintHelperWriteReviewEvidence;

class BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimeReviewEvidenceBuilderRegistry
{
public:
	static bool TryBuild(
		EBlueprintHelperTaskRuntimeCluster Cluster,
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		const FBlueprintHelperToolResultBase& StepResult,
		const FString& ArchiveSessionId,
		const FString& TaskRunId,
		int32 StepIndex,
		FBlueprintHelperWriteReviewEvidence& OutEvidence);
};
