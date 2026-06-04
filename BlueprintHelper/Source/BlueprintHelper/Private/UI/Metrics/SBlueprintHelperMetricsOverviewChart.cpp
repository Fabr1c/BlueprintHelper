// BlueprintHelper Metrics overview chart widget.

#include "UI/Metrics/SBlueprintHelperMetricsOverviewChart.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SBlueprintHelperMetricsOverviewChart::Construct(const FArguments& InArgs)
{
	TimelineMode = InArgs._TimelineMode;
	Bars = InArgs._Bars;
	OnTimelineModeSelected = InArgs._OnTimelineModeSelected;
	OnBucketSelected = InArgs._OnBucketSelected;
	OnRefreshClicked = InArgs._OnRefreshClicked;

	TSharedRef<SHorizontalBox> BarBox = SNew(SHorizontalBox);
	for (const FBlueprintHelperMetricsOverviewBarView& Bar : Bars)
	{
		BarBox->AddSlot()
		.FillWidth(1.0f)
		.Padding(2.0f, 0.0f)
		[
			BuildBar(Bar)
		];
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
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("B - Timeline Overview")))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Daily")))
					.OnClicked(this, &SBlueprintHelperMetricsOverviewChart::HandleDailyClicked)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Weekly")))
					.OnClicked(this, &SBlueprintHelperMetricsOverviewChart::HandleWeeklyClicked)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Refresh")))
					.OnClicked(this, &SBlueprintHelperMetricsOverviewChart::HandleRefreshClicked)
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0.0f, 10.0f, 0.0f, 0.0f)
			[
				BarBox
			]
		]
	];
}

TSharedRef<SWidget> SBlueprintHelperMetricsOverviewChart::BuildBar(
	const FBlueprintHelperMetricsOverviewBarView& Bar) const
{
	const FLinearColor ButtonColor = Bar.bIsSelected
		? FLinearColor(0.18f, 0.34f, 0.62f, 1.0f)
		: FLinearColor(0.08f, 0.08f, 0.08f, 1.0f);

	return SNew(SButton)
		.ButtonColorAndOpacity(ButtonColor)
		.OnClicked_Lambda(
			[this, BucketId = Bar.BucketId]()
			{
				return HandleBarClicked(BucketId);
			})
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Bar.ValueLabel))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 5.0f, 0.0f, 5.0f)
			[
				SNew(SBox)
				.HeightOverride(8.0f)
				[
					SNew(SProgressBar)
					.BorderPadding(FVector2D(0.0f, 0.0f))
					.Percent(Bar.Fraction)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Bar.Label))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Bar.Detail))
				.AutoWrapText(true)
			]
		];
}

FReply SBlueprintHelperMetricsOverviewChart::HandleDailyClicked() const
{
	OnTimelineModeSelected.ExecuteIfBound(EBlueprintHelperMetricsTimelineMode::Daily);
	return FReply::Handled();
}

FReply SBlueprintHelperMetricsOverviewChart::HandleWeeklyClicked() const
{
	OnTimelineModeSelected.ExecuteIfBound(EBlueprintHelperMetricsTimelineMode::Weekly);
	return FReply::Handled();
}

FReply SBlueprintHelperMetricsOverviewChart::HandleRefreshClicked() const
{
	OnRefreshClicked.ExecuteIfBound();
	return FReply::Handled();
}

FReply SBlueprintHelperMetricsOverviewChart::HandleBarClicked(FString BucketId) const
{
	OnBucketSelected.ExecuteIfBound(BucketId);
	return FReply::Handled();
}
