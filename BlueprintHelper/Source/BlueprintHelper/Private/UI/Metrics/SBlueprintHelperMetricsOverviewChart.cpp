// BlueprintHelper Metrics overview chart widget.

#include "UI/Metrics/SBlueprintHelperMetricsOverviewChart.h"

#include "InputCoreTypes.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

static FString BlueprintHelperMetricsOverviewTimelineLabel(
	EBlueprintHelperMetricsTimelineMode TimelineMode)
{
	return TimelineMode == EBlueprintHelperMetricsTimelineMode::Weekly
		? TEXT("Week")
		: TEXT("Day");
}

static FString BlueprintHelperMetricsOverviewRangeLabel(
	EBlueprintHelperMetricsTimelineMode TimelineMode)
{
	return TimelineMode == EBlueprintHelperMetricsTimelineMode::Weekly
		? TEXT("Last 8 ISO weeks")
		: TEXT("Last 14 local days");
}

static FString BlueprintHelperMetricsOverviewFormatInteger(int64 Value)
{
	return FString::Printf(TEXT("%lld"), Value);
}

static float BlueprintHelperMetricsOverviewBarFillHeight(float Fraction)
{
	return FMath::Max(4.0f, 150.0f * FMath::Clamp(Fraction, 0.0f, 1.0f));
}

class SBlueprintHelperMetricsOverviewBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintHelperMetricsOverviewBar)
	{
	}
		SLATE_ARGUMENT(FBlueprintHelperMetricsOverviewBarView, Bar)
		SLATE_EVENT(FOnBlueprintHelperMetricsBucketSelected, OnBucketSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Bar = InArgs._Bar;
		OnBucketSelected = InArgs._OnBucketSelected;

		const FLinearColor ContainerColor = Bar.bIsSelected
			? FLinearColor(0.16f, 0.27f, 0.49f, 1.0f)
			: FLinearColor(0.08f, 0.08f, 0.08f, 1.0f);
		const FLinearColor FillColor = Bar.bIsSelected
			? FLinearColor(0.35f, 0.60f, 0.95f, 1.0f)
			: FLinearColor(0.30f, 0.44f, 0.72f, 1.0f);

		SetToolTipText(FText::FromString(Bar.Detail));

		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.DarkGroupBorder")))
			.BorderBackgroundColor(ContainerColor)
			.Padding(FMargin(6.0f, 6.0f, 6.0f, 8.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Bar.ValueLabel.IsEmpty()
						? BlueprintHelperMetricsOverviewFormatInteger(Bar.Value)
						: Bar.ValueLabel))
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(0.0f, 8.0f, 0.0f, 8.0f)
				.VAlign(VAlign_Fill)
				[
					SNew(SBox)
					.HeightOverride(160.0f)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.DarkGroupBorder")))
						.BorderBackgroundColor(FLinearColor(0.03f, 0.03f, 0.03f, 1.0f))
						.Padding(6.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.FillHeight(1.0f)
							.VAlign(VAlign_Bottom)
							[
								SNew(SBox)
								.HeightOverride(BlueprintHelperMetricsOverviewBarFillHeight(
									Bar.Fraction))
								[
									SNew(SBorder)
									.BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
									.BorderBackgroundColor(FillColor)
								]
							]
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Bar.Label))
				]
			]
		];
	}

	virtual FReply OnMouseButtonDown(
		const FGeometry& MyGeometry,
		const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			OnBucketSelected.ExecuteIfBound(Bar.BucketId);
			return FReply::Handled();
		}
		return SCompoundWidget::OnMouseButtonDown(MyGeometry, MouseEvent);
	}

private:
	FBlueprintHelperMetricsOverviewBarView Bar;
	FOnBlueprintHelperMetricsBucketSelected OnBucketSelected;
};

void SBlueprintHelperMetricsOverviewChart::Construct(const FArguments& InArgs)
{
	OnTimelineModeSelected = InArgs._OnTimelineModeSelected;
	OnBucketSelected = InArgs._OnBucketSelected;
	OnRefreshClicked = InArgs._OnRefreshClicked;

	ChildSlot
	[
		SNew(SBorder)
		.Padding(8.0f)
		[
			SAssignNew(RootBox, SVerticalBox)
		]
	];

	SetData(
		InArgs._Title,
		InArgs._Subtitle,
		InArgs._TimelineMode,
		InArgs._Summary,
		InArgs._Bars,
		InArgs._bRefreshInProgress);
}

void SBlueprintHelperMetricsOverviewChart::SetData(
	const FString& InTitle,
	const FString& InSubtitle,
	EBlueprintHelperMetricsTimelineMode InTimelineMode,
	const FBlueprintHelperMetricsSummary& InSummary,
	const TArray<FBlueprintHelperMetricsOverviewBarView>& InBars,
	bool bInRefreshInProgress)
{
	Title = InTitle;
	Subtitle = InSubtitle;
	TimelineMode = InTimelineMode;
	Summary = InSummary;
	Bars = InBars;
	bRefreshInProgress = bInRefreshInProgress;
	RefreshView();
}

void SBlueprintHelperMetricsOverviewChart::RefreshView()
{
	if (!RootBox.IsValid())
	{
		return;
	}

	RootBox->ClearChildren();

	RootBox->AddSlot()
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Title.IsEmpty()
					? FString::Printf(
						TEXT("B - Overview by %s"),
						*BlueprintHelperMetricsOverviewTimelineLabel(TimelineMode))
					: Title))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Subtitle.IsEmpty()
					? BlueprintHelperMetricsOverviewRangeLabel(TimelineMode)
					: Subtitle))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(
				bRefreshInProgress ? TEXT("Refreshing Metrics data...") : TEXT("")))
		]
	];

	RootBox->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 10.0f, 0.0f, 0.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, 6.0f, 0.0f)
		[
			SNew(SButton)
			.ButtonColorAndOpacity(
				TimelineMode == EBlueprintHelperMetricsTimelineMode::Daily
					? FLinearColor(0.18f, 0.34f, 0.62f, 1.0f)
					: FLinearColor(0.08f, 0.08f, 0.08f, 1.0f))
			.Text(FText::FromString(TEXT("Daily")))
			.OnClicked(this, &SBlueprintHelperMetricsOverviewChart::HandleDailyClicked)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, 6.0f, 0.0f)
		[
			SNew(SButton)
			.ButtonColorAndOpacity(
				TimelineMode == EBlueprintHelperMetricsTimelineMode::Weekly
					? FLinearColor(0.18f, 0.34f, 0.62f, 1.0f)
					: FLinearColor(0.08f, 0.08f, 0.08f, 1.0f))
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
	];

	RootBox->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 12.0f, 0.0f, 0.0f)
	[
		SNew(SUniformGridPanel)
		.SlotPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
		+ SUniformGridPanel::Slot(0, 0)
		[
			BuildSummaryCard(
				TEXT("Total Events"),
				BlueprintHelperMetricsOverviewFormatInteger(Summary.TotalEvents),
				TEXT("All Metrics events"))
		]
		+ SUniformGridPanel::Slot(1, 0)
		[
			BuildSummaryCard(
				TEXT("Failures"),
				BlueprintHelperMetricsOverviewFormatInteger(Summary.TotalFailures),
				TEXT("Failed events"))
		]
		+ SUniformGridPanel::Slot(2, 0)
		[
			BuildSummaryCard(
				TEXT("Unknown Errors"),
				BlueprintHelperMetricsOverviewFormatInteger(Summary.UnknownErrors),
				TEXT("Unclassified failures"))
		]
		+ SUniformGridPanel::Slot(3, 0)
		[
			BuildSummaryCard(
				TEXT("Estimated Tokens"),
				BlueprintHelperMetricsOverviewFormatInteger(
					Summary.EstimatedInputTokens + Summary.EstimatedOutputTokens),
				FString::Printf(
					TEXT("In %lld / Out %lld"),
					Summary.EstimatedInputTokens,
					Summary.EstimatedOutputTokens))
		]
	];

	TSharedRef<SHorizontalBox> BarBox = SNew(SHorizontalBox);
	for (const FBlueprintHelperMetricsOverviewBarView& Bar : Bars)
	{
		BarBox->AddSlot()
		.FillWidth(1.0f)
		.Padding(0.0f, 0.0f, 6.0f, 0.0f)
		[
			BuildBar(Bar)
		];
	}

	TSharedRef<SWidget> BarsContent = Bars.Num() > 0
		? StaticCastSharedRef<SWidget>(BarBox)
		: StaticCastSharedRef<SWidget>(
			SNew(SBorder)
			.Padding(12.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("No Metrics timeline data.")))
			]);

	RootBox->AddSlot()
	.FillHeight(1.0f)
	.Padding(0.0f, 14.0f, 0.0f, 0.0f)
	[
		BarsContent
	];
}

TSharedRef<SWidget> SBlueprintHelperMetricsOverviewChart::BuildSummaryCard(
	const FString& Label,
	const FString& ValueText,
	const FString& CaptionText) const
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
		.Padding(10.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(ValueText))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(CaptionText))
				.AutoWrapText(true)
			]
		];
}

TSharedRef<SWidget> SBlueprintHelperMetricsOverviewChart::BuildBar(
	const FBlueprintHelperMetricsOverviewBarView& Bar) const
{
	return SNew(SBlueprintHelperMetricsOverviewBar)
		.Bar(Bar)
		.OnBucketSelected(OnBucketSelected);
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
