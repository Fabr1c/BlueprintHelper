#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/Pipeline/BlueprintHelperTaskRuntimePipeline.h"

class FJsonObject;

class BLUEPRINTHELPER_API FBlueprintHelperTaskRuntimeResultProjection
{
public:
	static void AttachPipelineTrace(
		TSharedPtr<FJsonObject>& Data,
		const FBlueprintHelperTaskRuntimePipelineContext& PipelineContext);
};
