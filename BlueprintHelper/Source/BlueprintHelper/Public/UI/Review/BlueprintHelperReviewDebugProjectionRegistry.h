// BlueprintHelper Review debug projection registry.

#pragma once

#include "CoreMinimal.h"
#include "UI/Review/BlueprintHelperReviewDebugProjectionAdapter.h"

struct FBlueprintHelperReviewDebugProjectionLookup
{
	bool bAvailable = false;
	FString EventType;
	FString Message;
	TSharedPtr<IBlueprintHelperReviewDebugProjectionAdapter> Adapter;
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewDebugProjectionRegistry
{
public:
	static TSharedRef<FBlueprintHelperReviewDebugProjectionRegistry> CreateDefault();

	bool RegisterProjectionAdapter(
		const TSharedRef<IBlueprintHelperReviewDebugProjectionAdapter>& Adapter,
		FString& OutError);

	FBlueprintHelperReviewDebugProjectionLookup FindProjectionAdapter(const FString& EventType) const;
	TArray<FBlueprintHelperReviewDebugEventModel> ProjectEvent(const TSharedPtr<FJsonObject>& EventJson) const;
	void RegisterBuiltInAdapters();

private:
	static FString NormalizeEventType(const FString& EventType);

	TMap<FString, TSharedPtr<IBlueprintHelperReviewDebugProjectionAdapter>> AdaptersByEventType;
};
