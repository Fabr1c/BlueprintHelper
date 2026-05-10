// BlueprintHelper Review panel style helpers.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

class FBlueprintHelperReviewPanelStyle
{
public:
	static FText StatusToText(EBlueprintHelperReviewChangeStatus Status);
	static FLinearColor GetReviewGreen();
	static FLinearColor GetReviewRed();
	static FLinearColor GetReviewYellow();
	static FLinearColor GetReviewPanelBackground();
	static FLinearColor GetReviewSectionBackground();
};
