// BlueprintHelper main window presenter event bridge.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"

class FBlueprintHelperReviewStoreService;

enum class EBlueprintHelperMainWindowVisualEventType : uint8
{
	CleanupReviewDataClicked
};

struct FBlueprintHelperMainWindowVisualEvent
{
	EBlueprintHelperMainWindowVisualEventType Type =
		EBlueprintHelperMainWindowVisualEventType::CleanupReviewDataClicked;

	static FBlueprintHelperMainWindowVisualEvent CleanupReviewDataClicked();
};

struct FBlueprintHelperMainWindowPresenterEvent
{
	bool bShowCleanupNotification = false;
	bool bUpdateCleanupNotification = false;
	bool bCleanupSucceeded = false;
	bool bExpireCleanupNotification = false;
	bool bLogAsWarning = false;
	FString CleanupStatusText;
	FString LogMessage;

	static FBlueprintHelperMainWindowPresenterEvent ShowCleanupNotification(const FString& InStatusText);
	static FBlueprintHelperMainWindowPresenterEvent UpdateCleanupNotification(
		const FString& InStatusText,
		bool bInSucceeded,
		bool bInExpire);
	static FBlueprintHelperMainWindowPresenterEvent LogStatus(
		const FString& InLogMessage,
		bool bInLogAsWarning = false);
};

class FBlueprintHelperMainWindowPresenter : public TSharedFromThis<FBlueprintHelperMainWindowPresenter>
{
public:
	using FPresenterEventSink = TFunction<void(const FBlueprintHelperMainWindowPresenterEvent&)>;

	explicit FBlueprintHelperMainWindowPresenter(const FBlueprintHelperReviewStoreService* InReviewStoreService);

	void SetEventSink(FPresenterEventSink InEventSink);
	FReply HandleVisualEvent(const FBlueprintHelperMainWindowVisualEvent& Event);
	const FString& GetLastCleanupStatus() const;

private:
	FReply HandleCleanupReviewDataClicked();
	void EmitEvent(const FBlueprintHelperMainWindowPresenterEvent& Event) const;
	void EmitLogStatus(const FString& InLogMessage, bool bInLogAsWarning = false) const;

	const FBlueprintHelperReviewStoreService* ReviewStoreService = nullptr;
	FPresenterEventSink EventSink;
	FString LastCleanupStatus;
	bool bCleanupInProgress = false;
};
