// BlueprintHelper Review UI utility functions implementation.

#include "UI/Review/Utils/BlueprintHelperReviewUIUtils.h"

#include "UI/Review/BlueprintHelperReviewDiffBlockNode.h"
#include "UI/Review/BlueprintHelperReviewPanelStateService.h"
#include "UI/Review/SBlueprintHelperReviewDiffFrame.h"
#include "Systems/Config/BlueprintHelperRuntimeSettingResolver.h"

#include "Dom/JsonValue.h"

// Shared action style map for diff block nodes.
static TMap<const UBlueprintHelperReviewDiffBlockNode*, UBlueprintHelperReviewUIUtils::FBlueprintHelperReviewDiffBlockNodeActionStyle> GBlueprintHelperReviewDiffBlockNodeActionStyles_Impl;

TMap<const UBlueprintHelperReviewDiffBlockNode*, UBlueprintHelperReviewUIUtils::FBlueprintHelperReviewDiffBlockNodeActionStyle>& UBlueprintHelperReviewUIUtils::GetDiffBlockNodeActionStyles()
{
	return GBlueprintHelperReviewDiffBlockNodeActionStyles_Impl;
}

// === From BlueprintHelperReviewSurfaceFrameBuilder.cpp ===

TSharedRef<SWidget> UBlueprintHelperReviewUIUtils::BlueprintHelperReviewBuildDiffFrameWidget(
	const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item,
	const TSharedRef<SWidget>& Content,
	bool bShowActions,
	bool bFillBackground,
	const FSlateColor& FrameColor,
	const TFunction<FReply(const FBlueprintHelperReviewActionIntent&)>& OnReviewActionIntent,
	const FBlueprintHelperReviewPanelSettings& ReviewPanelSettings,
	bool bSelected)
{
	return SNew(SBlueprintHelperReviewDiffFrame)
		.FrameColor(FrameColor)
		.ShowActions(bShowActions && Item.IsValid())
		.FillBackground(bFillBackground)
		.Selected(bSelected)
		.FrameOuterPadding(ReviewPanelSettings.DiffFrameOuterPadding)
		.ActionPadding(ReviewPanelSettings.DiffActionPadding)
		.ActionSpacing(ReviewPanelSettings.DiffActionSpacing)
		.SurfaceOverlayFillAlpha(ReviewPanelSettings.SurfaceOverlayFillAlpha)
		.SurfaceOverlaySelectedFillAlpha(ReviewPanelSettings.SurfaceOverlaySelectedFillAlpha)
		.OnAccept(FOnClicked::CreateLambda([Item, OnReviewActionIntent]()
		{
			return OnReviewActionIntent && Item.IsValid()
				? OnReviewActionIntent(FBlueprintHelperReviewActionIntent::Accept(
					FBlueprintHelperReviewPanelStateService::MakeChangeBinding(
						*Item,
						EBlueprintHelperReviewSurface::Unknown,
						Item->LocationKey),
					TEXT("diff_frame")))
				: FReply::Handled();
		}))
		.OnReject(FOnClicked::CreateLambda([Item, OnReviewActionIntent]()
		{
			return OnReviewActionIntent && Item.IsValid()
				? OnReviewActionIntent(FBlueprintHelperReviewActionIntent::Reject(
					FBlueprintHelperReviewPanelStateService::MakeChangeBinding(
						*Item,
						EBlueprintHelperReviewSurface::Unknown,
						Item->LocationKey),
					TEXT("diff_frame")))
				: FReply::Handled();
		}))
		[
			Content
		];
}

// === From BlueprintHelperReviewPanelSettingsResolver.cpp ===

TArray<float> UBlueprintHelperReviewUIUtils::BlueprintHelperReviewResolveFloatArray(
	const FString& DotPath,
	const TArray<float>& DefaultValue)
{
	const TSharedPtr<FJsonValue> JsonValue = FBlueprintHelperRuntimeSettingResolver::GetJsonValue(DotPath);
	const TArray<TSharedPtr<FJsonValue>>* JsonArray = nullptr;
	if (!JsonValue.IsValid() || !JsonValue->TryGetArray(JsonArray) || !JsonArray)
	{
		return DefaultValue;
	}

	TArray<float> Result;
	Result.Reserve(JsonArray->Num());
	for (const TSharedPtr<FJsonValue>& Entry : *JsonArray)
	{
		double Number = 0.0;
		if (!Entry.IsValid() || !Entry->TryGetNumber(Number))
		{
			return DefaultValue;
		}
		Result.Add(FMath::Max(0.0f, static_cast<float>(Number)));
	}

	return Result.Num() == DefaultValue.Num() ? Result : DefaultValue;
}

float UBlueprintHelperReviewUIUtils::BlueprintHelperReviewResolveNonNegativeFloat(
	const FString& DotPath,
	const float DefaultValue)
{
	return FMath::Max(
		0.0f,
		static_cast<float>(FBlueprintHelperRuntimeSettingResolver::GetDouble(DotPath, DefaultValue)));
}

float UBlueprintHelperReviewUIUtils::BlueprintHelperReviewResolveUnitFloat(
	const FString& DotPath,
	const float DefaultValue)
{
	return FMath::Clamp(
		static_cast<float>(FBlueprintHelperRuntimeSettingResolver::GetDouble(DotPath, DefaultValue)),
		0.0f,
		1.0f);
}

// === From BlueprintHelperReviewDiffBlockNode.cpp ===

UBlueprintHelperReviewUIUtils::FBlueprintHelperReviewDiffBlockNodeActionStyle UBlueprintHelperReviewUIUtils::BlueprintHelperReviewGetDiffBlockNodeActionStyle(
	const UBlueprintHelperReviewDiffBlockNode* Node)
{
	if (const FBlueprintHelperReviewDiffBlockNodeActionStyle* Style =
		GBlueprintHelperReviewDiffBlockNodeActionStyles_Impl.Find(Node))
	{
		return *Style;
	}
	return FBlueprintHelperReviewDiffBlockNodeActionStyle();
}

void UBlueprintHelperReviewUIUtils::BlueprintHelperReviewApplyDiffBlockNodeActionStyle(
	UBlueprintHelperReviewDiffBlockNode* Node,
	float InActionPadding,
	const FMargin& InActionSpacing,
	const FMargin& InActionAnchorPadding)
{
	if (!Node)
	{
		return;
	}

	FBlueprintHelperReviewDiffBlockNodeActionStyle& Style =
		GBlueprintHelperReviewDiffBlockNodeActionStyles_Impl.FindOrAdd(Node);
	Style.ActionPadding = FMath::Max(0.0f, InActionPadding);
	Style.ActionSpacing = InActionSpacing;
	Style.ActionAnchorPadding = InActionAnchorPadding;
}
