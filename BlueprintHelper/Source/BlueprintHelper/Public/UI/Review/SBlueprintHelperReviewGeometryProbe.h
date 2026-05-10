// BlueprintHelper Review geometry probe widget.
// Detects geometry changes on surfaces and invalidates overlays accordingly.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewPresenterTypes.h"
#include "Widgets/SCompoundWidget.h"

/**
 * 几何探测 widget，用于监听 surface 上 widget 几何变化并触发回调。
 */
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
