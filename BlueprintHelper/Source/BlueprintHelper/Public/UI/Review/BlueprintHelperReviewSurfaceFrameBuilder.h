// BlueprintHelper Review surface frame builder.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewPresenterTypes.h"

struct FBlueprintHelperReviewPanelSurfacePresenterArgs;

/**
 * Surface 帧构建器，提供 diff frame、review list overlay 等 UI 构建方法。
 */
class BLUEPRINTHELPER_API FBlueprintHelperReviewSurfaceFrameBuilder
{
public:
	static FString GetReviewTargetText(
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewSurface Surface);

	static FString BuildReadableChangeTitle(const FBlueprintHelperReviewVisibleChange& Change);

	static TSharedRef<SWidget> BuildReviewListOverlay(
		const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args,
		EBlueprintHelperReviewSurface Surface,
		bool (*Predicate)(const FBlueprintHelperReviewVisibleChange&));

	static TSharedRef<SWidget> BuildDiffFrame(
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item,
		const TSharedRef<SWidget>& Content,
		bool bShowActions,
		bool bFillBackground,
		const FSlateColor& FrameColor,
		const TFunction<FReply(const FString&)>& OnAcceptChangeId,
		const TFunction<FReply(const FString&)>& OnRejectChangeId,
		bool bSelected = false);

	static FLinearColor GetDiffFrameBackgroundColor(bool bFillBackground);
	static FLinearColor GetDiffFrameFillColor(
		const FLinearColor& FrameColor,
		bool bFillBackground,
		bool bSelected);
};
