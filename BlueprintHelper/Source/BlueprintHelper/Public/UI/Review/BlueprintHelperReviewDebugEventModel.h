// BlueprintHelper Review debug event model.

#pragma once

#include "CoreMinimal.h"

struct FBlueprintHelperReviewDebugEventModel
{
	FString EventId;
	FString Timestamp;
	FString EventType;
	FString ReviewEventId;
	FString AssetPath;
	FString TargetKey;
	FString SurfaceKind;
	FString ActionKind;
	FString Result;
	FString Message;
};

struct FBlueprintHelperReviewDebugTimelineModel
{
	TArray<FBlueprintHelperReviewDebugEventModel> Events;
};

struct FBlueprintHelperReviewDebugDetailModel
{
	FBlueprintHelperReviewDebugEventModel Event;
	FString RawJson;
};
