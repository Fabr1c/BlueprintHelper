// BlueprintHelper Review row highlight sync service.

#pragma once

#include "CoreMinimal.h"
#include "UI/Review/BlueprintHelperReviewSurfaceDiffFramePresenter.h"

struct FBlueprintHelperReviewPanelSurfacePresenterArgs;
struct FBlueprintHelperReviewSurfaceDiffProjectionModel;

struct BLUEPRINTHELPER_API FBlueprintHelperReviewRowHighlightSyncCallbacks
{
	TFunction<TArray<FBlueprintHelperReviewSurfaceDiffProjectionModel>(
		const FBlueprintHelperReviewSurfaceDiffFrameRoute&)> BuildSurfaceDiffModels;
	TFunction<void(
		FBlueprintHelperReviewPanelSurfacePresenterArgs&,
		const TArray<FBlueprintHelperReviewSurfaceDiffProjectionModel>&)> ConfigurePresenterArgs;
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewRowHighlightSyncService
{
public:
	int32 Sync(
		const FString& PreferredAssetPath,
		const FBlueprintHelperReviewRowHighlightSyncCallbacks& Callbacks) const;

#if WITH_DEV_AUTOMATION_TESTS
	static TArray<EBlueprintHelperReviewSurface> GetDefaultSurfacesForTesting();
#endif

private:
	static void AddDefaultSurfaces(TArray<EBlueprintHelperReviewSurface>& OutSurfaces);
};
