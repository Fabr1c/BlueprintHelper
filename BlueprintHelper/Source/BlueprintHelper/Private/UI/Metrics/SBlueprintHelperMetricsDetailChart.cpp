// BlueprintHelper Metrics detail chart widget.

#include "UI/Metrics/SBlueprintHelperMetricsDetailChart.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SBlueprintHelperMetricsDetailChart::Construct(const FArguments& InArgs)
{
	Title = InArgs._Title;
	Subtitle = InArgs._Subtitle;
	TotalText = InArgs._TotalText;
	Rows = InArgs._Rows;

	TSharedRef<SVerticalBox> RowBox = SNew(SVerticalBox);
	if (Rows.Num() == 0)
	{
		RowBox->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("No Metrics data for selected bucket.")))
		];
	}
	else
	{
		for (const FBlueprintHelperMetricsDetailBarView& Row : Rows)
		{
			RowBox->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				BuildRow(Row)
			];
		}
	}

	ChildSlot
	[
		SNew(SBorder)
		.Padding(8.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("C - %s"), *Title)))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("%s / %s"), *Subtitle, *TotalText)))
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				RowBox
			]
		]
	];
}

TSharedRef<SWidget> SBlueprintHelperMetricsDetailChart::BuildRow(
	const FBlueprintHelperMetricsDetailBarView& Row) const
{
	const FString ValueText = FString::Printf(
		TEXT("%lld %s"),
		Row.Value,
		*Row.UnitLabel);

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Row.Label))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::FromString(ValueText))
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 3.0f, 0.0f, 3.0f)
		[
			SNew(SBox)
			.HeightOverride(8.0f)
			[
				SNew(SProgressBar)
				.BorderPadding(FVector2D(0.0f, 0.0f))
				.Percent(Row.Fraction)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(Row.SubText))
			.AutoWrapText(true)
		];
}
