// BlueprintHelper main window shell implementation.

#include "UI/SBlueprintHelperMainWindow.h"

#include "Framework/Notifications/NotificationManager.h"
#include "UI/BlueprintHelperMainWindowPresenter.h"
#include "UI/Utils/BlueprintHelperMainWindowCleanupAsyncUtils.h"
#include "UI/SHelperMainWidget.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "UI/Review/SBlueprintHelperReviewPanel.h"
#include "Widgets/Text/STextBlock.h"

void SBlueprintHelperMainWindow::Construct(const FArguments& InArgs)
{
	ImportService = InArgs._ImportService;
	GraphResolver = InArgs._GraphResolver;
	ReviewStoreService = InArgs._ReviewStoreService;
	ReviewActionService = InArgs._ReviewActionService;
	MainWindowPresenter = MakeShared<FBlueprintHelperMainWindowPresenter>(ReviewStoreService);
	MainWindowPresenter->SetEventSink([this](const FBlueprintHelperMainWindowPresenterEvent& Event)
	{
		HandleMainWindowPresenterEvent(Event);
	});

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SButton)
				.ButtonColorAndOpacity(this, &SBlueprintHelperMainWindow::GetToolsTabColor)
				.Text(FText::FromString(TEXT("Tools")))
				.OnClicked(this, &SBlueprintHelperMainWindow::ShowToolsPage)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonColorAndOpacity(this, &SBlueprintHelperMainWindow::GetReviewTabColor)
				.Text(FText::FromString(TEXT("Review")))
				.OnClicked(this, &SBlueprintHelperMainWindow::ShowReviewPage)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(10.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Clean Review Data")))
				.ToolTipText(FText::FromString(TEXT("Clean reviewed Accept/Reject residual Review records and unreferenced Transaction files. Pending review data is preserved.")))
				.OnClicked(this, &SBlueprintHelperMainWindow::OnCleanupReviewDataClicked)
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(PageSwitcher, SWidgetSwitcher)
			.WidgetIndex(ActivePageIndex)
			+ SWidgetSwitcher::Slot()
			[
				SNew(SHelperMainWidget)
				.ImportService(ImportService)
				.GraphResolver(GraphResolver)
			]
			+ SWidgetSwitcher::Slot()
			[
				SNew(SBlueprintHelperReviewPanel)
				.ReviewStoreService(ReviewStoreService)
				.ReviewActionService(ReviewActionService)
			]
		]
	];
}

void SBlueprintHelperMainWindow::FlushCleanupTasks()
{
	FBlueprintHelperMainWindowCleanupAsyncUtils::FlushCleanupTasks();
}

void SBlueprintHelperMainWindow::ShutdownCleanupTasks()
{
	FBlueprintHelperMainWindowCleanupAsyncUtils::ShutdownCleanupTasks();
}

FReply SBlueprintHelperMainWindow::ShowToolsPage()
{
	ActivePageIndex = 0;
	if (PageSwitcher.IsValid())
	{
		PageSwitcher->SetActiveWidgetIndex(ActivePageIndex);
	}
	return FReply::Handled();
}

FReply SBlueprintHelperMainWindow::ShowReviewPage()
{
	ActivePageIndex = 1;
	if (PageSwitcher.IsValid())
	{
		PageSwitcher->SetActiveWidgetIndex(ActivePageIndex);
	}
	return FReply::Handled();
}

FReply SBlueprintHelperMainWindow::OnCleanupReviewDataClicked()
{
	return MainWindowPresenter.IsValid()
		? MainWindowPresenter->HandleVisualEvent(
			FBlueprintHelperMainWindowVisualEvent::CleanupReviewDataClicked())
		: FReply::Handled();
}

void SBlueprintHelperMainWindow::HandleMainWindowPresenterEvent(
	const FBlueprintHelperMainWindowPresenterEvent& Event)
{
	if (!Event.LogMessage.IsEmpty())
	{
		if (Event.bLogAsWarning)
		{
			UE_LOG(LogTemp, Warning, TEXT("BlueprintHelper %s"), *Event.LogMessage);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("BlueprintHelper %s"), *Event.LogMessage);
		}
	}
	if (Event.bShowCleanupNotification)
	{
		ShowCleanupNotification(Event.CleanupStatusText);
	}
	if (Event.bUpdateCleanupNotification)
	{
		UpdateCleanupNotification(
			Event.CleanupStatusText,
			Event.bCleanupSucceeded,
			Event.bExpireCleanupNotification);
	}
}

void SBlueprintHelperMainWindow::ShowCleanupNotification(const FString& StatusText)
{
	FNotificationInfo Info(FText::FromString(StatusText));
	Info.bFireAndForget = false;
	Info.bUseThrobber = true;
	Info.bUseSuccessFailIcons = false;
	Info.FadeOutDuration = 0.5f;
	Info.ExpireDuration = 4.0f;

	CleanupNotification = FSlateNotificationManager::Get().AddNotification(Info);
	if (TSharedPtr<SNotificationItem> Notification = CleanupNotification.Pin())
	{
		Notification->SetCompletionState(SNotificationItem::CS_Pending);
	}
}

void SBlueprintHelperMainWindow::UpdateCleanupNotification(
	const FString& StatusText,
	bool bSucceeded,
	bool bExpire)
{
	TSharedPtr<SNotificationItem> Notification = CleanupNotification.Pin();
	if (!Notification.IsValid() && !bExpire)
	{
		ShowCleanupNotification(StatusText);
		Notification = CleanupNotification.Pin();
	}
	if (!Notification.IsValid())
	{
		return;
	}

	Notification->SetText(FText::FromString(StatusText));
	Notification->SetCompletionState(bExpire
		? (bSucceeded ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail)
		: SNotificationItem::CS_Pending);
	if (bExpire)
	{
		Notification->ExpireAndFadeout();
		CleanupNotification.Reset();
	}
}

FSlateColor SBlueprintHelperMainWindow::GetToolsTabColor() const
{
	return FSlateColor(ActivePageIndex == 0
		? FLinearColor(0.18f, 0.34f, 0.62f, 1.0f)
		: FLinearColor(0.08f, 0.08f, 0.08f, 1.0f));
}

FSlateColor SBlueprintHelperMainWindow::GetReviewTabColor() const
{
	return FSlateColor(ActivePageIndex == 1
		? FLinearColor(0.18f, 0.34f, 0.62f, 1.0f)
		: FLinearColor(0.08f, 0.08f, 0.08f, 1.0f));
}

