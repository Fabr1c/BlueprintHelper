#pragma once

#include "CoreMinimal.h"

enum class EBlueprintHelperTaskRuntimePipelineStage : uint8
{
	Prepare,
	ResolvePreviewToken,
	CaptureReviewBaseline,
	ExecuteSteps,
	BuildReviewEvidence,
	RunPostOperations,
	BuildJournal,
	AttachRuntimeFacts,
	FinalizeResult
};

class BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimePipelineStageNames
{
public:
	static const TCHAR* ToString(EBlueprintHelperTaskRuntimePipelineStage Stage);
};
