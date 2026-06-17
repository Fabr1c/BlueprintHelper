// BlueprintHelper Review details geometry resolver.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "UI/Review/BlueprintHelperReviewPresenterTypes.h"

class SKismetInspector;
class SWidget;
class UObject;

struct BLUEPRINTHELPER_API FBlueprintHelperReviewDetailsGeometryResolutionContext
{
	TSharedPtr<SKismetInspector> KismetInspector;
	TFunction<UObject*()> ResolveDetailsObject;
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewDetailsGeometryResolver
{
public:
	bool ResolveRowGeometry(
		const FBlueprintHelperReviewVisibleChange& Change,
		const TSharedPtr<SWidget>& OverlayWidget,
		const FBlueprintHelperReviewDetailsGeometryResolutionContext& Context,
		FBlueprintHelperReviewSurfaceGeometryAnchor& OutAnchor) const;
};
