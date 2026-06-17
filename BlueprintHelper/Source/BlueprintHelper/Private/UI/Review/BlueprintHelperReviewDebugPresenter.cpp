// BlueprintHelper Review debug presenter implementation.

#include "UI/Review/BlueprintHelperReviewDebugPresenter.h"
#include "UI/Review/BlueprintHelperReviewDebugBundleService.h"
#include "UI/Review/BlueprintHelperReviewDebugProjectionRegistry.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FBlueprintHelperReviewDebugPresenter::FBlueprintHelperReviewDebugPresenter()
	: ProjectionRegistry(FBlueprintHelperReviewDebugProjectionRegistry::CreateDefault())
{
}

FBlueprintHelperReviewDebugPresenter::FBlueprintHelperReviewDebugPresenter(
	TSharedPtr<FBlueprintHelperReviewDebugProjectionRegistry> InProjectionRegistry)
	: ProjectionRegistry(InProjectionRegistry.IsValid()
		? InProjectionRegistry
		: FBlueprintHelperReviewDebugProjectionRegistry::CreateDefault())
{
}

bool FBlueprintHelperReviewDebugPresenter::LoadBundle(
	const FString& BundlePath,
	FBlueprintHelperReviewDebugTimelineModel& OutTimeline,
	FString& OutError) const
{
	FString BundleText;
	if (!FBlueprintHelperReviewDebugBundleService::LoadBundleText(BundlePath, BundleText, &OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> BundleJson;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BundleText);
	if (!FJsonSerializer::Deserialize(Reader, BundleJson) || !BundleJson.IsValid())
	{
		OutError = TEXT("debug_bundle_json_parse_failed");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* EventValues = nullptr;
	if (!BundleJson->TryGetArrayField(TEXT("events"), EventValues) || !EventValues)
	{
		OutTimeline.Events.Reset();
		return true;
	}

	OutTimeline.Events.Reset();
	for (const TSharedPtr<FJsonValue>& EventValue : *EventValues)
	{
		if (!EventValue.IsValid())
		{
			continue;
		}
		const TSharedPtr<FJsonObject> EventObject = EventValue->AsObject();
		if (!EventObject.IsValid() || !ProjectionRegistry.IsValid())
		{
			continue;
		}
		OutTimeline.Events.Append(ProjectionRegistry->ProjectEvent(EventObject));
	}
	return true;
}
