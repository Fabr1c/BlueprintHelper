// BlueprintHelper Metrics detail chart widget.

#include "UI/Metrics/SBlueprintHelperMetricsDetailChart.h"

#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SBlueprintHelperMetricsDetailChart::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBorder)
		.Padding(8.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(TitleTextWidget, STextBlock)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(SubtitleTextWidget, STextBlock)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(TotalTextWidget, STextBlock)
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SAssignNew(RowsBox, SVerticalBox)
				]
			]
		]
	];

	SetData(
		InArgs._Title,
		InArgs._Subtitle,
		InArgs._TotalText,
		InArgs._Rows);
}

void SBlueprintHelperMetricsDetailChart::SetData(
	const FString& InTitle,
	const FString& InSubtitle,
	const FString& InTotalText,
	const TArray<FBlueprintHelperMetricsDetailBarView>& InRows)
{
	Title = InTitle;
	Subtitle = InSubtitle;
	TotalText = InTotalText;
	Rows = InRows;

	if (TitleTextWidget.IsValid())
	{
		TitleTextWidget->SetText(FText::FromString(FString::Printf(TEXT("C - %s"), *Title)));
	}
	if (SubtitleTextWidget.IsValid())
	{
		SubtitleTextWidget->SetText(FText::FromString(Subtitle));
	}
	if (TotalTextWidget.IsValid())
	{
		TotalTextWidget->SetText(FText::FromString(TotalText));
	}

	RefreshRows();
}

void SBlueprintHelperMetricsDetailChart::RefreshRows()
{
	if (!RowsBox.IsValid())
	{
		return;
	}

	RowsBox->ClearChildren();
	if (Rows.Num() == 0)
	{
		RowsBox->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("No Metrics data for selected bucket.")))
		];
		return;
	}

	for (const FBlueprintHelperMetricsDetailBarView& Row : Rows)
	{
		RowsBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			BuildRow(Row)
		];
	}
}

TSharedRef<SWidget> SBlueprintHelperMetricsDetailChart::BuildRow(
	const FBlueprintHelperMetricsDetailBarView& Row) const
{
	const FString ValueText = FString::Printf(
		TEXT("%lld %s"),
		Row.Value,
		*Row.UnitLabel);

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
		.Padding(FMargin(8.0f, 8.0f, 10.0f, 8.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.34f)
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 10.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Row.Label))
				.AutoWrapText(true)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.30f)
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 12.0f, 0.0f)
			[
				SNew(SBox)
				.MinDesiredWidth(160.0f)
				.MaxDesiredWidth(260.0f)
				.HeightOverride(8.0f)
				[
					SNew(SProgressBar)
					.BorderPadding(FVector2D(0.0f, 0.0f))
					.Percent(FMath::Clamp(Row.Fraction, 0.0f, 1.0f))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 12.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(ValueText))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.36f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Row.SubText))
				.AutoWrapText(true)
			]
		];
}
