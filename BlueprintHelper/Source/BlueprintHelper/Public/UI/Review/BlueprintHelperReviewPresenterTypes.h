// BlueprintHelper Review surface presenter shared data types.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UObject/StrongObjectPtr.h"

class SGraphEditor;
class UBlueprint;
class UEdGraph;
struct FBlueprintHelperReviewAssetContext;

/**
 * 路由决策结果，描述一个 change 是否应路由到指定 surface。
 */
struct BLUEPRINTHELPER_API FBlueprintHelperReviewSurfaceRouteDecision
{
	bool bShouldShow = false;
	bool bHasExplicitTargets = false;
	int32 ExplicitTargetCount = 0;
	int32 MatchingTargetCount = 0;
	FString Reason;
};

/**
 * Graph Presenter 的运行时状态，持有预览 Blueprint、Graph、Editor 的引用。
 */
struct BLUEPRINTHELPER_API FBlueprintHelperReviewGraphPresenterState
{
	TSharedPtr<SGraphEditor> GraphEditorWidget;
	TStrongObjectPtr<UBlueprint> PreviewBlueprint;
	TStrongObjectPtr<UEdGraph> PreviewGraph;

	void Reset();
};

/**
 * Graph Presenter 的构建参数。
 */
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

/**
 * 用于定位 change 在 surface 上的几何锚点信息。
 */
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

/**
 * 行高亮信息，用于标记某行所属的 change。
 */
struct BLUEPRINTHELPER_API FBlueprintHelperReviewRowHighlight
{
	FString ChangeId;
	FString TargetKey;
	EBlueprintHelperReviewChangeKind ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	bool bSelected = false;
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

/**
 * Panel Surface Presenter 的构建参数，包含 change 列表、回调函数等。
 */
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
