// BlueprintHelper main window shell.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FBlueprintHelperGraphResolver;
class FBlueprintHelperImportService;
class FBlueprintHelperReviewActionService;
class FBlueprintHelperReviewStoreService;
class SWidgetSwitcher;

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
	FReply OnCleanupReviewDataClicked();
	FSlateColor GetToolsTabColor() const;
	FSlateColor GetReviewTabColor() const;

	const FBlueprintHelperImportService* ImportService = nullptr;
	const FBlueprintHelperGraphResolver* GraphResolver = nullptr;
	const FBlueprintHelperReviewStoreService* ReviewStoreService = nullptr;
	const FBlueprintHelperReviewActionService* ReviewActionService = nullptr;
	TSharedPtr<SWidgetSwitcher> PageSwitcher;
	FString LastCleanupStatus;
	bool bCleanupInProgress = false;
	int32 ActivePageIndex = 0;
};
