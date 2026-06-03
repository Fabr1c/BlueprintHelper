// BlueprintHelper Review Structure presenter.

#include "UI/Review/BlueprintHelperReviewStructurePresenter.h"

#include "Kismet2/StructureEditorUtils.h"
#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
#include "Shared/BlueprintHelperUserDefinedStructVersionCompat.h"
#else
#include "Shared/BlueprintHelperUserDefinedStructVersionCompat.h"
#endif
#include "UI/Review/BlueprintHelperReviewAssetContext.h"
#include "UI/Review/BlueprintHelperReviewPresenterWidgetUtils.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Views/SListView.h"

bool FBlueprintHelperReviewStructurePresenter::ShouldShowChange(
	const FBlueprintHelperReviewVisibleChange& Change)
{
	return FBlueprintHelperReviewPresenterWidgetUtils::ShouldShowIndependentSurfaceChange(
		Change,
		EBlueprintHelperReviewSurface::DataAsset,
		{TEXT("asset_factory"), TEXT("structure"), TEXT("struct_field"), TEXT("structure_field")});
}

TSharedRef<SWidget> FBlueprintHelperReviewStructurePresenter::BuildContent(
	const FBlueprintHelperReviewAssetContext& Context,
	FBlueprintHelperReviewDataAssetPresenterState& State,
	FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated)
{
	State.Rows.Reset();
	State.ListView.Reset();
	State.PropertyRowGenerator.Reset();

	UUserDefinedStruct* Structure = Context.Structure.Get();
	if (!Structure)
	{
		TArray<FString> Lines;
		Lines.Add(FString::Printf(TEXT("Asset: %s"), *Context.AssetPath));
		Lines.Add(FString::Printf(TEXT("Kind: %s"), BlueprintHelperReviewAssetKindToString(Context.AssetKind)));
		Lines.Add(TEXT("Structure: unavailable"));
		return FBlueprintHelperReviewPresenterWidgetUtils::BuildSummaryPanel(
			TEXT("Structure Summary"),
			Lines,
			FString(),
			EBlueprintHelperReviewSurface::Unknown,
			OnGeometryInvalidated);
	}

	const FString AssetName = FBlueprintHelperReviewPresenterWidgetUtils::GetAssetShortName(Context.AssetPath);
	TSharedRef<FBlueprintHelperReviewDataAssetRowItem> SummaryRow =
		MakeShared<FBlueprintHelperReviewDataAssetRowItem>();
	SummaryRow->Label = FString::Printf(TEXT("Structure: %s"), *AssetName);
	SummaryRow->Value = FString::Printf(TEXT("Fields: %d"), FStructureEditorUtils::GetVarDesc(Structure).Num());
	SummaryRow->SearchText = FString::Printf(
		TEXT("asset_factory:structure structure %s %s"),
		*AssetName,
		*Context.AssetPath);
	SummaryRow->bIsSection = true;
	State.Rows.Add(SummaryRow);

	for (const FStructVariableDescription& Variable : FStructureEditorUtils::GetVarDesc(Structure))
	{
		const FString FriendlyName = Variable.FriendlyName.IsEmpty()
			? Variable.VarName.ToString()
			: Variable.FriendlyName;
		TSharedRef<FBlueprintHelperReviewDataAssetRowItem> Row =
			MakeShared<FBlueprintHelperReviewDataAssetRowItem>();
		Row->Label = FriendlyName;
		Row->Value = Variable.ToPinType().PinCategory.ToString();
		Row->PinType = Variable.ToPinType();
		Row->bHasPinType = true;
		Row->SearchText = FString::Printf(
			TEXT("struct_field:%s structure_field:%s %s"),
			*FriendlyName,
			*FriendlyName,
			*Variable.VarName.ToString());
		Row->Depth = 1;
		State.Rows.Add(Row);
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
