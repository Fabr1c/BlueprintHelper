#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/Pipeline/BlueprintHelperTaskRuntimeStage.h"

struct BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimePipelineStageTrace
{
	EBlueprintHelperTaskRuntimePipelineStage Stage = EBlueprintHelperTaskRuntimePipelineStage::Prepare;
	FString Name;
};

class BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimePipelineContext
{
public:
	void RecordStage(EBlueprintHelperTaskRuntimePipelineStage Stage);
	bool HasStage(EBlueprintHelperTaskRuntimePipelineStage Stage) const;
	const TArray<FBlueprintHelperTaskRuntimePipelineStageTrace>& GetStageTrace() const;

private:
	TArray<FBlueprintHelperTaskRuntimePipelineStageTrace> StageTrace;
};

class BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimePipeline
{
public:
	static TArray<EBlueprintHelperTaskRuntimePipelineStage> GetDefaultStageOrder();
};
