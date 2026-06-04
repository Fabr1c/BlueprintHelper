// BlueprintHelper Metrics Slate panel implementation.

#include "UI/Metrics/SBlueprintHelperMetricsPanel.h"

#include "Styling/AppStyle.h"
#include "UI/Metrics/BlueprintHelperMetricsPanelPresenter.h"
#include "UI/Metrics/SBlueprintHelperMetricsDetailChart.h"
#include "UI/Metrics/SBlueprintHelperMetricsMetricSelector.h"
#include "UI/Metrics/SBlueprintHelperMetricsOverviewChart.h"
#include "UI/Metrics/Utils/BlueprintHelperMetricsPanelAsyncUtils.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

static FString BlueprintHelperMetricsPanelTimelineText(
	EBlueprintHelperMetricsTimelineMode TimelineMode)
{
	return TimelineMode == EBlueprintHelperMetricsTimelineMode::Weekly
		? TEXT("Weekly")
		: TEXT("Daily");
}

void SBlueprintHelperMetricsPanel::Construct(const FArguments& InArgs)
{
	Presenter = FBlueprintHelperMetricsPanelPresenter::CreateDefault();
	CurrentSnapshot = Presenter->GetSnapshot();

	ChildSlot
	[
		SAssignNew(ContentHost, SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
		.Padding(8.0f)
		[
			BuildContent()
		]
	];

	Presenter->SetEventSink(
		[this](const FBlueprintHelperMetricsPanelPresenterEvent& Event)
		{
			HandlePresenterEvent(Event);
		});
	Presenter->HandleVisualEvent(
		FBlueprintHelperMetricsPanelVisualEvent::RefreshClicked());
}

void SBlueprintHelperMetricsPanel::ShutdownAsyncTasks()
{
	FBlueprintHelperMetricsPanelAsyncUtils::ShutdownTasks();
}

void SBlueprintHelperMetricsPanel::HandlePresenterEvent(
	const FBlueprintHelperMetricsPanelPresenterEvent& Event)
{
	if (Event.bRefreshView)
	{
		RefreshFromSnapshot(Event.Snapshot);
	}
}

void SBlueprintHelperMetricsPanel::RefreshFromSnapshot(
	const FBlueprintHelperMetricsPanelSnapshot& Snapshot)
{
	CurrentSnapshot = Snapshot;
	if (ContentHost.IsValid())
	{
		ContentHost->SetContent(BuildContent());
	}
}

TSharedRef<SWidget> SBlueprintHelperMetricsPanel::BuildContent()
{
	return CurrentSnapshot.LoadState == EBlueprintHelperMetricsLoadState::Loaded
		? BuildLoadedContent()
		: BuildStatusContent();
}

TSharedRef<SWidget> SBlueprintHelperMetricsPanel::BuildStatusContent()
{
	const bool bLoading =
		CurrentSnapshot.LoadState == EBlueprintHelperMetricsLoadState::Loading;
	const FString Message = !CurrentSnapshot.ErrorText.IsEmpty()
		? CurrentSnapshot.ErrorText
		: CurrentSnapshot.StatusText;

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SThrobber)
				.Visibility(bLoading ? EVisibility::Visible : EVisibility::Collapsed)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Message))
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(
				TEXT("Mode: %s | files=%d lines=%d warnings=%d"),
				*BlueprintHelperMetricsPanelTimelineText(CurrentSnapshot.Selection.TimelineMode),
				CurrentSnapshot.FilesRead,
				CurrentSnapshot.LinesRead,
				CurrentSnapshot.ParseWarnings)))
		];
}

TSharedRef<SWidget> SBlueprintHelperMetricsPanel::BuildLoadedContent()
{
	return BuildAbcLayout();
}

TSharedRef<SWidget> SBlueprintHelperMetricsPanel::BuildAbcLayout()
{
	const FString SelectedUnitLabel = GetSelectedMetricUnitLabel();
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, 10.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(260.0f)
			[
				SNew(SBlueprintHelperMetricsMetricSelector)
				.Options(CurrentSnapshot.MetricOptions)
				.OnMetricSelected(FOnBlueprintHelperMetricsMetricSelected::CreateSP(
					this,
					&SBlueprintHelperMetricsPanel::HandleMetricSelected))
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(0.48f)
			.Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				SNew(SBlueprintHelperMetricsOverviewChart)
				.TimelineMode(CurrentSnapshot.Selection.TimelineMode)
				.Bars(CurrentSnapshot.OverviewBars)
				.OnTimelineModeSelected(FOnBlueprintHelperMetricsTimelineModeSelected::CreateSP(
					this,
					&SBlueprintHelperMetricsPanel::HandleTimelineModeSelected))
				.OnBucketSelected(FOnBlueprintHelperMetricsBucketSelected::CreateSP(
					this,
					&SBlueprintHelperMetricsPanel::HandleBucketSelected))
				.OnRefreshClicked(FOnBlueprintHelperMetricsRefreshClicked::CreateSP(
					this,
					&SBlueprintHelperMetricsPanel::HandleRefreshRequested))
			]
			+ SVerticalBox::Slot()
			.FillHeight(0.52f)
			[
				SNew(SBlueprintHelperMetricsDetailChart)
				.Title(CurrentSnapshot.SelectedMetricTitle)
				.Subtitle(CurrentSnapshot.SelectedBucketLabel)
				.TotalText(FString::Printf(
					TEXT("%lld %s"),
					CurrentSnapshot.SelectedBucketTotal,
					*SelectedUnitLabel))
				.Rows(CurrentSnapshot.DetailBars)
			]
		];
}

FString SBlueprintHelperMetricsPanel::GetSelectedMetricUnitLabel() const
{
	for (const FBlueprintHelperMetricsMetricOptionView& Option : CurrentSnapshot.MetricOptions)
	{
		if (Option.Kind == CurrentSnapshot.Selection.MetricKind)
		{
			return Option.UnitLabel;
		}
	}
	return TEXT("events");
}

void SBlueprintHelperMetricsPanel::HandleMetricSelected(
	EBlueprintHelperMetricsMetricKind MetricKind)
{
	if (Presenter.IsValid())
	{
		Presenter->HandleVisualEvent(
			FBlueprintHelperMetricsPanelVisualEvent::MetricSelected(MetricKind));
	}
}

void SBlueprintHelperMetricsPanel::HandleTimelineModeSelected(
	EBlueprintHelperMetricsTimelineMode TimelineMode)
{
	if (Presenter.IsValid())
	{
		Presenter->HandleVisualEvent(
			FBlueprintHelperMetricsPanelVisualEvent::TimelineModeChanged(TimelineMode));
	}
}

void SBlueprintHelperMetricsPanel::HandleBucketSelected(const FString& BucketId)
{
	if (Presenter.IsValid())
	{
		Presenter->HandleVisualEvent(
			FBlueprintHelperMetricsPanelVisualEvent::OverviewBucketSelected(BucketId));
	}
}

void SBlueprintHelperMetricsPanel::HandleRefreshRequested()
{
	if (Presenter.IsValid())
	{
		Presenter->HandleVisualEvent(
			FBlueprintHelperMetricsPanelVisualEvent::RefreshClicked());
	}
}
