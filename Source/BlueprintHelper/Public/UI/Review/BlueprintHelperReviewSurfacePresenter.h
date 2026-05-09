// BlueprintHelper Review surface presenter routing helpers.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewAssetContext.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SWidget.h"

class SKismetInspector;
class SGraphEditor;
class SMyBlueprint;
class UBlueprint;
class UEdGraph;

struct BLUEPRINTHELPER_API FBlueprintHelperReviewSurfaceRouteDecision
{
	bool bShouldShow = false;
	bool bHasExplicitTargets = false;
	int32 ExplicitTargetCount = 0;
	int32 MatchingTargetCount = 0;
	FString Reason;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewGraphPresenterState
{
	TSharedPtr<SGraphEditor> GraphEditorWidget;
	TStrongObjectPtr<UBlueprint> PreviewBlueprint;
	TStrongObjectPtr<UEdGraph> PreviewGraph;

	void Reset();
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewGraphPresenterArgs
{
	const FBlueprintHelperReviewAssetContext* AssetContext = nullptr;
	const TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>>* ChangeItems = nullptr;
	TSharedPtr<FBlueprintHelperReviewVisibleChange> SelectedChange;
	TFunction<void(const FString&)> AddDebugMessage;
	TFunction<FReply(const FString&)> OnAcceptChangeId;
	TFunction<FReply(const FString&)> OnRejectChangeId;
	TFunction<FSlateColor(EBlueprintHelperReviewChangeKind)> GetChangeColor;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewSurfaceGeometryAnchor
{
	bool bIsValid = false;
	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Size = FVector2D::ZeroVector;
	FString TargetText;
	FString Reason;
};

DECLARE_DELEGATE_RetVal_ThreeParams(
	bool,
	FBlueprintHelperReviewResolveRowGeometry,
	const FBlueprintHelperReviewVisibleChange&,
	EBlueprintHelperReviewSurface,
	FBlueprintHelperReviewSurfaceGeometryAnchor&);

struct BLUEPRINTHELPER_API FBlueprintHelperReviewPanelSurfacePresenterArgs
{
	const FBlueprintHelperReviewAssetContext* AssetContext = nullptr;
	const TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>>* ChangeItems = nullptr;
	TSharedPtr<FBlueprintHelperReviewVisibleChange> SelectedChange;
	TFunction<void(const FString&)> AddDebugMessage;
	TFunction<FReply(TSharedPtr<FBlueprintHelperReviewVisibleChange>)> OnAcceptChange;
	TFunction<FReply(TSharedPtr<FBlueprintHelperReviewVisibleChange>)> OnRejectChange;
	TFunction<FSlateColor(EBlueprintHelperReviewChangeKind)> GetChangeColor;
	TFunction<FSlateColor()> GetSelectedDiffColor;
	FBlueprintHelperReviewResolveRowGeometry ResolveRowGeometry;
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewSurfacePresenterRouter
{
public:
	static FBlueprintHelperReviewSurfaceRouteDecision RouteChangeToSurface(
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewSurface Surface);

	static bool ShouldShowChangeOnSurface(
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewSurface Surface);

	static FString BuildRouteDebugSummary(
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewSurface Surface,
		const FBlueprintHelperReviewSurfaceRouteDecision& Decision,
		const TCHAR* AssetKindName);
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewGraphPresenter
{
public:
	static bool ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change);
	static TSharedRef<SWidget> BuildContent(
		const FBlueprintHelperReviewGraphPresenterArgs& Args,
		FBlueprintHelperReviewGraphPresenterState& State);
	static UEdGraph* ResolveGraphForSelection(
		const FBlueprintHelperReviewAssetContext& Context,
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& SelectedChange);
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewBlueprintComponentsPresenter
{
public:
	static bool ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change);
	static TSharedRef<SWidget> BuildContent(const FBlueprintHelperReviewAssetContext& Context);
	static TSharedRef<SWidget> BuildOverlay(const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args);
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewMyBlueprintPresenter
{
public:
	static bool ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change);
	static TSharedRef<SWidget> BuildContent(
		const FBlueprintHelperReviewAssetContext& Context,
		TSharedPtr<SMyBlueprint>& OutMyBlueprintWidget,
		const TSharedPtr<SKismetInspector>& KismetInspector);
	static TSharedRef<SWidget> BuildOverlay(const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args);
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewObjectDetailsPresenter
{
public:
	static bool ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change);
	static TSharedRef<SWidget> BuildContent(
		const FBlueprintHelperReviewAssetContext& Context,
		TSharedPtr<SKismetInspector>& OutKismetInspector,
		const TSharedPtr<SMyBlueprint>& MyBlueprintWidget);
	static TSharedRef<SWidget> BuildOverlay(const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args);
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewSurfaceFrameBuilder
{
public:
	static FString GetReviewTargetText(
		const FBlueprintHelperReviewVisibleChange& Change,
		EBlueprintHelperReviewSurface Surface);

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
		const TFunction<FReply(TSharedPtr<FBlueprintHelperReviewVisibleChange>)>& OnAcceptChange,
		const TFunction<FReply(TSharedPtr<FBlueprintHelperReviewVisibleChange>)>& OnRejectChange);

	static FLinearColor GetDiffFrameBackgroundColor(bool bFillBackground);
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewSlateRowGeometryRegistry
{
public:
	static void RegisterRow(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& SearchText,
		const TSharedRef<SWidget>& RowWidget);

	static bool ResolveRowGeometry(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& TargetText,
		const TSharedPtr<SWidget>& OverlayWidget,
		FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor);
};
