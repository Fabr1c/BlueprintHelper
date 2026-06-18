// BlueprintHelper Review surface presenter registry implementation.

#include "UI/Review/BlueprintHelperReviewSurfacePresenterRegistry.h"

#include "UI/Review/BlueprintHelperReviewBlueprintComponentsPresenter.h"
#include "UI/Review/BlueprintHelperReviewDataAssetPresenter.h"
#include "UI/Review/BlueprintHelperReviewDataTablePresenter.h"
#include "UI/Review/BlueprintHelperReviewGraphPresenter.h"
#include "UI/Review/BlueprintHelperReviewMaterialPresenter.h"
#include "UI/Review/BlueprintHelperReviewMyBlueprintPresenter.h"
#include "UI/Review/BlueprintHelperReviewObjectDetailsPresenter.h"
#include "UI/Review/BlueprintHelperReviewWidgetTreePresenter.h"
#include "Widgets/SNullWidget.h"

TSharedRef<FBlueprintHelperReviewSurfacePresenterRegistry>
FBlueprintHelperReviewSurfacePresenterRegistry::CreateDefault()
{
	TSharedRef<FBlueprintHelperReviewSurfacePresenterRegistry> Registry =
		MakeShared<FBlueprintHelperReviewSurfacePresenterRegistry>();

	Registry->RegisterSurfacePresenter(
		EBlueprintHelperReviewSurface::Graph,
		&FBlueprintHelperReviewGraphPresenter::ShouldShowChange);
	Registry->RegisterSurfacePresenter(
		EBlueprintHelperReviewSurface::Components,
		&FBlueprintHelperReviewBlueprintComponentsPresenter::ShouldShowChange,
		[](const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
		{
			return FBlueprintHelperReviewBlueprintComponentsPresenter::BuildOverlay(Args);
		});
	Registry->RegisterSurfacePresenter(
		EBlueprintHelperReviewSurface::UMGWidgetTree,
		&FBlueprintHelperReviewUMGWidgetTreePresenter::ShouldShowChange,
		[](const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
		{
			return FBlueprintHelperReviewUMGWidgetTreePresenter::BuildOverlay(Args);
		});
	Registry->RegisterSurfacePresenter(
		EBlueprintHelperReviewSurface::MyBlueprint,
		&FBlueprintHelperReviewMyBlueprintPresenter::ShouldShowChange,
		[](const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
		{
			return FBlueprintHelperReviewMyBlueprintPresenter::BuildOverlay(Args);
		});
	Registry->RegisterSurfacePresenter(
		EBlueprintHelperReviewSurface::Details,
		&FBlueprintHelperReviewObjectDetailsPresenter::ShouldShowChange,
		[](const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
		{
			return FBlueprintHelperReviewObjectDetailsPresenter::BuildOverlay(Args);
		});
	Registry->RegisterSurfacePresenter(
		EBlueprintHelperReviewSurface::DataTable,
		&FBlueprintHelperReviewDataTablePresenter::ShouldShowChange,
		[](const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
		{
			return FBlueprintHelperReviewDataTablePresenter::BuildOverlay(Args);
		});
	Registry->RegisterSurfacePresenter(
		EBlueprintHelperReviewSurface::DataAsset,
		&FBlueprintHelperReviewDataAssetPresenter::ShouldShowChange,
		[](const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
		{
			return FBlueprintHelperReviewDataAssetPresenter::BuildOverlay(Args);
		});
	Registry->RegisterSurfacePresenter(
		EBlueprintHelperReviewSurface::Material,
		&FBlueprintHelperReviewMaterialPresenter::ShouldShowChange,
		[](const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
		{
			return FBlueprintHelperReviewMaterialPresenter::BuildOverlay(Args);
		});

	return Registry;
}

void FBlueprintHelperReviewSurfacePresenterRegistry::RegisterSurfacePresenter(
	EBlueprintHelperReviewSurface Surface,
	FBlueprintHelperReviewSurfaceChangePredicate Predicate)
{
	FSurfacePresenterDescriptor& Descriptor = SurfacePresenters.FindOrAdd(Surface);
	Descriptor.Predicate = Predicate;
}

void FBlueprintHelperReviewSurfacePresenterRegistry::RegisterSurfacePresenter(
	EBlueprintHelperReviewSurface Surface,
	FBlueprintHelperReviewSurfaceChangePredicate Predicate,
	FBlueprintHelperReviewSurfaceOverlayBuilder OverlayBuilder)
{
	FSurfacePresenterDescriptor& Descriptor = SurfacePresenters.FindOrAdd(Surface);
	Descriptor.Predicate = Predicate;
	Descriptor.OverlayBuilder = MoveTemp(OverlayBuilder);
}

void FBlueprintHelperReviewSurfacePresenterRegistry::RegisterOverlayBuilder(
	EBlueprintHelperReviewSurface Surface,
	FBlueprintHelperReviewSurfaceOverlayBuilder OverlayBuilder)
{
	FSurfacePresenterDescriptor& Descriptor = SurfacePresenters.FindOrAdd(Surface);
	Descriptor.OverlayBuilder = MoveTemp(OverlayBuilder);
}

FBlueprintHelperReviewSurfaceChangePredicate FBlueprintHelperReviewSurfacePresenterRegistry::FindPredicate(
	EBlueprintHelperReviewSurface Surface) const
{
	const FSurfacePresenterDescriptor* Descriptor = SurfacePresenters.Find(Surface);
	if (Descriptor == nullptr)
	{
		return nullptr;
	}
	return Descriptor->Predicate;
}

bool FBlueprintHelperReviewSurfacePresenterRegistry::CanBuildOverlay(
	EBlueprintHelperReviewSurface Surface) const
{
	const FSurfacePresenterDescriptor* Descriptor = SurfacePresenters.Find(Surface);
	return Descriptor != nullptr && Descriptor->OverlayBuilder;
}

TSharedRef<SWidget> FBlueprintHelperReviewSurfacePresenterRegistry::BuildOverlayOrNull(
	EBlueprintHelperReviewSurface Surface,
	const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args) const
{
	const FSurfacePresenterDescriptor* Descriptor = SurfacePresenters.Find(Surface);
	if (Descriptor == nullptr || !Descriptor->OverlayBuilder)
	{
		return SNullWidget::NullWidget;
	}
	return Descriptor->OverlayBuilder(Args);
}
