// BlueprintHelper Metrics metric selector widget.

#include "UI/Metrics/SBlueprintHelperMetricsMetricSelector.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SBlueprintHelperMetricsMetricSelector::Construct(const FArguments& InArgs)
{
	Options = InArgs._Options;
	OnMetricSelected = InArgs._OnMetricSelected;

	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	for (const FBlueprintHelperMetricsMetricOptionView& Option : Options)
	{
		List->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			BuildOption(Option)
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
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("A - Metric Selector")))
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				List
			]
		]
	];
}

TSharedRef<SWidget> SBlueprintHelperMetricsMetricSelector::BuildOption(
	const FBlueprintHelperMetricsMetricOptionView& Option) const
{
	const FLinearColor ButtonColor = Option.bIsSelected
		? FLinearColor(0.18f, 0.34f, 0.62f, 1.0f)
		: FLinearColor(0.08f, 0.08f, 0.08f, 1.0f);
	const FString TotalText = FString::Printf(
		TEXT("%lld %s"),
		Option.Total,
		*Option.UnitLabel);

	return SNew(SButton)
		.ButtonColorAndOpacity(ButtonColor)
		.OnClicked_Lambda(
			[this, MetricKind = Option.Kind]()
			{
				return HandleOptionClicked(MetricKind);
			})
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Option.Label))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(FText::FromString(TotalText))
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Option.Description))
				.AutoWrapText(true)
			]
		];
}

FReply SBlueprintHelperMetricsMetricSelector::HandleOptionClicked(
	EBlueprintHelperMetricsMetricKind MetricKind) const
{
	OnMetricSelected.ExecuteIfBound(MetricKind);
	return FReply::Handled();
}
