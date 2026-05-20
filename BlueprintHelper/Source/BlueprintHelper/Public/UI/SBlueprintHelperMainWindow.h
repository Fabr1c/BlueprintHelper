// BlueprintHelper main window shell.

#pragma once

#include "CoreMinimal.h"
#include "UI/BlueprintHelperUiSettings.h"
#include "Widgets/SCompoundWidget.h"

class FBlueprintHelperGraphResolver;
class FBlueprintHelperImportService;
class FBlueprintHelperMainWindowPresenter;
class FBlueprintHelperReviewActionService;
class FBlueprintHelperReviewStoreService;
class SNotificationItem;
class SWidgetSwitcher;
struct FBlueprintHelperMainWindowPresenterEvent;

class SBlueprintHelperMainWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintHelperMainWindow)
		: _ImportService(nullptr)
		, _GraphResolver(nullptr)
		, _ReviewStoreService(nullptr)
		, _ReviewActionService(nullptr)
	{
	}

	SLATE_ARGUMENT(const FBlueprintHelperImportService*, ImportService)
	SLATE_ARGUMENT(const FBlueprintHelperGraphResolver*, GraphResolver)
	SLATE_ARGUMENT(const FBlueprintHelperReviewStoreService*, ReviewStoreService)
	SLATE_ARGUMENT(const FBlueprintHelperReviewActionService*, ReviewActionService)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	static void FlushCleanupTasks();
	static void ShutdownCleanupTasks();

private:
	FReply ShowToolsPage();
	FReply ShowReviewPage();
	FReply ShowLayoutPage();
	FReply ShowSettingsPage();
	FReply OnCleanupReviewDataClicked();
	void HandleMainWindowPresenterEvent(const FBlueprintHelperMainWindowPresenterEvent& Event);
	void ShowCleanupNotification(const FString& StatusText);
	void UpdateCleanupNotification(const FString& StatusText, bool bSucceeded, bool bExpire);
	int32 ResolveDefaultTabIndex() const;
	FSlateColor GetTabColor(int32 PageIndex) const;
	FSlateColor GetToolsTabColor() const;
	FSlateColor GetReviewTabColor() const;
	FSlateColor GetLayoutTabColor() const;
	FSlateColor GetSettingsTabColor() const;

	const FBlueprintHelperImportService* ImportService = nullptr;
	const FBlueprintHelperGraphResolver* GraphResolver = nullptr;
	const FBlueprintHelperReviewStoreService* ReviewStoreService = nullptr;
	const FBlueprintHelperReviewActionService* ReviewActionService = nullptr;
	TSharedPtr<FBlueprintHelperMainWindowPresenter> MainWindowPresenter;
	TSharedPtr<SWidgetSwitcher> PageSwitcher;
	FBlueprintHelperMainWindowSettings MainWindowSettings;
	FBlueprintHelperNotificationSettings NotificationSettings;
	TWeakPtr<SNotificationItem> CleanupNotification;
	int32 ActivePageIndex = 0;
};
