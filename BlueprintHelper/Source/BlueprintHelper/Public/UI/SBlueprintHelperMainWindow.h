// BlueprintHelper main window shell.

#pragma once

#include "CoreMinimal.h"
#include "Systems/Review/BlueprintHelperReviewTargetValidityTypes.h"
#include "UI/BlueprintHelperUiSettings.h"
#include "Widgets/SCompoundWidget.h"

class FBlueprintHelperGraphResolver;
class FBlueprintHelperImportService;
class FBlueprintHelperMainWindowPresenter;
class FBlueprintHelperReviewActionService;
class FBlueprintHelperReviewStoreService;
class FBlueprintHelperReviewValiditySweepCoordinator;
class SBlueprintHelperMetricsPanel;
class SBlueprintHelperReviewPanel;
class SBox;
class SNotificationItem;
class SWidget;
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
	~SBlueprintHelperMainWindow();
	static void FlushCleanupTasks();
	static void ShutdownCleanupTasks();

#if WITH_DEV_AUTOMATION_TESTS
	void ShowReviewPageForTesting();
	TSharedPtr<SBlueprintHelperReviewPanel> GetReviewPanelForTesting() const;
#endif

private:
	FReply ShowToolsPage();
	FReply ShowReviewPage();
	FReply ShowLayoutPage();
	FReply ShowSettingsPage();
	FReply ShowMetricsPage();
	FReply OnCleanupReviewDataClicked();
	void ShowPage(int32 PageIndex);
	void EnsurePageConstructed(int32 PageIndex);
	TSharedRef<SWidget> BuildToolsPage();
	TSharedRef<SWidget> BuildReviewPage();
	TSharedRef<SWidget> BuildLayoutPage();
	TSharedRef<SWidget> BuildSettingsPage();
	TSharedRef<SWidget> BuildMetricsPage();
	void HandleMainWindowPresenterEvent(const FBlueprintHelperMainWindowPresenterEvent& Event);
	void HandleReviewValidityCandidatesReady(
		const FString& Source,
		const TArray<FBlueprintHelperReviewValidityCandidate>& Candidates);
	void ShowCleanupNotification(const FString& StatusText);
	void UpdateCleanupNotification(const FString& StatusText, bool bSucceeded, bool bExpire);
	int32 ResolveDefaultTabIndex() const;
	FSlateColor GetTabColor(int32 PageIndex) const;
	FSlateColor GetToolsTabColor() const;
	FSlateColor GetReviewTabColor() const;
	FSlateColor GetLayoutTabColor() const;
	FSlateColor GetSettingsTabColor() const;
	FSlateColor GetMetricsTabColor() const;

	const FBlueprintHelperImportService* ImportService = nullptr;
	const FBlueprintHelperGraphResolver* GraphResolver = nullptr;
	const FBlueprintHelperReviewStoreService* ReviewStoreService = nullptr;
	const FBlueprintHelperReviewActionService* ReviewActionService = nullptr;
	TSharedPtr<FBlueprintHelperMainWindowPresenter> MainWindowPresenter;
	TSharedPtr<FBlueprintHelperReviewValiditySweepCoordinator> ReviewValiditySweepCoordinator;
	TSharedPtr<SBox> PageHost;
	TArray<TSharedPtr<SWidget>> ConstructedPages;
	TWeakPtr<SBlueprintHelperReviewPanel> ReviewPanelWidget;
	TWeakPtr<SBlueprintHelperMetricsPanel> MetricsPanelWidget;
	FBlueprintHelperMainWindowSettings MainWindowSettings;
	FBlueprintHelperNotificationSettings NotificationSettings;
	FBlueprintHelperReviewPerformanceSettings ReviewPerformanceSettings;
	TWeakPtr<SNotificationItem> CleanupNotification;
	int32 ActivePageIndex = 0;
};
