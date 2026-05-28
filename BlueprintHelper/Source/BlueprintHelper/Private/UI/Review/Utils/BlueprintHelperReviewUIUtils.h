// BlueprintHelper Review UI utility functions.
// Aggregates static helpers extracted from anonymous namespaces across Review UI files.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"
#include "Input/Reply.h"
#include "Layout/Margin.h"
#include "Styling/SlateColor.h"
#include "Templates/Function.h"
#include "UI/Review/BlueprintHelperReviewPanelSettings.h"
#include "UI/Review/BlueprintHelperReviewPresenterTypes.h"

#include "BlueprintHelperReviewUIUtils.generated.h"

class UBlueprintHelperReviewDiffBlockNode;

UCLASS()
class BLUEPRINTHELPER_API UBlueprintHelperReviewUIUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Diff block node action style data. */
	struct FBlueprintHelperReviewDiffBlockNodeActionStyle
	{
		float ActionPadding = 5.0f;
		FMargin ActionSpacing = FMargin(0.0f, 0.0f, 6.0f, 0.0f);
		FMargin ActionAnchorPadding = FMargin(0.0f, 0.0f, 10.0f, 10.0f);
	};

	// === From BlueprintHelperReviewSurfaceFrameBuilder.cpp ===

	static TSharedRef<SWidget> BlueprintHelperReviewBuildDiffFrameWidget(
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item,
		const TSharedRef<SWidget>& Content,
		bool bShowActions,
		bool bFillBackground,
		const FSlateColor& FrameColor,
		const TFunction<FReply(const FBlueprintHelperReviewActionIntent&)>& OnReviewActionIntent,
		const FBlueprintHelperReviewPanelSettings& ReviewPanelSettings,
		bool bSelected);

	// === From BlueprintHelperReviewPanelSettingsResolver.cpp ===

	static TArray<float> BlueprintHelperReviewResolveFloatArray(
		const FString& DotPath,
		const TArray<float>& DefaultValue);

	static float BlueprintHelperReviewResolveNonNegativeFloat(
		const FString& DotPath,
		float DefaultValue);

	static float BlueprintHelperReviewResolveUnitFloat(
		const FString& DotPath,
		float DefaultValue);

	// === From BlueprintHelperReviewDiffBlockNode.cpp ===

	static FBlueprintHelperReviewDiffBlockNodeActionStyle BlueprintHelperReviewGetDiffBlockNodeActionStyle(
		const UBlueprintHelperReviewDiffBlockNode* Node);

	static void BlueprintHelperReviewApplyDiffBlockNodeActionStyle(
		UBlueprintHelperReviewDiffBlockNode* Node,
		float InActionPadding,
		const FMargin& InActionSpacing,
		const FMargin& InActionAnchorPadding);

	/** Access the shared action style map for diff block nodes. */
	static TMap<const UBlueprintHelperReviewDiffBlockNode*, FBlueprintHelperReviewDiffBlockNodeActionStyle>& GetDiffBlockNodeActionStyles();
};
