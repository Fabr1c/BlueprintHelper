// BlueprintHelper Review native My Blueprint row.

#include "UI/Review/Native/MyBlueprint/SBlueprintHelperReviewMyBlueprintRow.h"

#include "Styling/AppStyle.h"
#include "UI/Review/BlueprintHelperReviewRowHighlightModel.h"
#include "UI/Review/BlueprintHelperReviewSlateRowGeometryRegistry.h"
#include "UI/Review/SBlueprintHelperReviewGeometryProbe.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "SPinTypeSelector.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"

void SBlueprintHelperReviewMyBlueprintRow::Construct(
	const FArguments& InArgs,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	Item = InArgs._Item;
	AssetPath = InArgs._AssetPath;
	const FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated = InArgs._OnGeometryInvalidated;

	const FText Label = Item.IsValid() ? Item->Label : FText::GetEmpty();
	const FString SearchText = Item.IsValid() ? Item->SearchText : FString();
	const bool bSection = Item.IsValid()
		&& Item->Kind == FBlueprintHelperReviewMyBlueprintPresenter::ERowKind::Section;
	const bool bHasVariableType = Item.IsValid() && Item->bHasPinType;
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	const FText VariableTypeText = bHasVariableType
		? UEdGraphSchema_K2::TypeToText(Item->PinType)
		: FText::GetEmpty();
	TSharedRef<SWidget> VariableTypeIcon = SNew(SSpacer);
	if (bHasVariableType && K2Schema)
	{
		const FSlateBrush* PrimaryIcon = FBlueprintEditorUtils::GetIconFromPin(Item->PinType);
		const FSlateColor PrimaryColor = K2Schema->GetPinTypeColor(Item->PinType);
		const FSlateBrush* SecondaryIcon = FBlueprintEditorUtils::GetSecondaryIconFromPin(Item->PinType);
		const FSlateColor SecondaryColor = K2Schema->GetSecondaryPinTypeColor(Item->PinType);
		VariableTypeIcon = SPinTypeSelector::ConstructPinTypeImage(
			PrimaryIcon,
			PrimaryColor,
			SecondaryIcon,
			SecondaryColor,
			TSharedPtr<SToolTip>());
	}

	TSharedRef<SHorizontalBox> InnerRow = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		.Padding(4.0f, 0.0f, 2.0f, 0.0f)
		[
			SNew(STextBlock)
			.Font(bSection
				? FAppStyle::GetFontStyle(TEXT("BoldFont"))
				: FAppStyle::GetFontStyle(TEXT("NormalFont")))
			.Text(Label)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(6.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			.Visibility(this, &SBlueprintHelperReviewMyBlueprintRow::GetVariableTypeVisibility)
			.ToolTipText(VariableTypeText)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				VariableTypeIcon
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(3.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Text(VariableTypeText)
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4.0f, 0.0f, 0.0f, 0.0f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		[
			SNew(SHorizontalBox)
			.Visibility(this, &SBlueprintHelperReviewMyBlueprintRow::GetActionVisibility)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Accept")))
				.OnClicked(this, &SBlueprintHelperReviewMyBlueprintRow::OnAcceptClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Reject")))
				.OnClicked(this, &SBlueprintHelperReviewMyBlueprintRow::OnRejectClicked)
			]
		];

	TSharedRef<SWidget> RowBody = bSection
		? StaticCastSharedRef<SWidget>(
			SNew(SBorder)
			.BorderImage_Lambda([this]()
			{
				return IsHovered()
					? FAppStyle::Get().GetBrush(TEXT("Brushes.Secondary"))
					: FAppStyle::Get().GetBrush(TEXT("Brushes.Header"));
			})
			.Padding(FMargin(3.0f, 0.0f))
			[
				InnerRow
			])
		: StaticCastSharedRef<SWidget>(
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.Visibility(this, &SBlueprintHelperReviewMyBlueprintRow::GetDiffVisibility)
				.BorderImage(this, &SBlueprintHelperReviewMyBlueprintRow::GetDiffBrush)
				.Padding(0.0f)
			]
			+ SOverlay::Slot()
			[
				InnerRow
			]);
	RowBody = SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("NoBorder")))
		.Padding(FMargin(0.0f, 5.0f, 0.0f, 5.0f))
		[
			SNew(SBox)
			.MinDesiredHeight(25.0f)
			[
				RowBody
			]
		];

	TSharedRef<SWidget> RowWidget = SNew(SBlueprintHelperReviewGeometryProbe)
		.Surface(EBlueprintHelperReviewSurface::MyBlueprint)
		.TargetKey(SearchText)
		.OnGeometryInvalidated(OnGeometryInvalidated)
		[
			RowBody
		];

	if (Item.IsValid())
	{
		Item->RowWidget = RowWidget;
		FBlueprintHelperReviewSlateRowGeometryRegistry::RegisterRow(
			AssetPath,
			EBlueprintHelperReviewSurface::MyBlueprint,
			SearchText,
			RowWidget);
	}

	STableRow<TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>>::Construct(
		STableRow<TSharedPtr<FBlueprintHelperReviewMyBlueprintPresenter::FRowItem>>::FArguments()
		.Style(FAppStyle::Get(), bSection ? TEXT("DetailsView.TreeView.TableRow") : TEXT("TableView.Row"))
		.Padding(0.0f)
		.ShowSelection(!bSection)
		[
			RowWidget
		],
		OwnerTable);
}

FSlateColor SBlueprintHelperReviewMyBlueprintRow::GetRowBackgroundColor() const
{
	if (!Item.IsValid() || Item->Kind == FBlueprintHelperReviewMyBlueprintPresenter::ERowKind::Section)
	{
		return FSlateColor(FLinearColor(0.035f, 0.035f, 0.035f, 1.0f));
	}
	return FBlueprintHelperReviewRowHighlightModel::GetRowBackgroundColor(
		AssetPath,
		EBlueprintHelperReviewSurface::MyBlueprint,
		Item->SearchText);
}

EVisibility SBlueprintHelperReviewMyBlueprintRow::GetDiffVisibility() const
{
	return GetRowBackgroundColor().GetSpecifiedColor().A > 0.0f
		? EVisibility::HitTestInvisible
		: EVisibility::Collapsed;
}

const FSlateBrush* SBlueprintHelperReviewMyBlueprintRow::GetDiffBrush() const
{
	const FLinearColor DiffColor = GetRowBackgroundColor().GetSpecifiedColor();
	DiffBrush = FSlateRoundedBoxBrush(DiffColor, 0.0f);
	return &DiffBrush;
}

EVisibility SBlueprintHelperReviewMyBlueprintRow::GetActionVisibility() const
{
	if (!Item.IsValid()
		|| Item->Kind == FBlueprintHelperReviewMyBlueprintPresenter::ERowKind::Section
		|| Item->SearchText.IsEmpty())
	{
		return EVisibility::Collapsed;
	}

	return IsHovered() && GetRowBackgroundColor().GetSpecifiedColor().A > 0.0f
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

EVisibility SBlueprintHelperReviewMyBlueprintRow::GetVariableTypeVisibility() const
{
	return Item.IsValid() && Item->bHasPinType
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

FReply SBlueprintHelperReviewMyBlueprintRow::OnAcceptClicked() const
{
	if (Item.IsValid())
	{
		return FBlueprintHelperReviewRowHighlightModel::AcceptHighlightedRow(
			AssetPath,
			EBlueprintHelperReviewSurface::MyBlueprint,
			Item->SearchText);
	}
	return FReply::Handled();
}

FReply SBlueprintHelperReviewMyBlueprintRow::OnRejectClicked() const
{
	if (Item.IsValid())
	{
		return FBlueprintHelperReviewRowHighlightModel::RejectHighlightedRow(
			AssetPath,
			EBlueprintHelperReviewSurface::MyBlueprint,
			Item->SearchText);
	}
	return FReply::Handled();
}
