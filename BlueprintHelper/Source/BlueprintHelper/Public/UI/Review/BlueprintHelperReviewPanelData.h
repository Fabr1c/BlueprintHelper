// BlueprintHelper Review panel data DTOs.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

enum class EBlueprintHelperReviewActionIntentKind : uint8
{
	Accept,
	Reject
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewRowBinding
{
	FString AssetPath;
	FString ChangeId;
	FString AtomicTargetId;
	FString TargetKey;
	EBlueprintHelperReviewSurface Surface = EBlueprintHelperReviewSurface::Unknown;

	bool IsValid() const;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewActionIntent
{
	EBlueprintHelperReviewActionIntentKind Action = EBlueprintHelperReviewActionIntentKind::Accept;
	FBlueprintHelperReviewRowBinding Binding;
	FString SourceWidget;

	static FBlueprintHelperReviewActionIntent Accept(
		const FBlueprintHelperReviewRowBinding& InBinding,
		const FString& InSourceWidget = FString());
	static FBlueprintHelperReviewActionIntent Reject(
		const FBlueprintHelperReviewRowBinding& InBinding,
		const FString& InSourceWidget = FString());
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewSurfaceDiffEntry
{
	FBlueprintHelperReviewRowBinding Binding;
	EBlueprintHelperReviewChangeKind ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
	bool bSelected = false;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewSurfaceDiffModel
{
	FString AssetPath;
	EBlueprintHelperReviewSurface Surface = EBlueprintHelperReviewSurface::Unknown;
	TMap<FString, FBlueprintHelperReviewSurfaceDiffEntry> EntriesByTargetKey;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewTransientActionState
{
	FString ChangeId;
	EBlueprintHelperReviewActionIntentKind Action = EBlueprintHelperReviewActionIntentKind::Accept;
	EBlueprintHelperReviewChangeStatus Status = EBlueprintHelperReviewChangeStatus::Pending;
	FString Message;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewPresenterErrorState
{
	FString ChangeId;
	EBlueprintHelperReviewChangeStatus Status = EBlueprintHelperReviewChangeStatus::NeedsAction;
	FString Message;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewPanelState
{
	TArray<FBlueprintHelperReviewVisibleChange> PendingChanges;
	TArray<FBlueprintHelperReviewSurfaceDiffModel> SurfaceDiffModels;
	FString SelectedChangeId;
	FString SelectedAssetPath;
	TMap<FString, FBlueprintHelperReviewTransientActionState> TransientActionStatesByChangeId;
	TMap<FString, FBlueprintHelperReviewPresenterErrorState> PresenterErrorStatesByChangeId;
};

struct FBlueprintHelperReviewPanelDataSnapshot
{
	TArray<FBlueprintHelperReviewVisibleChange> PendingChanges;
	FString SelectedChangeId;
	FString SelectedAssetPath;

	static FBlueprintHelperReviewPanelDataSnapshot FromSelection(
		const TArray<FBlueprintHelperReviewVisibleChange>& InPendingChanges,
		const FBlueprintHelperReviewVisibleChange& InSelectedChange);
};
