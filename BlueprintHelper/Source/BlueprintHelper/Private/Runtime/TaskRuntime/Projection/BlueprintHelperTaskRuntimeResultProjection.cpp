#include "Runtime/TaskRuntime/Projection/BlueprintHelperTaskRuntimeResultProjection.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

void FBlueprintHelperTaskRuntimeResultProjection::AttachPipelineTrace(
	TSharedPtr<FJsonObject>& Data,
	const FBlueprintHelperTaskRuntimePipelineContext& PipelineContext)
{
	if (!Data.IsValid())
	{
		Data = MakeShared<FJsonObject>();
	}

	TArray<TSharedPtr<FJsonValue>> StageValues;
	for (const FBlueprintHelperTaskRuntimePipelineStageTrace& Trace : PipelineContext.GetStageTrace())
	{
		TSharedRef<FJsonObject> StageObject = MakeShared<FJsonObject>();
		StageObject->SetStringField(TEXT("stage"), Trace.Name);
		StageValues.Add(MakeShared<FJsonValueObject>(StageObject));
	}
	Data->SetArrayField(TEXT("task_runtime_pipeline_stage_trace"), MoveTemp(StageValues));
	Data->SetStringField(TEXT("task_runtime_pipeline_owner"), TEXT("TaskRuntimePipeline"));
}
