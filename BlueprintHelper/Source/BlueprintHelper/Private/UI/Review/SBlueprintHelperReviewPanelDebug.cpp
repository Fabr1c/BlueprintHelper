// BlueprintHelper fake Review panel implementation.

#include "UI/Review/SBlueprintHelperReviewPanel.h"

#include "HAL/PlatformApplicationMisc.h"
#include "Misc/DateTime.h"
#include "Styling/AppStyle.h"
#include "UI/Review/BlueprintHelperReviewDebugBundleService.h"
#include "UI/Review/BlueprintHelperReviewDebugFocusTraversalCoordinator.h"
#include "UI/Review/BlueprintHelperReviewDebugPresenter.h"
#include "UI/Review/BlueprintHelperReviewDebugText.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SBlueprintHelperReviewPanel::EnsureDebugBundleSession()
{
	if (DebugBundleSessionId.IsEmpty())
	{
		DebugBundleSessionId = TEXT("review_panel_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	}
	if (DebugBundlePath.IsEmpty())
	{
		DebugBundlePath = FBlueprintHelperReviewDebugBundleService::MakeDefaultBundlePath();
	}
	else
	{
		DebugBundlePath = FBlueprintHelperReviewDebugBundleService::NormalizeBundlePath(DebugBundlePath);
	}
	if (DebugBundlePathTextBox.IsValid())
	{
		DebugBundlePathTextBox->SetText(FText::FromString(DebugBundlePath));
	}
}

void SBlueprintHelperReviewPanel::AppendDebugBundleEvent(const TSharedRef<FJsonObject>& Event)
{
	EnsureDebugBundleSession();

	FString Error;
	if (FBlueprintHelperReviewDebugBundleService::AppendEvent(
		DebugBundlePath,
		DebugBundleSessionId,
		Event,
		&Error))
	{
		return;
	}

	DebugMessages.Insert(FString::Printf(
		TEXT("[%s] DebugBundle write failed: %s"),
		*FDateTime::Now().ToString(TEXT("%H:%M:%S")),
		*Error), 0);
	if (DebugMessageTextBox.IsValid())
	{
		DebugMessageTextBox->SetText(GetDebugMessagesText());
	}
}

void SBlueprintHelperReviewPanel::AddDebugMessage(const FString& Message)
{
	if (DebugMessages.Num() > 0 && DebugMessages[0].EndsWith(TEXT("] ") + Message))
	{
		return;
	}

	DebugMessages.Insert(FString::Printf(
		TEXT("[%s] %s"),
		*FDateTime::Now().ToString(TEXT("%H:%M:%S")),
		*Message), 0);
	if (DebugMessages.Num() > ReviewPanelSettings.DebugMaxMessages)
	{
		DebugMessages.SetNum(ReviewPanelSettings.DebugMaxMessages);
	}

	if (DebugMessageTextBox.IsValid())
	{
		DebugMessageTextBox->SetText(GetDebugMessagesText());
	}

	EnsureDebugBundleSession();
	AppendDebugBundleEvent(FBlueprintHelperReviewDebugBundleService::BuildLogEvent(
		DebugBundleSessionId,
		Message,
		SelectedChange,
		ReviewAssetContext.AssetPath));
}

FString SBlueprintHelperReviewPanel::BuildDebugMessagesString() const
{
	return FBlueprintHelperReviewDebugText::BuildCopyableText(DebugMessages);
}

FText SBlueprintHelperReviewPanel::GetDebugMessagesText() const
{
	return FText::FromString(BuildDebugMessagesString());
}

FText SBlueprintHelperReviewPanel::GetDebugBundlePathText() const
{
	return FText::FromString(DebugBundlePath);
}

void SBlueprintHelperReviewPanel::OnDebugBundlePathCommitted(const FText& Text, ETextCommit::Type)
{
	DebugBundlePath = FBlueprintHelperReviewDebugBundleService::NormalizeBundlePath(Text.ToString());
	if (DebugBundlePathTextBox.IsValid())
	{
		DebugBundlePathTextBox->SetText(FText::FromString(DebugBundlePath));
	}
	AddDebugMessage(FString::Printf(TEXT("DebugBundle path set to %s"), *DebugBundlePath));
}

FReply SBlueprintHelperReviewPanel::OnCopyDebugMessages() const
{
	const FString DebugText = BuildDebugMessagesString();
	FPlatformApplicationMisc::ClipboardCopy(*DebugText);
	return FReply::Handled();
}

FReply SBlueprintHelperReviewPanel::OnCopyDebugBundlePath() const
{
	FPlatformApplicationMisc::ClipboardCopy(*DebugBundlePath);
	return FReply::Handled();
}

FReply SBlueprintHelperReviewPanel::OnClearDebugMessages()
{
	DebugMessages.Reset();
	DebugMessages.Add(FString::Printf(
		TEXT("[%s] Review debug panel cleared."),
		*FDateTime::Now().ToString(TEXT("%H:%M:%S"))));
	if (DebugMessageTextBox.IsValid())
	{
		DebugMessageTextBox->SetText(GetDebugMessagesText());
	}
	return FReply::Handled();
}

FReply SBlueprintHelperReviewPanel::OnLoadDebugBundle()
{
	EnsureDebugBundleSession();
	if (DebugBundlePathTextBox.IsValid())
	{
		DebugBundlePath = FBlueprintHelperReviewDebugBundleService::NormalizeBundlePath(
			DebugBundlePathTextBox->GetText().ToString());
		DebugBundlePathTextBox->SetText(FText::FromString(DebugBundlePath));
	}

	FBlueprintHelperReviewDebugBundleService::FlushAsyncWrites();

	FString Error;
	FBlueprintHelperReviewDebugPresenter DebugPresenter;
	FBlueprintHelperReviewDebugTimelineModel TimelineModel;
	if (!DebugPresenter.LoadBundle(DebugBundlePath, TimelineModel, Error))
	{
		AddDebugMessage(FString::Printf(TEXT("DebugBundle load failed: %s"), *Error));
		return FReply::Handled();
	}

	FString BundleSummary;
	if (!FBlueprintHelperReviewDebugBundleService::LoadBundleSummaryText(DebugBundlePath, BundleSummary, &Error))
	{
		BundleSummary = FString::Printf(TEXT("DebugBundle summary failed: %s"), *Error);
	}

	TArray<FString> TimelineLines;
	for (const FBlueprintHelperReviewDebugEventModel& Event : TimelineModel.Events)
	{
		TimelineLines.Add(FString::Printf(
			TEXT("[%s] %s review=%s asset=%s target=%s result=%s %s"),
			*Event.Timestamp,
			*Event.EventType,
			*Event.ReviewEventId,
			*Event.AssetPath,
			*Event.TargetKey,
			*Event.Result,
			*Event.Message));
	}

	DebugMessages.Insert(BundleSummary, 0);
	DebugMessages.Insert(FBlueprintHelperReviewDebugText::BuildCopyableText(TimelineLines), 0);
	if (DebugMessageTextBox.IsValid())
	{
		DebugMessageTextBox->SetText(GetDebugMessagesText());
	}
	AddDebugMessage(FString::Printf(TEXT("DebugBundle loaded from %s"), *DebugBundlePath));
	return FReply::Handled();
}

FReply SBlueprintHelperReviewPanel::OnCaptureFocusDebugBundle()
{
	EnsureDebugBundleSession();
	ApplyDebugFocusTraversalStep(DebugFocusTraversalCoordinator.Start(
		ChangeItems,
		ReviewAssetContext.AssetPath,
		ReviewPanelSettings.bOverlayFilterCurrentAssetOnly));
	AdvanceDebugFocusTraversal();
	return FReply::Handled();
}

void SBlueprintHelperReviewPanel::AdvanceDebugFocusTraversal()
{
	ApplyDebugFocusTraversalStep(DebugFocusTraversalCoordinator.Advance());
	ProcessDebugFocusTraversalGeometryEvent();
}

void SBlueprintHelperReviewPanel::ProcessDebugFocusTraversalGeometryEvent()
{
	const FBlueprintHelperReviewDebugFocusTraversalStep Step =
		DebugFocusTraversalCoordinator.ProcessGeometryEvent(
			[this](FReviewChangeItem Item, FString& OutReason)
			{
				return IsDebugFocusTraversalChangeReady(Item, OutReason);
			});
	ApplyDebugFocusTraversalStep(Step);
	if (Step.Kind == EBlueprintHelperReviewDebugFocusTraversalStepKind::FocusReady)
	{
		AdvanceDebugFocusTraversal();
	}
}

void SBlueprintHelperReviewPanel::ApplyDebugFocusTraversalStep(
	const FBlueprintHelperReviewDebugFocusTraversalStep& Step)
{
	switch (Step.Kind)
	{
	case EBlueprintHelperReviewDebugFocusTraversalStepKind::AlreadyRunning:
	case EBlueprintHelperReviewDebugFocusTraversalStepKind::SkippedNoRows:
		AddDebugMessage(Step.Message);
		break;
	case EBlueprintHelperReviewDebugFocusTraversalStepKind::Started:
		AppendDebugBundleEvent(FBlueprintHelperReviewDebugBundleService::BuildFocusEvent(
			DebugBundleSessionId,
			Step.EventStage,
			Step.EventIndex,
			Step.Total,
			TSharedPtr<FBlueprintHelperReviewVisibleChange>(),
			ReviewAssetContext.AssetPath));
		AddDebugMessage(Step.Message);
		break;
	case EBlueprintHelperReviewDebugFocusTraversalStepKind::Focus:
		AppendDebugBundleEvent(FBlueprintHelperReviewDebugBundleService::BuildFocusEvent(
			DebugBundleSessionId,
			Step.EventStage,
			Step.EventIndex,
			Step.Total,
			Step.Item,
			ReviewAssetContext.AssetPath));
		OnChangeSelectionChanged(Step.Item, ESelectInfo::Direct);
		break;
	case EBlueprintHelperReviewDebugFocusTraversalStepKind::Complete:
		AppendDebugBundleEvent(FBlueprintHelperReviewDebugBundleService::BuildFocusEvent(
			DebugBundleSessionId,
			Step.EventStage,
			Step.EventIndex,
			Step.Total,
			TSharedPtr<FBlueprintHelperReviewVisibleChange>(),
			ReviewAssetContext.AssetPath));
		AddDebugMessage(Step.Message);
		break;
	case EBlueprintHelperReviewDebugFocusTraversalStepKind::FocusReady:
	case EBlueprintHelperReviewDebugFocusTraversalStepKind::WaitGeometry:
		AppendDebugBundleEvent(FBlueprintHelperReviewDebugBundleService::BuildFocusEvent(
			DebugBundleSessionId,
			Step.EventStage,
			Step.EventIndex,
			Step.Total,
			Step.Item,
			ReviewAssetContext.AssetPath,
			Step.Reason));
		break;
	default:
		break;
	}
}

bool SBlueprintHelperReviewPanel::IsDebugFocusTraversalChangeReady(FReviewChangeItem Item, FString& OutReason)
{
	if (!Item.IsValid())
	{
		OutReason = TEXT("invalid_change");
		return true;
	}

	bool bHasRowSurfaceTarget = false;
	FString LastReason;
	for (const FBlueprintHelperReviewAtomicTarget& Target : Item->AtomicTargets)
	{
		const EBlueprintHelperReviewSurface Surface = Target.Surface;
		const bool bRowSurface =
			Surface == EBlueprintHelperReviewSurface::Components ||
			Surface == EBlueprintHelperReviewSurface::UMGWidgetTree ||
			Surface == EBlueprintHelperReviewSurface::MyBlueprint ||
			Surface == EBlueprintHelperReviewSurface::Details ||
			Surface == EBlueprintHelperReviewSurface::DataTable ||
			Surface == EBlueprintHelperReviewSurface::DataAsset;
		if (!bRowSurface)
		{
			continue;
		}

		bHasRowSurfaceTarget = true;
		FBlueprintHelperReviewSurfaceGeometryAnchor Anchor;
		if (ResolveReviewRowGeometry(*Item, Surface, Anchor))
		{
			OutReason = FString::Printf(TEXT("geometry_ready:%s"), BlueprintHelperReviewSurfaceToString(Surface));
			return true;
		}
		LastReason = Anchor.Reason;
	}

	if (!bHasRowSurfaceTarget)
	{
		OutReason = TEXT("no_row_surface_required");
		return true;
	}

	OutReason = LastReason.IsEmpty() ? TEXT("geometry_not_ready") : LastReason;
	return false;
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildDebugPanel()
{
	if (DebugMessages.Num() == 0)
	{
		DebugMessages.Add(TEXT("[init] Review debug panel ready."));
	}
	EnsureDebugBundleSession();

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel")))
		.BorderBackgroundColor(FLinearColor(0.035f, 0.035f, 0.035f, 1.0f))
		.Padding(0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f, 6.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Debug")))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(FText::FromString(TEXT("CaptureFocus")))
					.OnClicked(this, &SBlueprintHelperReviewPanel::OnCaptureFocusDebugBundle)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(FText::FromString(TEXT("LoadBundle")))
					.OnClicked(this, &SBlueprintHelperReviewPanel::OnLoadDebugBundle)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(FText::FromString(TEXT("CopyPath")))
					.OnClicked(this, &SBlueprintHelperReviewPanel::OnCopyDebugBundlePath)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(FText::FromString(TEXT("Clear")))
					.OnClicked(this, &SBlueprintHelperReviewPanel::OnClearDebugMessages)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(FText::FromString(TEXT("CopyAll")))
					.OnClicked(this, &SBlueprintHelperReviewPanel::OnCopyDebugMessages)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f, 0.0f, 8.0f, 6.0f)
			[
				SAssignNew(DebugBundlePathTextBox, SEditableTextBox)
				.Text(this, &SBlueprintHelperReviewPanel::GetDebugBundlePathText)
				.OnTextCommitted(this, &SBlueprintHelperReviewPanel::OnDebugBundlePathCommitted)
				.ToolTipText(FText::FromString(TEXT("DebugBundle JSON path. Must stay inside Saved/BlueprintHelper/Debug.")))
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(8.0f, 0.0f, 8.0f, 8.0f)
			[
				SAssignNew(DebugMessageTextBox, SMultiLineEditableTextBox)
				.Text(this, &SBlueprintHelperReviewPanel::GetDebugMessagesText)
				.IsReadOnly(true)
				.AllowContextMenu(true)
				.AlwaysShowScrollbars(true)
				.AutoWrapText(false)
				.Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
				.ForegroundColor(FSlateColor(FLinearColor(0.72f, 0.72f, 0.72f, 1.0f)))
			]
		];
}
