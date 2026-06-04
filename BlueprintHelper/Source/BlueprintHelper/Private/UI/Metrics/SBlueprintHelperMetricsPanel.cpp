// BlueprintHelper Metrics Slate panel implementation.

#include "UI/Metrics/SBlueprintHelperMetricsPanel.h"

#include "Styling/AppStyle.h"
#include "UI/Metrics/BlueprintHelperMetricsPanelPresenter.h"
#include "UI/Metrics/Utils/BlueprintHelperMetricsPanelAsyncUtils.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

static FString BlueprintHelperMetricsPanelNumber(int64 Value)
{
	return LexToString(Value);
}

static FString BlueprintHelperMetricsPanelTimelineText(
	EBlueprintHelperMetricsTimelineMode TimelineMode)
{
	return TimelineMode == EBlueprintHelperMetricsTimelineMode::Weekly
		? TEXT("Weekly")
		: TEXT("Daily");
}

static TOptional<float> BlueprintHelperMetricsPanelPercent(
	int64 Value,
	int64 MaxValue)
{
	if (MaxValue <= 0)
	{
		return 0.0f;
	}
	return FMath::Clamp(static_cast<float>(Value) / static_cast<float>(MaxValue), 0.0f, 1.0f);
}

static FString BlueprintHelperMetricsPanelUsageDetail(
	const FBlueprintHelperMetricsUsageRow& Row)
{
	return FString::Printf(
		TEXT("success=%d failed=%d rate=%.1f%%"),
		Row.Success,
		Row.Failed,
		Row.SuccessRate * 100.0f);
}

static FString BlueprintHelperMetricsPanelTaskHealthLabel(
	const FBlueprintHelperMetricsTaskHealthRow& Row)
{
	if (!Row.FeatureName.IsEmpty())
	{
		return FString::Printf(
			TEXT("%s | %s"),
			*Row.FeatureName,
			*Row.TargetType);
	}
	return FString::Printf(TEXT("%s | %s"), *Row.TaskType, *Row.TargetType);
}

static FString BlueprintHelperMetricsPanelTaskHealthDetail(
	const FBlueprintHelperMetricsTaskHealthRow& Row)
{
	return FString::Printf(
		TEXT("preview=%d execute=%d success=%d failed=%d"),
		Row.PreviewAttempts,
		Row.ExecuteAttempts,
		Row.SuccessAttempts,
		Row.FailedAttempts);
}

static FString BlueprintHelperMetricsPanelErrorLabel(
	const FBlueprintHelperMetricsErrorRow& Row)
{
	if (!Row.ErrorCode.IsEmpty())
	{
		return Row.ErrorCode;
	}
	if (!Row.IssueCode.IsEmpty())
	{
		return Row.IssueCode;
	}
	return Row.Category.IsEmpty() ? TEXT("unknown") : Row.Category;
}

static FString BlueprintHelperMetricsPanelErrorDetail(
	const FBlueprintHelperMetricsErrorRow& Row)
{
	return FString::Printf(
		TEXT("category=%s issue=%s"),
		*Row.Category,
		*Row.IssueCode);
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

FReply SBlueprintHelperMetricsPanel::OnRefreshClicked()
{
	return Presenter.IsValid()
		? Presenter->HandleVisualEvent(
			FBlueprintHelperMetricsPanelVisualEvent::RefreshClicked())
		: FReply::Handled();
}

FReply SBlueprintHelperMetricsPanel::OnDailyClicked()
{
	return Presenter.IsValid()
		? Presenter->HandleVisualEvent(
			FBlueprintHelperMetricsPanelVisualEvent::TimelineModeChanged(
				EBlueprintHelperMetricsTimelineMode::Daily))
		: FReply::Handled();
}

FReply SBlueprintHelperMetricsPanel::OnWeeklyClicked()
{
	return Presenter.IsValid()
		? Presenter->HandleVisualEvent(
			FBlueprintHelperMetricsPanelVisualEvent::TimelineModeChanged(
				EBlueprintHelperMetricsTimelineMode::Weekly))
		: FReply::Handled();
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
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			BuildToolbar()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 8.0f, 0.0f, 8.0f)
		[
			SNew(SSeparator)
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			CurrentSnapshot.LoadState == EBlueprintHelperMetricsLoadState::Loaded
				? BuildLoadedContent()
				: BuildStatusContent()
		];
}

TSharedRef<SWidget> SBlueprintHelperMetricsPanel::BuildToolbar()
{
	const FString RootText = CurrentSnapshot.MetricsRoot.IsEmpty()
		? TEXT("Metrics root: default")
		: FString::Printf(TEXT("Metrics root: %s"), *CurrentSnapshot.MetricsRoot);

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, 6.0f, 0.0f)
		[
			SNew(SButton)
			.ButtonColorAndOpacity(this, &SBlueprintHelperMetricsPanel::GetDailyButtonColor)
			.Text(FText::FromString(TEXT("Daily")))
			.OnClicked(this, &SBlueprintHelperMetricsPanel::OnDailyClicked)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, 6.0f, 0.0f)
		[
			SNew(SButton)
			.ButtonColorAndOpacity(this, &SBlueprintHelperMetricsPanel::GetWeeklyButtonColor)
			.Text(FText::FromString(TEXT("Weekly")))
			.OnClicked(this, &SBlueprintHelperMetricsPanel::OnWeeklyClicked)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, 10.0f, 0.0f)
		[
			SNew(SButton)
			.Text(FText::FromString(TEXT("Refresh")))
			.OnClicked(this, &SBlueprintHelperMetricsPanel::OnRefreshClicked)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(RootText))
		];
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
				*BlueprintHelperMetricsPanelTimelineText(CurrentSnapshot.TimelineMode),
				CurrentSnapshot.FilesRead,
				CurrentSnapshot.LinesRead,
				CurrentSnapshot.ParseWarnings)))
		];
}

TSharedRef<SWidget> SBlueprintHelperMetricsPanel::BuildLoadedContent()
{
	return SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				BuildSummary()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				BuildBucketBars()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				BuildUsageRows(TEXT("Tool Usage"), CurrentSnapshot.ToolUsageRows)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				BuildTaskHealthRows()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				BuildErrorRows(TEXT("Error Categories"), CurrentSnapshot.ErrorCategoryRows)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				BuildErrorRows(TEXT("Top Errors"), CurrentSnapshot.TopErrorRows)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				BuildIoRows()
			]
		];
}

TSharedRef<SWidget> SBlueprintHelperMetricsPanel::BuildSummary()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(
				TEXT("Summary | mode=%s events=%d failures=%d unknown=%d"),
				*BlueprintHelperMetricsPanelTimelineText(CurrentSnapshot.TimelineMode),
				CurrentSnapshot.Summary.TotalEvents,
				CurrentSnapshot.Summary.TotalFailures,
				CurrentSnapshot.Summary.UnknownErrors)))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(
				TEXT("Input tokens=%s | Output tokens=%s | files=%d lines=%d warnings=%d"),
				*BlueprintHelperMetricsPanelNumber(CurrentSnapshot.Summary.EstimatedInputTokens),
				*BlueprintHelperMetricsPanelNumber(CurrentSnapshot.Summary.EstimatedOutputTokens),
				CurrentSnapshot.FilesRead,
				CurrentSnapshot.LinesRead,
				CurrentSnapshot.ParseWarnings)))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			SNew(SSeparator)
		];
}

TSharedRef<SWidget> SBlueprintHelperMetricsPanel::BuildBucketBars()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot()
	.AutoHeight()
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("Timeline")))
	];

	if (CurrentSnapshot.Buckets.Num() == 0)
	{
		Box->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("No bucket data")))
		];
		return Box;
	}

	int64 MaxValue = 1;
	for (const FBlueprintHelperMetricsBucket& Bucket : CurrentSnapshot.Buckets)
	{
		MaxValue = FMath::Max<int64>(MaxValue, Bucket.TotalEvents);
	}

	for (const FBlueprintHelperMetricsBucket& Bucket : CurrentSnapshot.Buckets)
	{
		const int64 TokenTotal =
			Bucket.EstimatedInputTokens + Bucket.EstimatedOutputTokens;
		const FString Detail = FString::Printf(
			TEXT("tools=%d preview=%d execute=%d failed=%d tokens=%s"),
			Bucket.ToolEvents,
			Bucket.TaskSpecPreviewAttempts,
			Bucket.TaskSpecExecuteAttempts,
			Bucket.FailureCount,
			*BlueprintHelperMetricsPanelNumber(TokenTotal));
		Box->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 5.0f, 0.0f, 0.0f)
		[
			BuildBarRow(Bucket.Label, Bucket.TotalEvents, MaxValue, Detail)
		];
	}
	return Box;
}

TSharedRef<SWidget> SBlueprintHelperMetricsPanel::BuildUsageRows(
	const FString& Title,
	const TArray<FBlueprintHelperMetricsUsageRow>& Rows)
{
	if (Rows.Num() == 0)
	{
		return BuildEmptySection(Title);
	}

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot()
	.AutoHeight()
	[
		SNew(STextBlock)
		.Text(FText::FromString(Title))
	];

	int64 MaxValue = 1;
	for (const FBlueprintHelperMetricsUsageRow& Row : Rows)
	{
		MaxValue = FMath::Max<int64>(MaxValue, Row.Total);
	}

	for (const FBlueprintHelperMetricsUsageRow& Row : Rows)
	{
		Box->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 5.0f, 0.0f, 0.0f)
		[
			BuildBarRow(
				Row.Name,
				Row.Total,
				MaxValue,
				BlueprintHelperMetricsPanelUsageDetail(Row))
		];
	}
	return Box;
}

TSharedRef<SWidget> SBlueprintHelperMetricsPanel::BuildTaskHealthRows()
{
	if (CurrentSnapshot.TaskHealthRows.Num() == 0)
	{
		return BuildEmptySection(TEXT("TaskSpec Health"));
	}

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot()
	.AutoHeight()
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("TaskSpec Health")))
	];

	int64 MaxValue = 1;
	for (const FBlueprintHelperMetricsTaskHealthRow& Row : CurrentSnapshot.TaskHealthRows)
	{
		MaxValue = FMath::Max<int64>(
			MaxValue,
			Row.PreviewAttempts + Row.ExecuteAttempts);
	}

	for (const FBlueprintHelperMetricsTaskHealthRow& Row : CurrentSnapshot.TaskHealthRows)
	{
		Box->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 5.0f, 0.0f, 0.0f)
		[
			BuildBarRow(
				BlueprintHelperMetricsPanelTaskHealthLabel(Row),
				Row.PreviewAttempts + Row.ExecuteAttempts,
				MaxValue,
				BlueprintHelperMetricsPanelTaskHealthDetail(Row))
		];
	}
	return Box;
}

TSharedRef<SWidget> SBlueprintHelperMetricsPanel::BuildErrorRows(
	const FString& Title,
	const TArray<FBlueprintHelperMetricsErrorRow>& Rows)
{
	if (Rows.Num() == 0)
	{
		return BuildEmptySection(Title);
	}

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot()
	.AutoHeight()
	[
		SNew(STextBlock)
		.Text(FText::FromString(Title))
	];

	int64 MaxValue = 1;
	for (const FBlueprintHelperMetricsErrorRow& Row : Rows)
	{
		MaxValue = FMath::Max<int64>(MaxValue, Row.Count);
	}

	for (const FBlueprintHelperMetricsErrorRow& Row : Rows)
	{
		Box->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 5.0f, 0.0f, 0.0f)
		[
			BuildBarRow(
				BlueprintHelperMetricsPanelErrorLabel(Row),
				Row.Count,
				MaxValue,
				BlueprintHelperMetricsPanelErrorDetail(Row))
		];
	}
	return Box;
}

TSharedRef<SWidget> SBlueprintHelperMetricsPanel::BuildIoRows()
{
	if (CurrentSnapshot.IoUsageRows.Num() == 0)
	{
		return BuildEmptySection(TEXT("IO Usage"));
	}

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot()
	.AutoHeight()
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("IO Usage")))
	];

	int64 MaxValue = 1;
	for (const FBlueprintHelperMetricsIoRow& Row : CurrentSnapshot.IoUsageRows)
	{
		MaxValue = FMath::Max<int64>(
			MaxValue,
			Row.EstimatedInputTokens + Row.EstimatedOutputTokens);
	}

	for (const FBlueprintHelperMetricsIoRow& Row : CurrentSnapshot.IoUsageRows)
	{
		const int64 TokenTotal =
			Row.EstimatedInputTokens + Row.EstimatedOutputTokens;
		const FString Detail = FString::Printf(
			TEXT("input chars=%s output chars=%s tokens=%s"),
			*BlueprintHelperMetricsPanelNumber(Row.InputChars),
			*BlueprintHelperMetricsPanelNumber(Row.OutputChars),
			*BlueprintHelperMetricsPanelNumber(TokenTotal));
		Box->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 5.0f, 0.0f, 0.0f)
		[
			BuildBarRow(Row.ToolName, TokenTotal, MaxValue, Detail)
		];
	}
	return Box;
}

TSharedRef<SWidget> SBlueprintHelperMetricsPanel::BuildEmptySection(
	const FString& Title)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(Title))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("No rows")))
		];
}

TSharedRef<SWidget> SBlueprintHelperMetricsPanel::BuildBarRow(
	const FString& Label,
	int64 Value,
	int64 MaxValue,
	const FString& Detail)
{
	const FString RowText = FString::Printf(
		TEXT("%s | %s | %s"),
		*Label,
		*BlueprintHelperMetricsPanelNumber(Value),
		*Detail);

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(RowText))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 3.0f, 0.0f, 0.0f)
		[
			SNew(SBox)
			.HeightOverride(8.0f)
			[
				SNew(SProgressBar)
				.BorderPadding(FVector2D(0.0f, 0.0f))
				.Percent(BlueprintHelperMetricsPanelPercent(Value, MaxValue))
			]
		];
}

FSlateColor SBlueprintHelperMetricsPanel::GetDailyButtonColor() const
{
	return FSlateColor(
		CurrentSnapshot.TimelineMode == EBlueprintHelperMetricsTimelineMode::Daily
			? FLinearColor(0.18f, 0.34f, 0.62f, 1.0f)
			: FLinearColor(0.08f, 0.08f, 0.08f, 1.0f));
}

FSlateColor SBlueprintHelperMetricsPanel::GetWeeklyButtonColor() const
{
	return FSlateColor(
		CurrentSnapshot.TimelineMode == EBlueprintHelperMetricsTimelineMode::Weekly
			? FLinearColor(0.18f, 0.34f, 0.62f, 1.0f)
			: FLinearColor(0.08f, 0.08f, 0.08f, 1.0f));
}
