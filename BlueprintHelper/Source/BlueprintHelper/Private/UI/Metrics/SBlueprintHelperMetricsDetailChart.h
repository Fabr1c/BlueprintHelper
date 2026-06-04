// BlueprintHelper Metrics detail chart widget.

#pragma once

#include "CoreMinimal.h"
#include "Systems/Metrics/BlueprintHelperMetricsData.h"
#include "Widgets/SCompoundWidget.h"

class STextBlock;
class SVerticalBox;

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
	void SetData(
		const FString& InTitle,
		const FString& InSubtitle,
		const FString& InTotalText,
		const TArray<FBlueprintHelperMetricsDetailBarView>& InRows);

private:
	void RefreshRows();
	TSharedRef<SWidget> BuildRow(
		const FBlueprintHelperMetricsDetailBarView& Row) const;

	FString Title;
	FString Subtitle;
	FString TotalText;
	TArray<FBlueprintHelperMetricsDetailBarView> Rows;
	TSharedPtr<STextBlock> TitleTextWidget;
	TSharedPtr<STextBlock> SubtitleTextWidget;
	TSharedPtr<STextBlock> TotalTextWidget;
	TSharedPtr<SVerticalBox> RowsBox;
};
