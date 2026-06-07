#include "Runtime/TaskRuntime/WriteContracts/BlueprintHelperAcceptedPayloadModel.h"

FString FBlueprintHelperAcceptedPayloadModelUtils::MakeReviewScopeIdentity(
	const FBlueprintHelperAcceptedPayloadModel& Model)
{
	if (!Model.ReviewScopeIdentity.IsEmpty())
	{
		return Model.ReviewScopeIdentity;
	}

	TArray<FString> Parts;
	if (!Model.TargetAssetPath.IsEmpty())
	{
		Parts.Add(Model.TargetAssetPath);
	}
	if (!Model.GraphName.IsEmpty())
	{
		Parts.Add(Model.GraphName);
	}
	if (!Model.WriteFamily.IsEmpty())
	{
		Parts.Add(Model.WriteFamily);
	}
	if (!Model.OperationId.IsEmpty())
	{
		Parts.Add(Model.OperationId);
	}
	return FString::Join(Parts, TEXT("|"));
}

FString FBlueprintHelperAcceptedPayloadModelUtils::MakeDebugTraceId(
	const FBlueprintHelperAcceptedPayloadModel& Model)
{
	if (!Model.DebugTraceId.IsEmpty())
	{
		return Model.DebugTraceId;
	}

	const FString ScopeIdentity = MakeReviewScopeIdentity(Model);
	const FString HashSource = FString::Printf(
		TEXT("%s|%s|%s|%s"),
		*Model.TaskId,
		*Model.OperationId,
		*Model.RuntimeAdapterId,
		*ScopeIdentity);
	return FString::Printf(TEXT("accepted_payload_%08x"), GetTypeHash(HashSource));
}

TSharedRef<FJsonObject> FBlueprintHelperAcceptedPayloadModelUtils::ToJson(
	const FBlueprintHelperAcceptedPayloadModel& Model)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("task_id"), Model.TaskId);
	Json->SetStringField(TEXT("operation_id"), Model.OperationId);
	Json->SetStringField(TEXT("write_family"), Model.WriteFamily);
	Json->SetStringField(TEXT("runtime_adapter_id"), Model.RuntimeAdapterId);
	Json->SetStringField(TEXT("task_spec_strategy"), Model.TaskSpecStrategy);
	Json->SetStringField(TEXT("bridge_command"), Model.BridgeCommand);
	Json->SetStringField(TEXT("target_asset_path"), Model.TargetAssetPath);
	Json->SetStringField(TEXT("graph_name"), Model.GraphName);
	Json->SetStringField(TEXT("mode"), Model.Mode);
	Json->SetStringField(TEXT("source_payload_schema"), Model.SourcePayloadSchema);
	Json->SetStringField(TEXT("review_scope_identity"), MakeReviewScopeIdentity(Model));
	Json->SetStringField(TEXT("debug_trace_id"), MakeDebugTraceId(Model));
	if (Model.RawPayload.IsValid())
	{
		Json->SetObjectField(TEXT("raw_payload"), Model.RawPayload.ToSharedRef());
	}
	return Json;
}
