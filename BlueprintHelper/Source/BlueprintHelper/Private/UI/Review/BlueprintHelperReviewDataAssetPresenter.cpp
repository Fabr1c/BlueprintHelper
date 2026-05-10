// BlueprintHelper Review DataAsset presenter.

#include "UI/Review/BlueprintHelperReviewDataAssetPresenter.h"

#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "UI/Review/BlueprintHelperReviewAssetContext.h"
#include "UI/Review/BlueprintHelperReviewPresenterWidgetUtils.h"
#include "UI/Review/BlueprintHelperReviewRowHighlightModel.h"
#include "UI/Review/BlueprintHelperReviewStructurePresenter.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Views/SListView.h"

bool FBlueprintHelperReviewDataAssetPresenter::ShouldShowChange(
	const FBlueprintHelperReviewVisibleChange& Change)
{
	const bool bObjectDetailsChange =
		FBlueprintHelperReviewPresenterWidgetUtils::ShouldShowIndependentSurfaceChange(
			Change,
			EBlueprintHelperReviewSurface::DataAsset,
			{TEXT("object_property"), TEXT("data_asset_property"), TEXT("asset_factory")});
	return bObjectDetailsChange || FBlueprintHelperReviewStructurePresenter::ShouldShowChange(Change);
}

TSharedRef<SWidget> FBlueprintHelperReviewDataAssetPresenter::BuildContent(
	const FBlueprintHelperReviewAssetContext& Context,
	FBlueprintHelperReviewDataAssetPresenterState& State,
	FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated)
{
	if (Context.Structure.IsValid())
	{
		return FBlueprintHelperReviewStructurePresenter::BuildContent(
			Context,
			State,
			OnGeometryInvalidated);
	}

	State.Rows.Reset();
	State.ListView.Reset();
	State.PropertyRowGenerator.Reset();

	const FString AssetName = FBlueprintHelperReviewPresenterWidgetUtils::GetAssetShortName(Context.AssetPath);
	if (UObject* AssetObject = Context.AssetObject.Get())
	{
		TSharedRef<FBlueprintHelperReviewDataAssetRowItem> SummaryRow =
			MakeShared<FBlueprintHelperReviewDataAssetRowItem>();
		SummaryRow->Label = FString::Printf(TEXT("Object: %s"), *AssetName);
		SummaryRow->Value = AssetObject->GetClass() ? AssetObject->GetClass()->GetName() : FString(TEXT("<unknown>"));
		SummaryRow->SearchText = FString::Printf(
			TEXT("asset_factory:data_asset data_asset object_property %s %s"),
			*AssetName,
			*Context.AssetPath);
		SummaryRow->bIsSection = true;
		State.Rows.Add(SummaryRow);

		FPropertyEditorModule& PropertyEditorModule =
			FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		FPropertyRowGeneratorArgs RowGeneratorArgs;
		RowGeneratorArgs.bShouldShowHiddenProperties = false;
		RowGeneratorArgs.bAllowEditingClassDefaultObjects = false;
		State.PropertyRowGenerator = PropertyEditorModule.CreatePropertyRowGenerator(RowGeneratorArgs);
		State.PropertyRowGenerator->SetObjects({ AssetObject });

		FBlueprintHelperReviewPresenterWidgetUtils::FlattenDetailTreeNodes(
			State.PropertyRowGenerator->GetRootTreeNodes(),
			0,
			State.Rows);
	}

	if (State.Rows.Num() == 0)
	{
		TArray<FString> Lines;
		Lines.Add(FString::Printf(TEXT("Asset: %s"), *Context.AssetPath));
		Lines.Add(FString::Printf(TEXT("Kind: %s"), BlueprintHelperReviewAssetKindToString(Context.AssetKind)));
		Lines.Add(TEXT("Object rows: unavailable"));
		return FBlueprintHelperReviewPresenterWidgetUtils::BuildSummaryPanel(
			TEXT("Object Details"),
			Lines,
			FString(),
			EBlueprintHelperReviewSurface::Unknown,
			OnGeometryInvalidated);
	}

	const FString AssetPath = Context.AssetPath;
	TSharedRef<SListView<TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>>> ListView =
		SAssignNew(State.ListView, SListView<TSharedPtr<FBlueprintHelperReviewDataAssetRowItem>>)
		.ListItemsSource(&State.Rows)
		.SelectionMode(ESelectionMode::None)
		.OnGenerateRow_Lambda([AssetPath, OnGeometryInvalidated](
			TSharedPtr<FBlueprintHelperReviewDataAssetRowItem> Item,
			const TSharedRef<STableViewBase>& OwnerTable) -> TSharedRef<ITableRow>
		{
			return FBlueprintHelperReviewPresenterWidgetUtils::GenerateDataAssetRow(
				Item,
				OwnerTable,
				AssetPath,
				OnGeometryInvalidated);
		});

	return SNew(SBorder)
		.Padding(8.0f)
		[
			ListView
		];
}

TSharedRef<SWidget> FBlueprintHelperReviewDataAssetPresenter::BuildOverlay(
	const FBlueprintHelperReviewPanelSurfacePresenterArgs& Args)
{
	return FBlueprintHelperReviewRowHighlightModel::BuildRowHighlightOverlay(
		Args,
		EBlueprintHelperReviewSurface::DataAsset,
		&FBlueprintHelperReviewDataAssetPresenter::ShouldShowChange);
}

