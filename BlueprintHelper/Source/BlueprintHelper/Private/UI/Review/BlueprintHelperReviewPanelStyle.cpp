// BlueprintHelper Review panel style helpers.

#include "UI/Review/BlueprintHelperReviewPanelStyle.h"

FText FBlueprintHelperReviewPanelStyle::StatusToText(EBlueprintHelperReviewChangeStatus Status)
{
	return FText::FromString(BlueprintHelperReviewChangeStatusToString(Status));
}

FLinearColor FBlueprintHelperReviewPanelStyle::GetReviewGreen()
{
	return FLinearColor(0.05f, 0.75f, 0.22f, 0.85f);
}

FLinearColor FBlueprintHelperReviewPanelStyle::GetReviewRed()
{
	return FLinearColor(0.95f, 0.12f, 0.10f, 0.85f);
}

FLinearColor FBlueprintHelperReviewPanelStyle::GetReviewYellow()
{
	return FLinearColor(1.0f, 0.72f, 0.08f, 0.85f);
}

FLinearColor FBlueprintHelperReviewPanelStyle::GetReviewPanelBackground()
{
	return FLinearColor(0.015f, 0.015f, 0.015f, 1.0f);
}

FLinearColor FBlueprintHelperReviewPanelStyle::GetReviewSectionBackground()
{
	return FLinearColor(0.035f, 0.035f, 0.035f, 1.0f);
}
