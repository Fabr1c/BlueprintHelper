// BlueprintHelper Review row highlight sync service.

#include "UI/Review/BlueprintHelperReviewRowHighlightSyncService.h"

#include "UI/Review/BlueprintHelperReviewRowHighlightModel.h"
#include "UI/Review/BlueprintHelperReviewSurfacePresenter.h"

int32 FBlueprintHelperReviewRowHighlightSyncService::Sync(
	const FString& PreferredAssetPath,
	const FBlueprintHelperReviewRowHighlightSyncCallbacks& Callbacks) const
{
	if (!Callbacks.BuildSurfaceDiffModels || !Callbacks.ConfigurePresenterArgs)
	{
		return 0;
	}

	TArray<EBlueprintHelperReviewSurface> Surfaces;
	AddDefaultSurfaces(Surfaces);

	int32 SyncedSurfaceCount = 0;
	for (const EBlueprintHelperReviewSurface Surface : Surfaces)
	{
		const FBlueprintHelperReviewSurfaceDiffFrameRoute Route =
			FBlueprintHelperReviewSurfaceDiffFramePresenter::ResolveSurfaceRoute(Surface);
		if (Route.ShouldShowChange == nullptr)
		{
			continue;
		}

		const TArray<FBlueprintHelperReviewSurfaceDiffProjectionModel> SurfaceDiffModels =
			Callbacks.BuildSurfaceDiffModels(Route);
		FBlueprintHelperReviewPanelSurfacePresenterArgs Args;
		Callbacks.ConfigurePresenterArgs(Args, SurfaceDiffModels);
		FBlueprintHelperReviewRowHighlightModel::RebuildSurfaceState(
			Args,
			Route.Surface,
			Route.ShouldShowChange,
			PreferredAssetPath);
		++SyncedSurfaceCount;
	}

	return SyncedSurfaceCount;
}

#if WITH_DEV_AUTOMATION_TESTS
TArray<EBlueprintHelperReviewSurface> FBlueprintHelperReviewRowHighlightSyncService::GetDefaultSurfacesForTesting()
{
	TArray<EBlueprintHelperReviewSurface> Surfaces;
	AddDefaultSurfaces(Surfaces);
	return Surfaces;
}
#endif

void FBlueprintHelperReviewRowHighlightSyncService::AddDefaultSurfaces(
	TArray<EBlueprintHelperReviewSurface>& OutSurfaces)
{
	OutSurfaces.Add(EBlueprintHelperReviewSurface::Components);
	OutSurfaces.Add(EBlueprintHelperReviewSurface::UMGWidgetTree);
	OutSurfaces.Add(EBlueprintHelperReviewSurface::MyBlueprint);
	OutSurfaces.Add(EBlueprintHelperReviewSurface::Details);
	OutSurfaces.Add(EBlueprintHelperReviewSurface::DataTable);
	OutSurfaces.Add(EBlueprintHelperReviewSurface::DataAsset);
	OutSurfaces.Add(EBlueprintHelperReviewSurface::Material);
}
