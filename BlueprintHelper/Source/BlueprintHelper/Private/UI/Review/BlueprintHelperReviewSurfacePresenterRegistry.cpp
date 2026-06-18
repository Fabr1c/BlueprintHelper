// BlueprintHelper Review surface presenter registry implementation.

#include "UI/Review/BlueprintHelperReviewSurfacePresenterRegistry.h"

#include "UI/Review/BlueprintHelperReviewBlueprintComponentsPresenter.h"
#include "UI/Review/BlueprintHelperReviewDataAssetPresenter.h"
#include "UI/Review/BlueprintHelperReviewDataTablePresenter.h"
#include "UI/Review/BlueprintHelperReviewGraphPresenter.h"
#include "UI/Review/BlueprintHelperReviewMaterialPresenter.h"
#include "UI/Review/BlueprintHelperReviewMyBlueprintPresenter.h"
#include "UI/Review/Native/Components/SBlueprintHelperReviewComponentsPanel.h"
#include "UI/Review/BlueprintHelperReviewObjectDetailsPresenter.h"
#include "UI/Review/BlueprintHelperReviewWidgetTreePresenter.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STreeView.h"

static TSharedRef<SWidget> BlueprintHelperReviewSurfacePresenterRegistryBuildContentErrorWidget(
	EBlueprintHelperReviewSurface Surface)
{
	const FString Message = FString::Printf(
		TEXT("Missing Review surface content builder: %s"),
		BlueprintHelperReviewSurfaceToString(Surface));
	ensureMsgf(false, TEXT("%s"), *Message);
	return SNew(STextBlock)
		.Text(FText::FromString(Message));
}

static bool BlueprintHelperReviewSurfaceContentHasAssetContext(
	const FBlueprintHelperReviewPanelSurfaceContentArgs& Args,
	EBlueprintHelperReviewSurface Surface)
{
	if (Args.AssetContext != nullptr)
	{
		return true;
	}
	ensureMsgf(
		false,
		TEXT("Missing Review surface content asset context: %s"),
		BlueprintHelperReviewSurfaceToString(Surface));
	return false;
}

static void BlueprintHelperReviewSurfacePresenterRegistryInvalidateWidget(
	const TWeakPtr<SWidget>& Widget)
{
	if (const TSharedPtr<SWidget> PinnedWidget = Widget.Pin())
	{
		PinnedWidget->Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
	}
}

static void BlueprintHelperReviewSurfacePresenterRegistryInvalidateMyBlueprintRows(
	const TArray<TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>>& Items)
{
	for (const TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>& Item : Items)
	{
		if (!Item.IsValid())
		{
			continue;
		}
		BlueprintHelperReviewSurfacePresenterRegistryInvalidateWidget(Item->RowWidget);
		BlueprintHelperReviewSurfacePresenterRegistryInvalidateMyBlueprintRows(Item->Children);
	}
}

static void BlueprintHelperReviewSurfacePresenterRegistryInvalidateWidgetTreeRows(
	const TArray<TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>>& Items)
{
	for (const TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>& Item : Items)
	{
		if (!Item.IsValid())
		{
			continue;
		}
		BlueprintHelperReviewSurfacePresenterRegistryInvalidateWidget(Item->RowWidget);
		BlueprintHelperReviewSurfacePresenterRegistryInvalidateWidgetTreeRows(Item->Children);
	}
}

static void BlueprintHelperReviewSurfacePresenterRegistryInvalidateDataAssetRows(
	const TArray<TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>>& Items)
{
	for (const TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>& Item : Items)
	{
		if (Item.IsValid())
		{
			BlueprintHelperReviewSurfacePresenterRegistryInvalidateWidget(Item->RowWidget);
		}
	}
}

static bool BlueprintHelperReviewSurfacePresenterRegistryInvalidateHost(
	const FBlueprintHelperReviewSurfaceRowsRefreshArgs& Args,
	EBlueprintHelperReviewSurfaceHostSlot HostSlot)
{
	const TWeakPtr<SWidget>* HostWidget = Args.HostWidgets.Find(HostSlot);
	if (!HostWidget)
	{
		return false;
	}
	if (const TSharedPtr<SWidget> PinnedWidget = HostWidget->Pin())
	{
		PinnedWidget->Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
		return true;
	}
	return false;
}

static bool BlueprintHelperReviewSurfacePresenterRegistryRefreshComponentsRows(
	const FBlueprintHelperReviewSurfaceRowsRefreshArgs& Args)
{
	bool bHandled = false;
	if (Args.ComponentsState != nullptr && Args.ComponentsState->ComponentsPanel.IsValid())
	{
		Args.ComponentsState->ComponentsPanel->RequestRowsRefresh();
		bHandled = true;
	}
	return BlueprintHelperReviewSurfacePresenterRegistryInvalidateHost(
		Args,
		EBlueprintHelperReviewSurfaceHostSlot::Structure) || bHandled;
}

static bool BlueprintHelperReviewSurfacePresenterRegistryRefreshWidgetTreeRows(
	const FBlueprintHelperReviewSurfaceRowsRefreshArgs& Args)
{
	bool bHandled = false;
	if (Args.WidgetTreeState != nullptr)
	{
		BlueprintHelperReviewSurfacePresenterRegistryInvalidateWidgetTreeRows(Args.WidgetTreeState->RootItems);
		if (Args.WidgetTreeState->TreeView.IsValid())
		{
			Args.WidgetTreeState->TreeView->RequestTreeRefresh();
			bHandled = true;
		}
	}
	return BlueprintHelperReviewSurfacePresenterRegistryInvalidateHost(
		Args,
		EBlueprintHelperReviewSurfaceHostSlot::Structure) || bHandled;
}

static bool BlueprintHelperReviewSurfacePresenterRegistryRefreshMyBlueprintRows(
	const FBlueprintHelperReviewSurfaceRowsRefreshArgs& Args)
{
	bool bHandled = false;
	if (Args.MyBlueprintState != nullptr)
	{
		BlueprintHelperReviewSurfacePresenterRegistryInvalidateMyBlueprintRows(Args.MyBlueprintState->RootItems);
		if (Args.MyBlueprintState->TreeView.IsValid())
		{
			Args.MyBlueprintState->TreeView->RequestTreeRefresh();
			bHandled = true;
		}
	}
	return BlueprintHelperReviewSurfacePresenterRegistryInvalidateHost(
		Args,
		EBlueprintHelperReviewSurfaceHostSlot::MyBlueprint) || bHandled;
}

static bool BlueprintHelperReviewSurfacePresenterRegistryRefreshDetailsRows(
	const FBlueprintHelperReviewSurfaceRowsRefreshArgs& Args)
{
	bool bHandled = Args.RefreshDetailsRows ? Args.RefreshDetailsRows() : false;
	return BlueprintHelperReviewSurfacePresenterRegistryInvalidateHost(
		Args,
		EBlueprintHelperReviewSurfaceHostSlot::Details) || bHandled;
}

static bool BlueprintHelperReviewSurfacePresenterRegistryRefreshDataTableRows(
	const FBlueprintHelperReviewSurfaceRowsRefreshArgs& Args)
{
	bool bHandled = false;
	if (Args.DataTableState != nullptr)
	{
		BlueprintHelperReviewSurfacePresenterRegistryInvalidateDataAssetRows(Args.DataTableState->SelectedRowFields);
		if (Args.DataTableState->ListView.IsValid())
		{
			Args.DataTableState->ListView->RequestListRefresh();
			bHandled = true;
		}
		if (Args.DataTableState->SelectedRowFieldListView.IsValid())
		{
			Args.DataTableState->SelectedRowFieldListView->RequestListRefresh();
			bHandled = true;
		}
	}
	return BlueprintHelperReviewSurfacePresenterRegistryInvalidateHost(
		Args,
		EBlueprintHelperReviewSurfaceHostSlot::MainWorkspace) || bHandled;
}

static bool BlueprintHelperReviewSurfacePresenterRegistryRefreshDataAssetRows(
	const FBlueprintHelperReviewSurfaceRowsRefreshArgs& Args)
{
	bool bHandled = false;
	if (Args.DataAssetState != nullptr)
	{
		BlueprintHelperReviewSurfacePresenterRegistryInvalidateDataAssetRows(Args.DataAssetState->Rows);
		if (Args.DataAssetState->ListView.IsValid())
		{
			Args.DataAssetState->ListView->RequestListRefresh();
			bHandled = true;
		}
	}
	return BlueprintHelperReviewSurfacePresenterRegistryInvalidateHost(
		Args,
		EBlueprintHelperReviewSurfaceHostSlot::MainWorkspace) || bHandled;
}

static bool BlueprintHelperReviewSurfacePresenterRegistryRefreshMaterialRows(
	const FBlueprintHelperReviewSurfaceRowsRefreshArgs& Args)
{
	bool bHandled = false;
	if (Args.MaterialState != nullptr)
	{
		BlueprintHelperReviewSurfacePresenterRegistryInvalidateDataAssetRows(Args.MaterialState->Rows);
		if (Args.MaterialState->ListView.IsValid())
		{
			Args.MaterialState->ListView->RequestListRefresh();
			bHandled = true;
		}
	}
	return BlueprintHelperReviewSurfacePresenterRegistryInvalidateHost(
		Args,
		EBlueprintHelperReviewSurfaceHostSlot::MainWorkspace) || bHandled;
}

static TSharedRef<SWidget> BlueprintHelperReviewSurfacePresenterRegistryBuildGraphContent(
	const FBlueprintHelperReviewPanelSurfaceContentArgs& Args)
{
	if (!BlueprintHelperReviewSurfaceContentHasAssetContext(Args, EBlueprintHelperReviewSurface::Graph)
		|| Args.GraphState == nullptr)
	{
		return BlueprintHelperReviewSurfacePresenterRegistryBuildContentErrorWidget(EBlueprintHelperReviewSurface::Graph);
	}

	FBlueprintHelperReviewGraphPresenterArgs GraphArgs;
	GraphArgs.AssetContext = Args.AssetContext;
	GraphArgs.ChangeItems = Args.ChangeItems;
	GraphArgs.SurfaceDiffModels = Args.SurfaceDiffModels;
	GraphArgs.SelectedChange = Args.SelectedChange;
	const bool bSelectedMatchesGraphNavigation =
		Args.SelectedChange.IsValid()
		&& !Args.RequestedGraphNavigationChangeId.IsEmpty()
		&& Args.SelectedChange->ChangeId == Args.RequestedGraphNavigationChangeId;
	GraphArgs.RequestedGraphName = bSelectedMatchesGraphNavigation
		? Args.RequestedGraphNavigationGraphName
		: FString();
	GraphArgs.bAllowGraphNavigationWithoutGraphReview =
		bSelectedMatchesGraphNavigation && Args.bAllowGraphNavigationWithoutGraphReview;
	GraphArgs.AddDebugMessage = Args.AddDebugMessage;
	GraphArgs.OnReviewActionIntent = Args.OnReviewActionIntent;
	GraphArgs.GetChangeColor = Args.GetChangeColor;
	GraphArgs.ReviewPanelSettings = Args.ReviewPanelSettings;
	return FBlueprintHelperReviewGraphPresenter::BuildContent(GraphArgs, *Args.GraphState);
}

static TSharedRef<SWidget> BlueprintHelperReviewSurfacePresenterRegistryBuildComponentsContent(
	const FBlueprintHelperReviewPanelSurfaceContentArgs& Args)
{
	if (!BlueprintHelperReviewSurfaceContentHasAssetContext(Args, EBlueprintHelperReviewSurface::Components)
		|| Args.ComponentsState == nullptr)
	{
		return BlueprintHelperReviewSurfacePresenterRegistryBuildContentErrorWidget(EBlueprintHelperReviewSurface::Components);
	}
	return FBlueprintHelperReviewBlueprintComponentsPresenter::BuildContent(
		*Args.AssetContext,
		*Args.ComponentsState,
		Args.OnGeometryInvalidated);
}

static TSharedRef<SWidget> BlueprintHelperReviewSurfacePresenterRegistryBuildWidgetTreeContent(
	const FBlueprintHelperReviewPanelSurfaceContentArgs& Args)
{
	if (!BlueprintHelperReviewSurfaceContentHasAssetContext(Args, EBlueprintHelperReviewSurface::UMGWidgetTree)
		|| Args.WidgetTreeState == nullptr)
	{
		return BlueprintHelperReviewSurfacePresenterRegistryBuildContentErrorWidget(EBlueprintHelperReviewSurface::UMGWidgetTree);
	}
	return FBlueprintHelperReviewUMGWidgetTreePresenter::BuildContent(
		*Args.AssetContext,
		*Args.WidgetTreeState,
		Args.OnGeometryInvalidated);
}

static TSharedRef<SWidget> BlueprintHelperReviewSurfacePresenterRegistryBuildDataTableContent(
	const FBlueprintHelperReviewPanelSurfaceContentArgs& Args)
{
	if (!BlueprintHelperReviewSurfaceContentHasAssetContext(Args, EBlueprintHelperReviewSurface::DataTable)
		|| Args.DataTableState == nullptr)
	{
		return BlueprintHelperReviewSurfacePresenterRegistryBuildContentErrorWidget(EBlueprintHelperReviewSurface::DataTable);
	}
	if (Args.Host == EBlueprintHelperReviewSurfaceContentHost::MainWorkspace && Args.GraphState != nullptr)
	{
		Args.GraphState->Reset();
	}
	return FBlueprintHelperReviewDataTablePresenter::BuildContent(
		*Args.AssetContext,
		*Args.DataTableState,
		Args.OnGeometryInvalidated);
}

static TSharedRef<SWidget> BlueprintHelperReviewSurfacePresenterRegistryBuildDataAssetContent(
	const FBlueprintHelperReviewPanelSurfaceContentArgs& Args)
{
	if (!BlueprintHelperReviewSurfaceContentHasAssetContext(Args, EBlueprintHelperReviewSurface::DataAsset)
		|| Args.DataAssetState == nullptr)
	{
		return BlueprintHelperReviewSurfacePresenterRegistryBuildContentErrorWidget(EBlueprintHelperReviewSurface::DataAsset);
	}
	if (Args.Host == EBlueprintHelperReviewSurfaceContentHost::MainWorkspace && Args.GraphState != nullptr)
	{
		Args.GraphState->Reset();
	}
	return FBlueprintHelperReviewDataAssetPresenter::BuildContent(
		*Args.AssetContext,
		*Args.DataAssetState,
		Args.OnGeometryInvalidated);
}

static TSharedRef<SWidget> BlueprintHelperReviewSurfacePresenterRegistryBuildMaterialContent(
	const FBlueprintHelperReviewPanelSurfaceContentArgs& Args)
{
	if (!BlueprintHelperReviewSurfaceContentHasAssetContext(Args, EBlueprintHelperReviewSurface::Material)
		|| Args.MaterialState == nullptr)
	{
		return BlueprintHelperReviewSurfacePresenterRegistryBuildContentErrorWidget(EBlueprintHelperReviewSurface::Material);
	}
	if (Args.Host == EBlueprintHelperReviewSurfaceContentHost::MainWorkspace && Args.GraphState != nullptr)
	{
		Args.GraphState->Reset();
	}
	return FBlueprintHelperReviewMaterialPresenter::BuildContent(
		*Args.AssetContext,
		*Args.MaterialState,
		Args.OnGeometryInvalidated);
}

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

	Registry->RegisterContentBuilder(
		EBlueprintHelperReviewSurface::Graph,
		&BlueprintHelperReviewSurfacePresenterRegistryBuildGraphContent);
	Registry->RegisterContentBuilder(
		EBlueprintHelperReviewSurface::Components,
		&BlueprintHelperReviewSurfacePresenterRegistryBuildComponentsContent);
	Registry->RegisterContentBuilder(
		EBlueprintHelperReviewSurface::UMGWidgetTree,
		&BlueprintHelperReviewSurfacePresenterRegistryBuildWidgetTreeContent);
	Registry->RegisterContentBuilder(
		EBlueprintHelperReviewSurface::DataTable,
		&BlueprintHelperReviewSurfacePresenterRegistryBuildDataTableContent);
	Registry->RegisterContentBuilder(
		EBlueprintHelperReviewSurface::DataAsset,
		&BlueprintHelperReviewSurfacePresenterRegistryBuildDataAssetContent);
	Registry->RegisterContentBuilder(
		EBlueprintHelperReviewSurface::Material,
		&BlueprintHelperReviewSurfacePresenterRegistryBuildMaterialContent);

	Registry->RegisterHostBinding({ EBlueprintHelperReviewSurface::Components, EBlueprintHelperReviewSurfaceHostSlot::Structure, true, true });
	Registry->RegisterHostBinding({ EBlueprintHelperReviewSurface::UMGWidgetTree, EBlueprintHelperReviewSurfaceHostSlot::Structure, true, true });
	Registry->RegisterHostBinding({ EBlueprintHelperReviewSurface::MyBlueprint, EBlueprintHelperReviewSurfaceHostSlot::MyBlueprint, true, true });
	Registry->RegisterHostBinding({ EBlueprintHelperReviewSurface::Details, EBlueprintHelperReviewSurfaceHostSlot::Details, true, true });
	Registry->RegisterHostBinding({ EBlueprintHelperReviewSurface::DataTable, EBlueprintHelperReviewSurfaceHostSlot::MainWorkspace, true, true });
	Registry->RegisterHostBinding({ EBlueprintHelperReviewSurface::DataAsset, EBlueprintHelperReviewSurfaceHostSlot::MainWorkspace, true, true });
	Registry->RegisterHostBinding({ EBlueprintHelperReviewSurface::Material, EBlueprintHelperReviewSurfaceHostSlot::MainWorkspace, true, true });

	Registry->RegisterRowsRefreshBuilder(
		EBlueprintHelperReviewSurface::Components,
		&BlueprintHelperReviewSurfacePresenterRegistryRefreshComponentsRows);
	Registry->RegisterRowsRefreshBuilder(
		EBlueprintHelperReviewSurface::UMGWidgetTree,
		&BlueprintHelperReviewSurfacePresenterRegistryRefreshWidgetTreeRows);
	Registry->RegisterRowsRefreshBuilder(
		EBlueprintHelperReviewSurface::MyBlueprint,
		&BlueprintHelperReviewSurfacePresenterRegistryRefreshMyBlueprintRows);
	Registry->RegisterRowsRefreshBuilder(
		EBlueprintHelperReviewSurface::Details,
		&BlueprintHelperReviewSurfacePresenterRegistryRefreshDetailsRows);
	Registry->RegisterRowsRefreshBuilder(
		EBlueprintHelperReviewSurface::DataTable,
		&BlueprintHelperReviewSurfacePresenterRegistryRefreshDataTableRows);
	Registry->RegisterRowsRefreshBuilder(
		EBlueprintHelperReviewSurface::DataAsset,
		&BlueprintHelperReviewSurfacePresenterRegistryRefreshDataAssetRows);
	Registry->RegisterRowsRefreshBuilder(
		EBlueprintHelperReviewSurface::Material,
		&BlueprintHelperReviewSurfacePresenterRegistryRefreshMaterialRows);

	Registry->RegisterDetailsObjectPolicy(EBlueprintHelperReviewSurface::UMGWidgetTree, false);
	Registry->RegisterDetailsObjectPolicy(EBlueprintHelperReviewSurface::DataTable, false);
	Registry->RegisterDetailsObjectPolicy(EBlueprintHelperReviewSurface::DataAsset, false);

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

void FBlueprintHelperReviewSurfacePresenterRegistry::RegisterContentBuilder(
	EBlueprintHelperReviewSurface Surface,
	FBlueprintHelperReviewSurfaceContentBuilder ContentBuilder)
{
	FSurfacePresenterDescriptor& Descriptor = SurfacePresenters.FindOrAdd(Surface);
	Descriptor.ContentBuilder = MoveTemp(ContentBuilder);
}

void FBlueprintHelperReviewSurfacePresenterRegistry::RegisterHostBinding(
	const FBlueprintHelperReviewSurfaceHostBinding& HostBinding)
{
	FSurfacePresenterDescriptor& Descriptor = SurfacePresenters.FindOrAdd(HostBinding.Surface);
	Descriptor.HostBinding = HostBinding;
	Descriptor.bHasHostBinding = true;
}

void FBlueprintHelperReviewSurfacePresenterRegistry::RegisterRowsRefreshBuilder(
	EBlueprintHelperReviewSurface Surface,
	FBlueprintHelperReviewSurfaceRowsRefreshBuilder RowsRefreshBuilder)
{
	FSurfacePresenterDescriptor& Descriptor = SurfacePresenters.FindOrAdd(Surface);
	Descriptor.RowsRefreshBuilder = MoveTemp(RowsRefreshBuilder);
}

void FBlueprintHelperReviewSurfacePresenterRegistry::RegisterDetailsObjectPolicy(
	EBlueprintHelperReviewSurface Surface,
	bool bUsesDetailsObject)
{
	FSurfacePresenterDescriptor& Descriptor = SurfacePresenters.FindOrAdd(Surface);
	Descriptor.bUsesDetailsObject = bUsesDetailsObject;
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

const FBlueprintHelperReviewSurfaceHostBinding* FBlueprintHelperReviewSurfacePresenterRegistry::FindHostBinding(
	EBlueprintHelperReviewSurface Surface) const
{
	const FSurfacePresenterDescriptor* Descriptor = SurfacePresenters.Find(Surface);
	if (Descriptor == nullptr || !Descriptor->bHasHostBinding)
	{
		return nullptr;
	}
	return &Descriptor->HostBinding;
}

TArray<FBlueprintHelperReviewSurfaceHostBinding> FBlueprintHelperReviewSurfacePresenterRegistry::ListHostBindings() const
{
	TArray<FBlueprintHelperReviewSurfaceHostBinding> HostBindings;
	for (const TPair<EBlueprintHelperReviewSurface, FSurfacePresenterDescriptor>& Entry : SurfacePresenters)
	{
		if (Entry.Value.bHasHostBinding)
		{
			HostBindings.Add(Entry.Value.HostBinding);
		}
	}
	return HostBindings;
}

TMap<EBlueprintHelperReviewSurface, TFunction<bool()>> FBlueprintHelperReviewSurfacePresenterRegistry::BuildRowsRefreshHandlers(
	const FBlueprintHelperReviewSurfaceRowsRefreshArgs& Args) const
{
	TMap<EBlueprintHelperReviewSurface, TFunction<bool()>> Handlers;
	for (const TPair<EBlueprintHelperReviewSurface, FSurfacePresenterDescriptor>& Entry : SurfacePresenters)
	{
		if (!Entry.Value.RowsRefreshBuilder)
		{
			continue;
		}

		FBlueprintHelperReviewSurfaceRowsRefreshBuilder Builder = Entry.Value.RowsRefreshBuilder;
		Handlers.Add(Entry.Key, [Builder, Args]()
		{
			return Builder(Args);
		});
	}
	return Handlers;
}

bool FBlueprintHelperReviewSurfacePresenterRegistry::UsesDetailsObject(EBlueprintHelperReviewSurface Surface) const
{
	const FSurfacePresenterDescriptor* Descriptor = SurfacePresenters.Find(Surface);
	return Descriptor == nullptr || Descriptor->bUsesDetailsObject;
}

bool FBlueprintHelperReviewSurfacePresenterRegistry::CanBuildOverlay(
	EBlueprintHelperReviewSurface Surface) const
{
	const FSurfacePresenterDescriptor* Descriptor = SurfacePresenters.Find(Surface);
	return Descriptor != nullptr && Descriptor->OverlayBuilder;
}

bool FBlueprintHelperReviewSurfacePresenterRegistry::CanBuildContent(
	EBlueprintHelperReviewSurface Surface) const
{
	const FSurfacePresenterDescriptor* Descriptor = SurfacePresenters.Find(Surface);
	return Descriptor != nullptr && Descriptor->ContentBuilder;
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

TSharedRef<SWidget> FBlueprintHelperReviewSurfacePresenterRegistry::BuildContentOrError(
	EBlueprintHelperReviewSurface Surface,
	const FBlueprintHelperReviewPanelSurfaceContentArgs& Args) const
{
	const FSurfacePresenterDescriptor* Descriptor = SurfacePresenters.Find(Surface);
	if (Descriptor == nullptr || !Descriptor->ContentBuilder)
	{
		return BlueprintHelperReviewSurfacePresenterRegistryBuildContentErrorWidget(Surface);
	}
	return Descriptor->ContentBuilder(Args);
}
