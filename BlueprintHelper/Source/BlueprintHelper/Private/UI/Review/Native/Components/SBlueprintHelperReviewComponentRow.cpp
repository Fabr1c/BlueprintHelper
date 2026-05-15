// BlueprintHelper Review native Components row.

#include "UI/Review/Native/Components/SBlueprintHelperReviewComponentRow.h"

#include "Styling/AppStyle.h"
#include "Styling/SlateIconFinder.h"
#include "UI/Review/BlueprintHelperReviewRowHighlightModel.h"
#include "UI/Review/BlueprintHelperReviewSlateRowGeometryRegistry.h"
#include "UI/Review/SBlueprintHelperReviewGeometryProbe.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"

void SBlueprintHelperReviewComponentRow::Construct(
	const FArguments& InArgs,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	Item = InArgs._Item;
	AssetPath = InArgs._AssetPath;
	const FBlueprintHelperReviewGeometryInvalidated OnGeometryInvalidated = InArgs._OnGeometryInvalidated;
	const FString SearchText = Item.IsValid() ? Item->ComponentName : FString();
	const FText DisplayName = FText::FromString(Item.IsValid() ? Item->DisplayName : FString());
	const FText ComponentClass = FText::FromString(Item.IsValid() ? Item->ComponentClass : FString());

	TSharedRef<SWidget> InnerRow = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SImage)
			.Image(this, &SBlueprintHelperReviewComponentRow::GetComponentIconBrush)
			.ColorAndOpacity(FSlateColor::UseForeground())
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(6.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Font(FAppStyle::GetFontStyle(TEXT("NormalFont")))
			.Text(DisplayName)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(8.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Text(ComponentClass)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SSpacer)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4.0f, 0.0f, 0.0f, 0.0f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		[
			SNew(SHorizontalBox)
			.Visibility(this, &SBlueprintHelperReviewComponentRow::GetActionVisibility)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Accept")))
				.OnClicked(this, &SBlueprintHelperReviewComponentRow::OnAcceptClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Reject")))
				.OnClicked(this, &SBlueprintHelperReviewComponentRow::OnRejectClicked)
			]
		];

	TSharedRef<SWidget> RowBody = SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.Visibility(this, &SBlueprintHelperReviewComponentRow::GetDiffVisibility)
			.BorderImage(this, &SBlueprintHelperReviewComponentRow::GetDiffBrush)
			.Padding(0.0f)
		]
		+ SOverlay::Slot()
		[
			InnerRow
		];
	RowBody = SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("NoBorder")))
		.Padding(FMargin(0.0f, 5.0f, 0.0f, 5.0f))
		.ToolTipText(Item.IsValid() ? Item->ToolTipText : FText::GetEmpty())
		[
			SNew(SBox)
			.MinDesiredHeight(25.0f)
			[
				RowBody
			]
		];

	TSharedRef<SWidget> RowWidget = SNew(SBlueprintHelperReviewGeometryProbe)
		.Surface(EBlueprintHelperReviewSurface::Components)
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
			EBlueprintHelperReviewSurface::Components,
			Item->ComponentName,
			RowWidget);
		if (!Item->DisplayName.IsEmpty())
		{
			FBlueprintHelperReviewSlateRowGeometryRegistry::RegisterRow(
				AssetPath,
				EBlueprintHelperReviewSurface::Components,
				Item->DisplayName,
				RowWidget);
		}
	}

	STableRow<TSharedPtr<FBlueprintHelperReviewComponentRowItem>>::Construct(
		STableRow<TSharedPtr<FBlueprintHelperReviewComponentRowItem>>::FArguments()
		.Style(FAppStyle::Get(), TEXT("SceneOutliner.TableViewRow"))
		.Padding(0.0f)
		.ShowSelection(true)
		[
			RowWidget
		],
		OwnerTable);
}

FSlateColor SBlueprintHelperReviewComponentRow::GetRowBackgroundColor() const
{
	if (!Item.IsValid())
	{
		return FSlateColor(FLinearColor::Transparent);
	}
	return FBlueprintHelperReviewRowHighlightModel::GetRowBackgroundColor(
		AssetPath,
		EBlueprintHelperReviewSurface::Components,
		Item->ComponentName);
}

EVisibility SBlueprintHelperReviewComponentRow::GetDiffVisibility() const
{
	return GetRowBackgroundColor().GetSpecifiedColor().A > 0.0f
		? EVisibility::HitTestInvisible
		: EVisibility::Collapsed;
}

const FSlateBrush* SBlueprintHelperReviewComponentRow::GetDiffBrush() const
{
	const FLinearColor DiffColor = GetRowBackgroundColor().GetSpecifiedColor();
	DiffBrush = FSlateRoundedBoxBrush(DiffColor, 0.0f);
	return &DiffBrush;
}

const FSlateBrush* SBlueprintHelperReviewComponentRow::GetComponentIconBrush() const
{
	if (Item.IsValid() && Item->ComponentClassObject.IsValid())
	{
		return FSlateIconFinder::FindIconBrushForClass(Item->ComponentClassObject.Get());
	}
	return FAppStyle::GetBrush(TEXT("Kismet.AllClasses.FunctionIcon"));
}

EVisibility SBlueprintHelperReviewComponentRow::GetActionVisibility() const
{
	if (!Item.IsValid() || Item->ComponentName.IsEmpty())
	{
		return EVisibility::Collapsed;
	}

	return IsHovered() && GetRowBackgroundColor().GetSpecifiedColor().A > 0.0f
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

FReply SBlueprintHelperReviewComponentRow::OnAcceptClicked() const
{
	if (Item.IsValid())
	{
		return FBlueprintHelperReviewRowHighlightModel::AcceptHighlightedRow(
			AssetPath,
			EBlueprintHelperReviewSurface::Components,
			Item->ComponentName);
	}
	return FReply::Handled();
}

FReply SBlueprintHelperReviewComponentRow::OnRejectClicked() const
{
	if (Item.IsValid())
	{
		return FBlueprintHelperReviewRowHighlightModel::RejectHighlightedRow(
			AssetPath,
			EBlueprintHelperReviewSurface::Components,
			Item->ComponentName);
	}
	return FReply::Handled();
}
