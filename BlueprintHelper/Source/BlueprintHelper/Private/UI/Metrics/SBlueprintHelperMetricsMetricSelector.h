// BlueprintHelper Metrics metric selector widget.

#pragma once

#include "CoreMinimal.h"
#include "Systems/Metrics/BlueprintHelperMetricsData.h"
#include "Widgets/SCompoundWidget.h"

DECLARE_DELEGATE_OneParam(
	FOnBlueprintHelperMetricsMetricSelected,
	EBlueprintHelperMetricsMetricKind);

class SVerticalBox;

class SBlueprintHelperMetricsMetricSelector : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintHelperMetricsMetricSelector)
	{
	}
		SLATE_ARGUMENT(TArray<FBlueprintHelperMetricsMetricOptionView>, Options)
		SLATE_EVENT(FOnBlueprintHelperMetricsMetricSelected, OnMetricSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetOptions(const TArray<FBlueprintHelperMetricsMetricOptionView>& InOptions);

private:
	void RebuildOptions();
	TSharedRef<SWidget> BuildOption(
		const FBlueprintHelperMetricsMetricOptionView& Option) const;
	FReply HandleOptionClicked(EBlueprintHelperMetricsMetricKind MetricKind) const;

	TArray<FBlueprintHelperMetricsMetricOptionView> Options;
	FOnBlueprintHelperMetricsMetricSelected OnMetricSelected;
	TSharedPtr<SVerticalBox> OptionsBox;
};
