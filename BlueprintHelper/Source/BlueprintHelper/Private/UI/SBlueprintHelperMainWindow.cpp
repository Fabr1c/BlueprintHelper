// BlueprintHelper main window shell implementation.

#include "UI/SBlueprintHelperMainWindow.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutCoordinator.h"
#include "UI/BlueprintHelperUiSettingsResolver.h"
#include "UI/BlueprintHelperMainWindowPresenter.h"
#include "UI/Utils/BlueprintHelperMainWindowCleanupAsyncUtils.h"
#include "UI/Layout/SBlueprintHelperLayoutRuleEditor.h"
#include "Styling/AppStyle.h"
#include "UI/TaskSpecWorkbench/SBlueprintHelperTaskSpecWorkbench.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "UI/Review/SBlueprintHelperReviewPanel.h"
#include "UI/Settings/SBlueprintHelperSettingsPanel.h"
#include "Widgets/Text/STextBlock.h"

void SBlueprintHelperMainWindow::Construct(const FArguments& InArgs)
{
	ImportService = InArgs._ImportService;
	GraphResolver = InArgs._GraphResolver;
	ReviewStoreService = InArgs._ReviewStoreService;
	ReviewActionService = InArgs._ReviewActionService;
		MainWindowSettings = FBlueprintHelperUiSettingsResolver::LoadMainWindowSettings();
	NotificationSettings = FBlueprintHelperUiSettingsResolver::LoadNotificationSettings();
	ActivePageIndex = ResolveDefaultTabIndex();
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
		.Padding(MainWindowSettings.TabBarPadding)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(MainWindowSettings.TabButtonSpacing)
			[
				SNew(SButton)
				.ButtonColorAndOpacity(this, &SBlueprintHelperMainWindow::GetToolsTabColor)
				.Text(FText::FromString(TEXT("Tools")))
				.OnClicked(this, &SBlueprintHelperMainWindow::ShowToolsPage)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(MainWindowSettings.TabButtonSpacing)
			[
				SNew(SButton)
				.ButtonColorAndOpacity(this, &SBlueprintHelperMainWindow::GetReviewTabColor)
				.Text(FText::FromString(TEXT("Review")))
				.OnClicked(this, &SBlueprintHelperMainWindow::ShowReviewPage)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(MainWindowSettings.TabButtonSpacing)
			[
				SNew(SButton)
				.ButtonColorAndOpacity(this, &SBlueprintHelperMainWindow::GetLayoutTabColor)
				.Text(FText::FromString(TEXT("Layout")))
				.OnClicked(this, &SBlueprintHelperMainWindow::ShowLayoutPage)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(MainWindowSettings.TabButtonSpacing)
			[
				SNew(SButton)
				.ButtonColorAndOpacity(this, &SBlueprintHelperMainWindow::GetSettingsTabColor)
				.Text(FText::FromString(TEXT("Setting")))
				.OnClicked(this, &SBlueprintHelperMainWindow::ShowSettingsPage)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(MainWindowSettings.CleanupButtonMarginLeft, 0.0f, 0.0f, 0.0f))
			[
				SNew(SButton)
				.Text(MainWindowSettings.CleanupButtonLabel)
				.ToolTipText(FText::FromString(TEXT("Clean reviewed Accept/Reject residual Review records. Pending review data is preserved.")))
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
				SNew(SBlueprintHelperTaskSpecWorkbench)
				.GraphResolver(GraphResolver)
			]
			+ SWidgetSwitcher::Slot()
			[
				SNew(SBlueprintHelperReviewPanel)
				.ReviewStoreService(ReviewStoreService)
				.ReviewActionService(ReviewActionService)
			]
			+ SWidgetSwitcher::Slot()
			[
				SNew(SBlueprintHelperLayoutRuleEditor)
				.InitialRuleSetJson(FBlueprintHelperGraphLayoutCoordinator::LoadConfiguredRuleSetJson())
				.DefaultRuleSetJson(FBlueprintHelperGraphLayoutCoordinator::GetDefaultRuleSetJson())
				.OnImportJson(FBlueprintHelperLayoutRuleEditorImportJson::CreateStatic(&FBlueprintHelperGraphLayoutCoordinator::LoadConfiguredRuleSetJson))
				.OnExportJson(FBlueprintHelperLayoutRuleEditorExportJson::CreateStatic(&FBlueprintHelperGraphLayoutCoordinator::SaveConfiguredRuleSetJson))
				.OnValidateJson(FBlueprintHelperLayoutRuleEditorValidateJson::CreateStatic(&FBlueprintHelperGraphLayoutCoordinator::ValidateRuleSetJson))
			]
			+ SWidgetSwitcher::Slot()
			[
				SNew(SBlueprintHelperSettingsPanel)
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

FReply SBlueprintHelperMainWindow::ShowLayoutPage()
{
	ActivePageIndex = 2;
	if (PageSwitcher.IsValid())
	{
		PageSwitcher->SetActiveWidgetIndex(ActivePageIndex);
	}
	return FReply::Handled();
}

FReply SBlueprintHelperMainWindow::ShowSettingsPage()
{
	ActivePageIndex = 3;
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
	Info.bFireAndForget = NotificationSettings.bCleanupFireAndForget;
	Info.bUseThrobber = NotificationSettings.bCleanupUseThrobber;
	Info.bUseSuccessFailIcons = NotificationSettings.bCleanupUseSuccessFailIcons;
	Info.FadeOutDuration = NotificationSettings.CleanupFadeOutSeconds;
	Info.ExpireDuration = NotificationSettings.CleanupExpireSeconds;

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

int32 SBlueprintHelperMainWindow::ResolveDefaultTabIndex() const
{
	if (MainWindowSettings.DefaultTab.Equals(TEXT("review"), ESearchCase::IgnoreCase))
	{
		return 1;
	}
	if (MainWindowSettings.DefaultTab.Equals(TEXT("layout"), ESearchCase::IgnoreCase))
	{
		return 2;
	}
	if (MainWindowSettings.DefaultTab.Equals(TEXT("settings"), ESearchCase::IgnoreCase))
	{
		return 3;
	}
	return 0;
}

FSlateColor SBlueprintHelperMainWindow::GetTabColor(int32 PageIndex) const
{
	return FSlateColor(ActivePageIndex == PageIndex
		? MainWindowSettings.ActiveTabColor
		: MainWindowSettings.InactiveTabColor);
}

FSlateColor SBlueprintHelperMainWindow::GetToolsTabColor() const
{
	return GetTabColor(0);
}

FSlateColor SBlueprintHelperMainWindow::GetReviewTabColor() const
{
	return GetTabColor(1);
}

FSlateColor SBlueprintHelperMainWindow::GetLayoutTabColor() const
{
	return GetTabColor(2);
}

FSlateColor SBlueprintHelperMainWindow::GetSettingsTabColor() const
{
	return GetTabColor(3);
}
