// BlueprintHelper Metrics detail chart widget.

#pragma once

#include "CoreMinimal.h"
#include "Systems/Metrics/BlueprintHelperMetricsData.h"
#include "Widgets/SCompoundWidget.h"

class SBlueprintHelperMetricsDetailChart : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintHelperMetricsDetailChart)
	{
	}
		SLATE_ARGUMENT(FString, Title)
		SLATE_ARGUMENT(FString, Subtitle)
		SLATE_ARGUMENT(FString, TotalText)
		SLATE_ARGUMENT(TArray<FBlueprintHelperMetricsDetailBarView>, Rows)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> BuildRow(
		const FBlueprintHelperMetricsDetailBarView& Row) const;

	FString Title;
	FString Subtitle;
	FString TotalText;
	TArray<FBlueprintHelperMetricsDetailBarView> Rows;
};
