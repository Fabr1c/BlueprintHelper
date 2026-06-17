// BlueprintHelper Review generic debug projection adapter.

#pragma once

#include "CoreMinimal.h"
#include "UI/Review/BlueprintHelperReviewDebugProjectionAdapter.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewGenericDebugProjectionAdapter
	: public IBlueprintHelperReviewDebugProjectionAdapter
{
public:
	explicit FBlueprintHelperReviewGenericDebugProjectionAdapter(const FString& InEventType);

	virtual FString GetEventType() const override;
	virtual FBlueprintHelperReviewDebugProjectionResult Project(
		const TSharedPtr<FJsonObject>& EventJson) const override;

private:
	FString EventType;
};
