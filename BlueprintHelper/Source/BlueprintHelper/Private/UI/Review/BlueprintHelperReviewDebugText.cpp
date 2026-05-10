#include "UI/Review/BlueprintHelperReviewDebugText.h"

FString FBlueprintHelperReviewDebugText::BuildCopyableText(const TArray<FString>& Messages)
{
	return FString::Join(Messages, LINE_TERMINATOR);
}
