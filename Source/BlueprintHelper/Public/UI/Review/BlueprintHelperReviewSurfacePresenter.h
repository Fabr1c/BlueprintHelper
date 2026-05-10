// BlueprintHelper Review surface presenter routing helpers.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewAssetContext.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWidget.h"

class SKismetInspector;
class SGraphEditor;
class SSubobjectBlueprintEditor;
class UBlueprint;
class UEdGraph;
template <typename ItemType>
class STreeView;

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
	FVector2D HostSize = FVector2D::ZeroVector;
	FString TargetText;
	FString Reason;
	FString DebugMode;
};

DECLARE_DELEGATE_RetVal_ThreeParams(
	bool,
	FBlueprintHelperReviewResolveRowGeometry,
	const FBlueprintHelperReviewVisibleChange&,
	EBlueprintHelperReviewSurface,
	FBlueprintHelperReviewSurfaceGeometryAnchor&);

DECLARE_DELEGATE_OneParam(
	FBlueprintHelperReviewGeometryInvalidated,
	EBlueprintHelperReviewSurface);

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
	FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated;
};

class BLUEPRINTHELPER_API SBlueprintHelperReviewGeometryProbe : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintHelperReviewGeometryProbe)
		: _Surface(EBlueprintHelperReviewSurface::Unknown)
	{
	}

	SLATE_ARGUMENT(EBlueprintHelperReviewSurface, Surface)
	SLATE_ARGUMENT(FString, TargetKey)
	SLATE_EVENT(FBlueprintHelperReviewGeometryInvalidated, OnGeometryInvalidated)
	SLATE_DEFAULT_SLOT(FArguments, Content)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	EBlueprintHelperReviewSurface Surface = EBlueprintHelperReviewSurface::Unknown;
	FString TargetKey;
	FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated;
	bool bHadValidGeometry = false;
	FVector2D LastAbsolutePosition = FVector2D::ZeroVector;
	FVector2D LastLocalSize = FVector2D::ZeroVector;
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

	static EBlueprintHelperReviewSurface GetStructurePanelSurfaceForAssetKind(
		EBlueprintHelperReviewAssetKind AssetKind);

	static EBlueprintHelperReviewSurface GetMainWorkspaceSurfaceForAssetKind(
		EBlueprintHelperReviewAssetKind AssetKind);

	static bool ShouldDetailsPanelOwnOverlay(EBlueprintHelperReviewSurface Surface);

	static bool ShouldMainWorkspaceOwnOverlay(EBlueprintHelperReviewSurface Surface);
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
	struct FState
	{
		TSharedPtr<SSubobjectBlueprintEditor> SubobjectEditor;
		FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated;
	};

	static bool ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change);
	static TSharedRef<SWidget> BuildContent(
		const FBlueprintHelperReviewAssetContext& Context,
		FState& State,
		FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated = FBlueprintHelperReviewGeometryInvalidated());
	static TSharedRef<SWidget> BuildOverlay(const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args);
	static bool ResolveRowGeometry(
		const FBlueprintHelperReviewVisibleChange& Change,
		FState& State,
		const TSharedPtr<SWidget>& OverlayWidget,
		FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor);
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewMyBlueprintPresenter
{
public:
	enum class ERowKind : uint8
	{
		Section,
		Graph,
		Function,
		Macro,
		Event,
		Dispatcher,
		Variable,
		ReviewOnly
	};

	struct FRowItem
	{
		FText Label;
		FString SearchText;
		FName IconName;
		ERowKind Kind = ERowKind::ReviewOnly;
		TArray<TSharedPtr<FRowItem>> Children;
		TWeakPtr<SWidget> RowWidget;
	};

	struct FState
	{
		TArray<TSharedPtr<FRowItem>> RootItems;
		TSharedPtr<STreeView<TSharedPtr<FRowItem>>> TreeView;
		FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated;
	};

	static bool ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change);
	static TSharedRef<SWidget> BuildContent(
		const FBlueprintHelperReviewAssetContext& Context,
		FState& State,
		const TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>>& ChangeItems,
		FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated = FBlueprintHelperReviewGeometryInvalidated());
	static TSharedRef<SWidget> BuildOverlay(const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args);
	static bool ResolveRowGeometry(
		const FBlueprintHelperReviewVisibleChange& Change,
		FState& State,
		const TSharedPtr<SWidget>& OverlayWidget,
		FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor);
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewObjectDetailsPresenter
{
public:
	static bool ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change);
	static TSharedRef<SWidget> BuildContent(
		const FBlueprintHelperReviewAssetContext& Context,
		TSharedPtr<SKismetInspector>& OutKismetInspector);
	static TSharedRef<SWidget> BuildOverlay(const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args);
};

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
		const TFunction<FReply(TSharedPtr<FBlueprintHelperReviewVisibleChange>)>& OnAcceptChange,
		const TFunction<FReply(TSharedPtr<FBlueprintHelperReviewVisibleChange>)>& OnRejectChange,
		bool bSelected = false);

	static FLinearColor GetDiffFrameBackgroundColor(bool bFillBackground);
	static FLinearColor GetDiffFrameFillColor(
		const FLinearColor& FrameColor,
		bool bFillBackground,
		bool bSelected);
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewSlateRowGeometryRegistry
{
public:
	static void RegisterRow(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& SearchText,
		const TSharedRef<SWidget>& RowWidget,
		const TCHAR* DebugMode = TEXT("slate_row"));

	static bool ResolveRowGeometry(
		const FString& AssetPath,
		EBlueprintHelperReviewSurface Surface,
		const FString& TargetText,
		const TSharedPtr<SWidget>& OverlayWidget,
		FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor);
};
