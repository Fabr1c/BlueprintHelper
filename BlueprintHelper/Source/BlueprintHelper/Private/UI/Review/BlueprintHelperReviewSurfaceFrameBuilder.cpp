// BlueprintHelper Review surface frame builder.

#include "UI/Review/BlueprintHelperReviewSurfaceFrameBuilder.h"
#include "UI/Review/BlueprintHelperReviewReadableTextUtils.h"
#include "UI/Review/BlueprintHelperReviewAssetContext.h"
#include "UI/Review/BlueprintHelperReviewSurfaceFrameDebugUtils.h"
#include "UI/Review/BlueprintHelperReviewSurfaceFrameGeometryUtils.h"
#include "UI/Review/BlueprintHelperReviewSurfaceFrameWidgetUtils.h"
#include "UI/Review/BlueprintHelperReviewSurfaceRouter.h"
#include "UI/Review/BlueprintHelperReviewPanelStateService.h"
#include "UI/Review/SBlueprintHelperReviewDiffFrame.h"
#include "UI/Review/Utils/BlueprintHelperReviewUIUtils.h"

#include "Widgets/SCanvas.h"
#include "Widgets/SNullWidget.h"

void BlueprintHelperReviewSetSurfaceFrameGeometryPadding(const FVector2D& Padding);
void BlueprintHelperReviewSetSurfaceFrameWidgetStyle(
	float FrameOuterPadding,
	float ActionPadding,
	const FMargin& ActionSpacing,
	float FillAlpha,
	float SelectedFillAlpha);


FString FBlueprintHelperReviewSurfaceFrameBuilder::GetReviewTargetText(
	const FBlueprintHelperReviewVisibleChange& Change,
	EBlueprintHelperReviewSurface Surface)
{
	return FBlueprintHelperReviewReadableTextUtils::GetReviewListTargetText(
		MakeShared<FBlueprintHelperReviewVisibleChange>(Change),
		Surface);
}

FString FBlueprintHelperReviewSurfaceFrameBuilder::BuildReadableChangeTitle(
	const FBlueprintHelperReviewVisibleChange& Change)
{
	return FBlueprintHelperReviewReadableTextUtils::BuildReadableChangeTitle(Change);
}

TSharedRef<SWidget> FBlueprintHelperReviewSurfaceFrameBuilder::BuildReviewListOverlay(
	const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args,
	EBlueprintHelperReviewSurface Surface,
	bool (*Predicate)(const FBlueprintHelperReviewVisibleChange&))
{
	if (!Args.ChangeItems)
	{
		return SNullWidget::NullWidget;
	}

	struct FReviewPanelVisibleFrame
	{
		TSharedPtr<FBlueprintHelperReviewVisibleChange> Item;
		FString TargetText;
		FBlueprintHelperReviewSurfaceGeometryAnchor GeometryAnchor;
		bool bHasStableGeometry = false;
	};

	const FString CurrentAssetPath = Args.SelectedChange.IsValid()
		? Args.SelectedChange->AssetPath
		: FString();
	TArray<FReviewPanelVisibleFrame> VisibleFrames;
	BlueprintHelperReviewSetSurfaceFrameGeometryPadding(Args.ReviewPanelSettings.SurfaceGeometryPadding);
	BlueprintHelperReviewSetSurfaceFrameWidgetStyle(
		Args.ReviewPanelSettings.DiffFrameOuterPadding,
		Args.ReviewPanelSettings.DiffActionPadding,
		Args.ReviewPanelSettings.DiffActionSpacing,
		Args.ReviewPanelSettings.SurfaceOverlayFillAlpha,
		Args.ReviewPanelSettings.SurfaceOverlaySelectedFillAlpha);

	for (const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item : *Args.ChangeItems)
	{
		if (!Item.IsValid())
		{
			continue;
		}
		if (Args.ReviewPanelSettings.bOverlayFilterCurrentAssetOnly
			&& !CurrentAssetPath.IsEmpty()
			&& Item->AssetPath != CurrentAssetPath)
		{
			continue;
		}

		const FBlueprintHelperReviewSurfaceRouteDecision RouteDecision =
			FBlueprintHelperReviewSurfacePresenterRouter::RouteChangeToSurface(*Item, Surface);
		if (Args.AddDebugMessage)
		{
			const EBlueprintHelperReviewAssetKind AssetKind = Args.AssetContext
				? Args.AssetContext->AssetKind
				: EBlueprintHelperReviewAssetKind::Unknown;
			Args.AddDebugMessage(FBlueprintHelperReviewSurfacePresenterRouter::BuildRouteDebugSummary(
				*Item,
				Surface,
				RouteDecision,
				BlueprintHelperReviewAssetKindToString(AssetKind)));
		}
		if (!RouteDecision.bShouldShow || !Predicate(*Item))
		{
			continue;
		}

		const FString TargetText = FBlueprintHelperReviewReadableTextUtils::GetReviewListTargetText(Item, Surface);
		FReviewPanelVisibleFrame Frame;
		Frame.Item = Item;
		Frame.TargetText = TargetText;
		Frame.bHasStableGeometry = FBlueprintHelperReviewSurfaceFrameGeometryUtils::TryResolveSlateRowGeometry(
			Item,
			Surface,
			TargetText,
			Args,
			Frame.GeometryAnchor);
		if (Frame.bHasStableGeometry)
		{
			FBlueprintHelperReviewSurfaceFrameGeometryUtils::ApplyRowGeometryPadding(Frame.GeometryAnchor);
		}
		VisibleFrames.Add(Frame);
	}

	if (VisibleFrames.Num() == 0)
	{
		return SNullWidget::NullWidget;
	}

	TSharedRef<SCanvas> GeometryCanvas = SNew(SCanvas);
	int32 StableFrameCount = 0;
	for (const FReviewPanelVisibleFrame& Frame : VisibleFrames)
	{
		const FString TargetText = Frame.GeometryAnchor.TargetText.IsEmpty()
			? Frame.TargetText
			: Frame.GeometryAnchor.TargetText;
		if (!Frame.bHasStableGeometry)
		{
			if (Frame.Item.IsValid())
			{
				const FString FrameReason = Frame.GeometryAnchor.Reason.IsEmpty()
					? TEXT("geometry_not_ready")
					: Frame.GeometryAnchor.Reason;
				FBlueprintHelperReviewSurfaceFrameDebugUtils::EmitDedupedFrameDebug(
					Args.AddDebugMessage,
					FString::Printf(
						TEXT("ReviewFrameGeometry change=%s surface=%s mode=slate_row result=pending reason=geometry_not_ready frameReason=%s target=\"%s\""),
						*Frame.Item->ChangeId,
						BlueprintHelperReviewSurfaceToString(Surface),
						*FrameReason,
						*TargetText),
					Surface,
					Frame.Item->ChangeId,
					TEXT("pending"),
					FrameReason);
			}
			continue;
		}

		if (Frame.Item.IsValid())
		{
			const FString GeometryMode = Frame.GeometryAnchor.DebugMode.IsEmpty()
				? TEXT("slate_row")
				: Frame.GeometryAnchor.DebugMode;
			FBlueprintHelperReviewSurfaceFrameDebugUtils::EmitDedupedFrameDebug(
				Args.AddDebugMessage,
				FString::Printf(
					TEXT("ReviewFrameGeometry change=%s surface=%s mode=%s result=shown reason=%s pos=(%.1f,%.1f) size=(%.1f,%.1f) target=\"%s\""),
					*Frame.Item->ChangeId,
					BlueprintHelperReviewSurfaceToString(Surface),
					*GeometryMode,
					*Frame.GeometryAnchor.Reason,
					static_cast<double>(Frame.GeometryAnchor.Position.X),
					static_cast<double>(Frame.GeometryAnchor.Position.Y),
					static_cast<double>(Frame.GeometryAnchor.Size.X),
					static_cast<double>(Frame.GeometryAnchor.Size.Y),
					*TargetText),
				Surface,
				Frame.Item->ChangeId,
				TEXT("shown"),
				Frame.GeometryAnchor.Reason);
		}

		bool bSelected = false;
		if (Frame.Item.IsValid() && Args.SelectedChange.IsValid())
		{
			bSelected = Frame.Item->ChangeId == Args.SelectedChange->ChangeId;
		}
		const FSlateColor FrameColor = bSelected && Frame.Item.IsValid() && Args.GetSelectedDiffColor
			? Args.GetSelectedDiffColor()
			: (Frame.Item.IsValid() && Args.GetChangeColor
				? Args.GetChangeColor(Frame.Item->ChangeKind)
				: FSlateColor(FLinearColor::Transparent));

		GeometryCanvas->AddSlot()
		.Position(Frame.GeometryAnchor.Position)
		.Size(Frame.GeometryAnchor.Size)
		[
			UBlueprintHelperReviewUIUtils::BlueprintHelperReviewBuildDiffFrameWidget(
				Frame.Item,
				SNullWidget::NullWidget,
				false,
				true,
				FrameColor,
				Args.OnReviewActionIntent,
				Args.ReviewPanelSettings,
				bSelected)
		];
		++StableFrameCount;
	}

	if (StableFrameCount > 0)
	{
		return GeometryCanvas;
	}

	return SNullWidget::NullWidget;
}

FLinearColor FBlueprintHelperReviewSurfaceFrameBuilder::GetDiffFrameBackgroundColor(bool bFillBackground)
{
	return FBlueprintHelperReviewSurfaceFrameWidgetUtils::GetReviewFrameBackgroundColor(bFillBackground);
}

FLinearColor FBlueprintHelperReviewSurfaceFrameBuilder::GetDiffFrameFillColor(
	const FLinearColor& FrameColor,
	bool bFillBackground,
	bool bSelected)
{
	return FBlueprintHelperReviewSurfaceFrameWidgetUtils::GetReviewFrameFillColor(
		FrameColor,
		bFillBackground,
		bSelected);
}

TSharedRef<SWidget> FBlueprintHelperReviewSurfaceFrameBuilder::BuildDiffFrame(
	const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item,
	const TSharedRef<SWidget>& Content,
	bool bShowActions,
	bool bFillBackground,
		const FSlateColor& FrameColor,
		const TFunction<FReply(const FBlueprintHelperReviewActionIntent&)>& OnReviewActionIntent,
		const FBlueprintHelperReviewPanelSettings& ReviewPanelSettings,
		bool bSelected)
{
	BlueprintHelperReviewSetSurfaceFrameWidgetStyle(
		ReviewPanelSettings.DiffFrameOuterPadding,
		ReviewPanelSettings.DiffActionPadding,
		ReviewPanelSettings.DiffActionSpacing,
		ReviewPanelSettings.SurfaceOverlayFillAlpha,
		ReviewPanelSettings.SurfaceOverlaySelectedFillAlpha);
	return UBlueprintHelperReviewUIUtils::BlueprintHelperReviewBuildDiffFrameWidget(
		Item,
		Content,
		bShowActions,
		bFillBackground,
		FrameColor,
		OnReviewActionIntent,
		ReviewPanelSettings,
		bSelected);
}
