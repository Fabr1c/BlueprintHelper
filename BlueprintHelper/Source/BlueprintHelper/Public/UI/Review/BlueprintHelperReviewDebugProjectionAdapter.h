// BlueprintHelper Review debug projection adapter.

#pragma once

#include "CoreMinimal.h"
#include "UI/Review/BlueprintHelperReviewDebugEventModel.h"

class FJsonObject;

struct FBlueprintHelperReviewDebugProjectionResult
{
	bool bProjected = false;
	FString Message;
	TArray<FBlueprintHelperReviewDebugEventModel> Events;
};

class BLUEPRINTHELPER_API IBlueprintHelperReviewDebugProjectionAdapter
{
public:
	virtual ~IBlueprintHelperReviewDebugProjectionAdapter();

	virtual FString GetEventType() const = 0;
	virtual FBlueprintHelperReviewDebugProjectionResult Project(
		const TSharedPtr<FJsonObject>& EventJson) const = 0;
};
