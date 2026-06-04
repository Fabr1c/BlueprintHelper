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
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

static FString BlueprintHelperMetricsPanelTimelineText(
	EBlueprintHelperMetricsTimelineMode TimelineMode)
{
	return TimelineMode == EBlueprintHelperMetricsTimelineMode::Weekly
		? TEXT("Weekly")
		: TEXT("Daily");
}

static FString BlueprintHelperMetricsPanelOverviewRangeText(
	EBlueprintHelperMetricsTimelineMode TimelineMode)
{
	return TimelineMode == EBlueprintHelperMetricsTimelineMode::Weekly
		? TEXT("Last 8 ISO weeks")
		: TEXT("Last 14 local days");
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
		RefreshFromSnapshot(Event.Snapshot, Event.UpdateScope);
	}
}

void SBlueprintHelperMetricsPanel::RefreshFromSnapshot(
	const FBlueprintHelperMetricsPanelSnapshot& Snapshot,
	EBlueprintHelperMetricsPanelUpdateScope UpdateScope)
{
	const bool bHasLoadedRoot =
		MetricSelectorWidget.IsValid() &&
		OverviewChartWidget.IsValid() &&
		DetailChartWidget.IsValid();

	CurrentSnapshot = Snapshot;
	if (!ContentHost.IsValid())
	{
		return;
	}

	if (Snapshot.LoadState == EBlueprintHelperMetricsLoadState::Loaded)
	{
		if (!bHasLoadedRoot)
		{
			ContentHost->SetContent(BuildLoadedContent());
		}
		ApplySnapshotToRegions(Snapshot, UpdateScope);
		return;
	}

	MetricSelectorWidget.Reset();
	OverviewChartWidget.Reset();
	DetailChartWidget.Reset();
	StatusTextWidget.Reset();
	ContentHost->SetContent(BuildStatusContent());
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
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			BuildAbcLayout()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			SAssignNew(StatusTextWidget, STextBlock)
		];
}

TSharedRef<SWidget> SBlueprintHelperMetricsPanel::BuildAbcLayout()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, 10.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(260.0f)
			[
				SAssignNew(MetricSelectorWidget, SBlueprintHelperMetricsMetricSelector)
				.Options(CurrentSnapshot.MetricOptions)
				.OnMetricSelected(FOnBlueprintHelperMetricsMetricSelected::CreateSP(
					this,
					&SBlueprintHelperMetricsPanel::HandleMetricSelected))
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Vertical)
			+ SSplitter::Slot()
			.Value(0.48f)
			[
				SAssignNew(OverviewChartWidget, SBlueprintHelperMetricsOverviewChart)
				.Title(BuildOverviewTitle())
				.Subtitle(BuildOverviewSubtitle())
				.TimelineMode(CurrentSnapshot.Selection.TimelineMode)
				.Summary(CurrentSnapshot.Summary)
				.Bars(CurrentSnapshot.OverviewBars)
				.bRefreshInProgress(CurrentSnapshot.bRefreshInProgress)
				.OnTimelineModeSelected(
					FOnBlueprintHelperMetricsTimelineModeSelected::CreateSP(
						this,
						&SBlueprintHelperMetricsPanel::HandleTimelineModeSelected))
				.OnBucketSelected(FOnBlueprintHelperMetricsBucketSelected::CreateSP(
					this,
					&SBlueprintHelperMetricsPanel::HandleBucketSelected))
				.OnRefreshClicked(FOnBlueprintHelperMetricsRefreshClicked::CreateSP(
					this,
					&SBlueprintHelperMetricsPanel::HandleRefreshRequested))
			]
			+ SSplitter::Slot()
			.Value(0.52f)
			[
				SAssignNew(DetailChartWidget, SBlueprintHelperMetricsDetailChart)
				.Title(CurrentSnapshot.SelectedMetricTitle)
				.Subtitle(CurrentSnapshot.SelectedBucketLabel)
				.TotalText(BuildDetailTotalText())
				.Rows(CurrentSnapshot.DetailBars)
			]
		];
}

void SBlueprintHelperMetricsPanel::ApplySnapshotToRegions(
	const FBlueprintHelperMetricsPanelSnapshot& Snapshot,
	EBlueprintHelperMetricsPanelUpdateScope UpdateScope)
{
	UpdateStatusText(Snapshot);

	if (MetricSelectorWidget.IsValid() &&
		(UpdateScope == EBlueprintHelperMetricsPanelUpdateScope::MetricSelector ||
			UpdateScope == EBlueprintHelperMetricsPanelUpdateScope::AllRegions ||
			UpdateScope == EBlueprintHelperMetricsPanelUpdateScope::InitialContent))
	{
		MetricSelectorWidget->SetOptions(Snapshot.MetricOptions);
	}

	const bool bNeedsOverviewRefresh =
		UpdateScope == EBlueprintHelperMetricsPanelUpdateScope::Overview ||
		UpdateScope == EBlueprintHelperMetricsPanelUpdateScope::OverviewAndDetail ||
		UpdateScope == EBlueprintHelperMetricsPanelUpdateScope::AllRegions ||
		UpdateScope == EBlueprintHelperMetricsPanelUpdateScope::InitialContent ||
		UpdateScope == EBlueprintHelperMetricsPanelUpdateScope::StatusOnly;
	if (OverviewChartWidget.IsValid() && bNeedsOverviewRefresh)
	{
		OverviewChartWidget->SetData(
			BuildOverviewTitle(),
			BuildOverviewSubtitle(),
			Snapshot.Selection.TimelineMode,
			Snapshot.Summary,
			Snapshot.OverviewBars,
			Snapshot.bRefreshInProgress);
	}

	if (DetailChartWidget.IsValid() &&
		(UpdateScope == EBlueprintHelperMetricsPanelUpdateScope::Detail ||
			UpdateScope == EBlueprintHelperMetricsPanelUpdateScope::OverviewAndDetail ||
			UpdateScope == EBlueprintHelperMetricsPanelUpdateScope::AllRegions ||
			UpdateScope == EBlueprintHelperMetricsPanelUpdateScope::InitialContent))
	{
		DetailChartWidget->SetData(
			Snapshot.SelectedMetricTitle,
			Snapshot.SelectedBucketLabel,
			BuildDetailTotalText(),
			Snapshot.DetailBars);
	}
}

void SBlueprintHelperMetricsPanel::UpdateStatusText(
	const FBlueprintHelperMetricsPanelSnapshot& Snapshot)
{
	if (!StatusTextWidget.IsValid())
	{
		return;
	}

	const FString RefreshText = Snapshot.bRefreshInProgress &&
			!Snapshot.RefreshStatusText.IsEmpty()
		? FString::Printf(TEXT(" | %s"), *Snapshot.RefreshStatusText)
		: FString();
	StatusTextWidget->SetText(FText::FromString(FString::Printf(
		TEXT("Mode: %s | files=%d lines=%d warnings=%d%s"),
		*BlueprintHelperMetricsPanelTimelineText(Snapshot.Selection.TimelineMode),
		Snapshot.FilesRead,
		Snapshot.LinesRead,
		Snapshot.ParseWarnings,
		*RefreshText)));
}

FString SBlueprintHelperMetricsPanel::BuildOverviewTitle() const
{
	return FString::Printf(
		TEXT("B - %s by %s"),
		*CurrentSnapshot.SelectedMetricTitle,
		*BlueprintHelperMetricsPanelTimelineText(CurrentSnapshot.Selection.TimelineMode));
}

FString SBlueprintHelperMetricsPanel::BuildOverviewSubtitle() const
{
	return BlueprintHelperMetricsPanelOverviewRangeText(
		CurrentSnapshot.Selection.TimelineMode);
}

FString SBlueprintHelperMetricsPanel::BuildDetailTotalText() const
{
	return FString::Printf(
		TEXT("%lld %s"),
		CurrentSnapshot.SelectedBucketTotal,
		*GetSelectedMetricUnitLabel());
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
