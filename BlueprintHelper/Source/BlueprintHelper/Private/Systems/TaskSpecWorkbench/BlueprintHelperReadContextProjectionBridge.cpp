#include "Systems/TaskSpecWorkbench/BlueprintHelperReadContextProjectionBridge.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

void FBlueprintHelperReadContextProjectionBridge::AttachTaskCoreProjectionMetadata(
	TSharedRef<FJsonObject> Payload,
	const FString& RequestedFormat,
	bool bProjectionUnavailable)
{
	Payload->SetStringField(TEXT("projection_owner"), TEXT("task-core"));
	Payload->SetStringField(TEXT("ue_callback_schema"), TEXT("LogicSnapshot.v1"));
	if (!RequestedFormat.IsEmpty())
	{
		Payload->SetStringField(TEXT("requested_format"), RequestedFormat);
	}

	if (!bProjectionUnavailable)
	{
		return;
	}

	TSharedRef<FJsonObject> Diagnostic = MakeShared<FJsonObject>();
	Diagnostic->SetStringField(TEXT("code"), TEXT("projection_unavailable"));
	Diagnostic->SetStringField(TEXT("source"), TEXT("task_spec_workbench"));
	Diagnostic->SetStringField(
		TEXT("message"),
		TEXT("Task-core logic projector is unavailable in editor-only Workbench export."));
	Diagnostic->SetStringField(TEXT("requested_format"), RequestedFormat);

	TArray<TSharedPtr<FJsonValue>> Diagnostics;
	Diagnostics.Add(MakeShared<FJsonValueObject>(Diagnostic));
	Payload->SetArrayField(TEXT("diagnostics"), Diagnostics);
}
