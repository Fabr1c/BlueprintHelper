// BlueprintHelper Review generic debug projection adapter implementation.

#include "UI/Review/BlueprintHelperReviewGenericDebugProjectionAdapter.h"

#include "Dom/JsonObject.h"

FBlueprintHelperReviewGenericDebugProjectionAdapter::FBlueprintHelperReviewGenericDebugProjectionAdapter(
	const FString& InEventType)
	: EventType(InEventType)
{
}

FString FBlueprintHelperReviewGenericDebugProjectionAdapter::GetEventType() const
{
	return EventType;
}

FBlueprintHelperReviewDebugProjectionResult FBlueprintHelperReviewGenericDebugProjectionAdapter::Project(
	const TSharedPtr<FJsonObject>& EventJson) const
{
	FBlueprintHelperReviewDebugProjectionResult Result;
	if (!EventJson.IsValid())
	{
		Result.Message = TEXT("invalid_event_json");
		return Result;
	}

	FBlueprintHelperReviewDebugEventModel Model;
	EventJson->TryGetStringField(TEXT("event_id"), Model.EventId);
	EventJson->TryGetStringField(TEXT("created_at"), Model.Timestamp);
	EventJson->TryGetStringField(TEXT("event_type"), Model.EventType);
	EventJson->TryGetStringField(TEXT("review_event_id"), Model.ReviewEventId);
	EventJson->TryGetStringField(TEXT("change_id"), Model.ReviewEventId);
	EventJson->TryGetStringField(TEXT("asset_path"), Model.AssetPath);
	EventJson->TryGetStringField(TEXT("target_key"), Model.TargetKey);
	EventJson->TryGetStringField(TEXT("surface_kind"), Model.SurfaceKind);
	EventJson->TryGetStringField(TEXT("action_kind"), Model.ActionKind);
	EventJson->TryGetStringField(TEXT("result"), Model.Result);
	EventJson->TryGetStringField(TEXT("message"), Model.Message);
	if (Model.EventType.IsEmpty())
	{
		Model.EventType = EventType;
	}
	if (Model.EventId.IsEmpty())
	{
		Model.EventId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	}

	Result.Events.Add(Model);
	Result.bProjected = true;
	Result.Message = TEXT("projected");
	return Result;
}
