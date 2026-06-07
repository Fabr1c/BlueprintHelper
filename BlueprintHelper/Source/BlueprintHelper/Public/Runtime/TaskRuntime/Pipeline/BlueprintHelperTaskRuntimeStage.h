#pragma once

#include "CoreMinimal.h"

enum class EBlueprintHelperTaskRuntimePipelineStage : uint8
{
	ValidateCompiledPlanContract,
	ResolveBridgeRoute,
	ResolveClusterFamilyAdapter,
	ExecuteCluster,
	BuildReviewEvidence,
	ProjectMetricsAndResult,
	RunPostOperations,
	BuildJournal,
	FinalizeBridgeResponse
};

class BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimePipelineStageNames
{
public:
	static const TCHAR* ToString(EBlueprintHelperTaskRuntimePipelineStage Stage);
};
