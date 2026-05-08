#include "UI/Review/BlueprintHelperReviewDebugText.h"

namespace BlueprintHelperReviewDebugText
{
	FString BuildCopyableText(const TArray<FString>& Messages)
	{
		return FString::Join(Messages, LINE_TERMINATOR);
	}
}
