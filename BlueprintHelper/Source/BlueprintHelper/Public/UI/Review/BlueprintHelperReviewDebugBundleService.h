// BlueprintHelper ReviewPanel debug bundle service.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;
struct FBlueprintHelperReviewVisibleChange;

class BLUEPRINTHELPER_API FBlueprintHelperReviewDebugBundleService
{
public:
	static FString GetDebugRootDir();
	static FString GetReviewPanelBundleDir();
	static FString MakeDefaultBundlePath();
	static FString NormalizeBundlePath(const FString& InPath);
	static bool IsPathInsideDebugRoot(const FString& Path);

	static TSharedRef<FJsonObject> BuildLogEvent(
		const FString& SessionId,
		const FString& Message,
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& SelectedChange,
		const FString& AssetPath);

	static TSharedRef<FJsonObject> BuildFocusEvent(
		const FString& SessionId,
		const FString& Phase,
		int32 Index,
		int32 Count,
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Change,
		const FString& AssetPath,
		const FString& Reason = FString());

	static TSharedRef<FJsonObject> BuildActionHashGuardEvent(
		const FString& SessionId,
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Change,
		const FString& AssetPath,
		const FString& TargetKey,
		const FString& ExpectedHash,
		const FString& CurrentHash,
		const FString& CurrentSnapshotJson,
		const FString& RecordedAfterSnapshotJson);

	static bool AppendEvent(
		const FString& BundlePath,
		const FString& SessionId,
		const TSharedRef<FJsonObject>& Event,
		FString* OutError = nullptr);
	static void FlushAsyncWrites();
	static void ShutdownAsyncWrites();

	static bool LoadBundleText(
		const FString& BundlePath,
		FString& OutText,
		FString* OutError = nullptr);

	static bool LoadBundleSummaryText(
		const FString& BundlePath,
		FString& OutSummaryText,
		FString* OutError = nullptr);

private:
	static void SetError(FString* OutError, const FString& Error);
	static TSharedRef<FJsonObject> CreateEmptyBundle(const FString& SessionId);
	static bool LoadOrCreateBundle(
		const FString& BundlePath,
		const FString& SessionId,
		TSharedPtr<FJsonObject>& OutBundle,
		FString* OutError);
	static bool SaveBundle(
		const FString& BundlePath,
		const TSharedRef<FJsonObject>& Bundle,
		FString* OutError);
	static TSharedRef<FJsonObject> BuildChangeSummary(
		const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Change);
};
