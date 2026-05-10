// BlueprintHelper fake Review panel implementation.

#include "UI/Review/SBlueprintHelperReviewPanel.h"

#include "HAL/PlatformApplicationMisc.h"
#include "Misc/DateTime.h"
#include "Styling/AppStyle.h"
#include "UI/Review/BlueprintHelperReviewDebugText.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

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
}

FString SBlueprintHelperReviewPanel::BuildDebugMessagesString() const
{
	return FBlueprintHelperReviewDebugText::BuildCopyableText(DebugMessages);
}

FText SBlueprintHelperReviewPanel::GetDebugMessagesText() const
{
	return FText::FromString(BuildDebugMessagesString());
}

FReply SBlueprintHelperReviewPanel::OnCopyDebugMessages() const
{
	const FString DebugText = BuildDebugMessagesString();
	FPlatformApplicationMisc::ClipboardCopy(*DebugText);
	return FReply::Handled();
}

TSharedRef<SWidget> SBlueprintHelperReviewPanel::BuildDebugPanel()
{
	if (DebugMessages.Num() == 0)
	{
		DebugMessages.Add(TEXT("[init] Review debug panel ready."));
	}

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
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(FText::FromString(TEXT("CopyAll")))
					.OnClicked(this, &SBlueprintHelperReviewPanel::OnCopyDebugMessages)
				]
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
