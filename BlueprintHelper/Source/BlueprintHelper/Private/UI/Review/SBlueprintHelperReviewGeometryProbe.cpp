// BlueprintHelper Review geometry probe widget.

#include "UI/Review/SBlueprintHelperReviewGeometryProbe.h"
#include "UI/Review/BlueprintHelperReviewRowHighlightModel.h"

void SBlueprintHelperReviewGeometryProbe::Construct(const FArguments& InArgs)
{
	Surface = InArgs._Surface;
	TargetKey = InArgs._TargetKey;
	OnGeometryInvalidated = InArgs._OnGeometryInvalidated;

	ChildSlot
	[
		InArgs._Content.Widget
	];
}

void SBlueprintHelperReviewGeometryProbe::Tick(
	const FGeometry& AllottedGeometry,
	const double InCurrentTime,
	const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const FVector2D AbsolutePosition = AllottedGeometry.LocalToAbsolute(FVector2D::ZeroVector);
	const bool bHasValidGeometry = LocalSize.X > 0.0f && LocalSize.Y > 0.0f;
	const bool bGeometryChanged = bHasValidGeometry
		&& (!bHadValidGeometry
			|| !AbsolutePosition.Equals(LastAbsolutePosition, 0.5f)
			|| !LocalSize.Equals(LastLocalSize, 0.5f));

	bHadValidGeometry = bHasValidGeometry;
	LastAbsolutePosition = AbsolutePosition;
	LastLocalSize = LocalSize;

	if (FBlueprintHelperReviewRowHighlightModel::IsRowHighlightSurface(Surface))
	{
		return;
	}

	if (bGeometryChanged && OnGeometryInvalidated.IsBound())
	{
		OnGeometryInvalidated.Execute(Surface);
	}
}
