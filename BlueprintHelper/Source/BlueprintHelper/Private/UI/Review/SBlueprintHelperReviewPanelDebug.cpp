// BlueprintHelper fake Review panel implementation.

#include "UI/Review/SBlueprintHelperReviewPanel.h"

#include "HAL/PlatformApplicationMisc.h"
#include "Misc/DateTime.h"
#include "Styling/AppStyle.h"
#include "UI/Review/BlueprintHelperReviewDebugBundleService.h"
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
	static constexpr int32 MaxDebugMessages = 200;

	DebugMessages.Insert(FString::Printf(
		TEXT("[%s] %s"),
		*FDateTime::Now().ToString(TEXT("%H:%M:%S")),
		*Message), 0);
	if (DebugMessages.Num() > MaxDebugMessages)
	{
		DebugMessages.SetNum(MaxDebugMessages);
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

FReply SBlueprintHelperReviewPanel::OnLoadDebugBundle()
{
	EnsureDebugBundleSession();
	if (DebugBundlePathTextBox.IsValid())
	{
		DebugBundlePath = FBlueprintHelperReviewDebugBundleService::NormalizeBundlePath(
			DebugBundlePathTextBox->GetText().ToString());
		DebugBundlePathTextBox->SetText(FText::FromString(DebugBundlePath));
	}

	FString BundleText;
	FString Error;
	if (!FBlueprintHelperReviewDebugBundleService::LoadBundleText(DebugBundlePath, BundleText, &Error))
	{
		AddDebugMessage(FString::Printf(TEXT("DebugBundle load failed: %s"), *Error));
		return FReply::Handled();
	}

	DebugMessages.Insert(BundleText, 0);
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
	if (bDebugFocusTraversalActive)
	{
		AddDebugMessage(TEXT("Debug focus traversal is already running."));
		return FReply::Handled();
	}

	DebugFocusTraversalItems.Reset();
	const FString CurrentAssetPath = ReviewAssetContext.AssetPath;
	for (const FReviewChangeItem& Item : ChangeItems)
	{
		if (!Item.IsValid())
		{
			continue;
		}
		if (!CurrentAssetPath.IsEmpty() && Item->AssetPath != CurrentAssetPath)
		{
			continue;
		}
		DebugFocusTraversalItems.Add(Item);
	}
	if (DebugFocusTraversalItems.Num() == 0)
	{
		for (const FReviewChangeItem& Item : ChangeItems)
		{
			if (Item.IsValid())
			{
				DebugFocusTraversalItems.Add(Item);
			}
		}
	}
	if (DebugFocusTraversalItems.Num() == 0)
	{
		AddDebugMessage(TEXT("Debug focus traversal skipped: no final change rows available."));
		return FReply::Handled();
	}

	DebugFocusTraversalIndex = 0;
	bDebugFocusTraversalActive = true;
	AppendDebugBundleEvent(FBlueprintHelperReviewDebugBundleService::BuildFocusEvent(
		DebugBundleSessionId,
		TEXT("start"),
		0,
		DebugFocusTraversalItems.Num(),
		TSharedPtr<FBlueprintHelperReviewVisibleChange>(),
		CurrentAssetPath));
	AddDebugMessage(FString::Printf(
		TEXT("Debug focus traversal started: %d rows."),
		DebugFocusTraversalItems.Num()));
	RegisterActiveTimer(
		0.12f,
		FWidgetActiveTimerDelegate::CreateSP(this, &SBlueprintHelperReviewPanel::TickDebugFocusTraversal));
	return FReply::Handled();
}

EActiveTimerReturnType SBlueprintHelperReviewPanel::TickDebugFocusTraversal(double, float)
{
	if (!bDebugFocusTraversalActive)
	{
		return EActiveTimerReturnType::Stop;
	}

	if (!DebugFocusTraversalItems.IsValidIndex(DebugFocusTraversalIndex))
	{
		bDebugFocusTraversalActive = false;
		AppendDebugBundleEvent(FBlueprintHelperReviewDebugBundleService::BuildFocusEvent(
			DebugBundleSessionId,
			TEXT("complete"),
			DebugFocusTraversalItems.Num(),
			DebugFocusTraversalItems.Num(),
			TSharedPtr<FBlueprintHelperReviewVisibleChange>(),
			ReviewAssetContext.AssetPath));
		AddDebugMessage(FString::Printf(
			TEXT("Debug focus traversal completed: %d rows."),
			DebugFocusTraversalItems.Num()));
		return EActiveTimerReturnType::Stop;
	}

	const FReviewChangeItem Item = DebugFocusTraversalItems[DebugFocusTraversalIndex];
	AppendDebugBundleEvent(FBlueprintHelperReviewDebugBundleService::BuildFocusEvent(
		DebugBundleSessionId,
		TEXT("focus"),
		DebugFocusTraversalIndex + 1,
		DebugFocusTraversalItems.Num(),
		Item,
		ReviewAssetContext.AssetPath));
	OnChangeSelectionChanged(Item, ESelectInfo::Direct);
	++DebugFocusTraversalIndex;
	return EActiveTimerReturnType::Continue;
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
