// BlueprintHelper Review panel v2 state service implementation.

#include "UI/Review/BlueprintHelperReviewPanelStateService.h"
#include "Shared/Review/BlueprintHelperReviewStatusUtils.h"

FBlueprintHelperReviewPanelState FBlueprintHelperReviewPanelStateService::BuildPanelState(
	const TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>>& ChangeItems,
	const TSharedPtr<FBlueprintHelperReviewVisibleChange>& SelectedChange)
{
	FBlueprintHelperReviewPanelState State;
	State.SelectedChangeId = SelectedChange.IsValid() ? SelectedChange->ChangeId : FString();
	State.SelectedAssetPath = SelectedChange.IsValid() ? SelectedChange->AssetPath : FString();

	TMap<FString, int32> ModelIndexByKey;
	for (const TSharedPtr<FBlueprintHelperReviewVisibleChange>& Item : ChangeItems)
	{
		if (!Item.IsValid())
		{
			continue;
		}
		if (!FBlueprintHelperReviewStatusUtils::IsOpenReviewStatus(Item->Status))
		{
			continue;
		}

		State.PendingChanges.Add(*Item);
		for (const FBlueprintHelperReviewAtomicTarget& Target : Item->AtomicTargets)
		{
			if (!FBlueprintHelperReviewStatusUtils::IsOpenReviewStatus(Target.Status))
			{
				continue;
			}

			const EBlueprintHelperReviewSurface Surface = Target.Surface;
			if (Surface == EBlueprintHelperReviewSurface::Unknown)
			{
				continue;
			}

			const FString AssetPath = Target.AssetPath.IsEmpty() ? Item->AssetPath : Target.AssetPath;
			const FString ModelKey = FString::Printf(
				TEXT("%s|%s"),
				*AssetPath,
				BlueprintHelperReviewSurfaceToString(Surface));
			int32* ExistingIndex = ModelIndexByKey.Find(ModelKey);
			if (!ExistingIndex)
			{
				FBlueprintHelperReviewSurfaceDiffModel Model;
				Model.AssetPath = AssetPath;
				Model.Surface = Surface;
				const int32 NewIndex = State.SurfaceDiffModels.Add(Model);
				ModelIndexByKey.Add(ModelKey, NewIndex);
				ExistingIndex = ModelIndexByKey.Find(ModelKey);
			}

			TArray<FString> Keys;
			AddTargetKey(Target.TargetKey, Keys);
			AddTargetKey(Target.PropertyPath, Keys);
			AddTargetKey(Target.ComponentPath, Keys);
			AddTargetKey(Target.DisplayLabel, Keys);
			if (Keys.Num() == 0)
			{
				AddTargetKey(Item->LocationKey, Keys);
				AddTargetKey(Item->DisplayLabel, Keys);
			}

			for (const FString& Key : Keys)
			{
				FBlueprintHelperReviewSurfaceDiffEntry Entry;
				Entry.Binding = MakeTargetBinding(*Item, Target, Surface, Key);
				Entry.ChangeKind = Item->ChangeKind;
				Entry.bSelected = SelectedChange.IsValid() && SelectedChange->ChangeId == Item->ChangeId;
				State.SurfaceDiffModels[*ExistingIndex].EntriesByTargetKey.Add(Key, Entry);
			}
		}
	}

	return State;
}

FBlueprintHelperReviewRowBinding FBlueprintHelperReviewPanelStateService::MakeChangeBinding(
	const FBlueprintHelperReviewVisibleChange& Change,
	EBlueprintHelperReviewSurface Surface,
	const FString& TargetKey)
{
	FBlueprintHelperReviewRowBinding Binding;
	Binding.AssetPath = Change.AssetPath;
	Binding.ChangeId = Change.ChangeId;
	Binding.Surface = Surface;
	Binding.TargetKey = TargetKey.IsEmpty() ? Change.LocationKey : TargetKey;
	Binding.AtomicTargetId = MakeAtomicTargetId(Change, nullptr, Surface, Binding.TargetKey);
	return Binding;
}

FBlueprintHelperReviewRowBinding FBlueprintHelperReviewPanelStateService::MakeTargetBinding(
	const FBlueprintHelperReviewVisibleChange& Change,
	const FBlueprintHelperReviewAtomicTarget& Target,
	EBlueprintHelperReviewSurface Surface,
	const FString& TargetKey)
{
	FBlueprintHelperReviewRowBinding Binding;
	Binding.AssetPath = Target.AssetPath.IsEmpty() ? Change.AssetPath : Target.AssetPath;
	Binding.ChangeId = Change.ChangeId;
	Binding.Surface = Surface;
	Binding.TargetKey = TargetKey.IsEmpty() ? Target.TargetKey : TargetKey;
	Binding.AtomicTargetId = MakeAtomicTargetId(Change, &Target, Surface, Binding.TargetKey);
	return Binding;
}

bool FBlueprintHelperReviewPanelStateService::TryFindChangeByIntent(
	const FBlueprintHelperReviewActionIntent& Intent,
	const TArray<FBlueprintHelperReviewVisibleChange>& PendingChanges,
	FBlueprintHelperReviewVisibleChange& OutChange)
{
	if (!Intent.Binding.IsValid())
	{
		return false;
	}

	for (const FBlueprintHelperReviewVisibleChange& Change : PendingChanges)
	{
		if (Change.ChangeId == Intent.Binding.ChangeId)
		{
			OutChange = Change;
			return true;
		}
	}
	return false;
}

void FBlueprintHelperReviewPanelStateService::SetTransientActionState(
	FBlueprintHelperReviewPanelState& State,
	const FString& ChangeId,
	EBlueprintHelperReviewActionIntentKind Action,
	EBlueprintHelperReviewChangeStatus Status,
	const FString& Message)
{
	if (ChangeId.IsEmpty())
	{
		return;
	}

	FBlueprintHelperReviewTransientActionState& TransientState =
		State.TransientActionStatesByChangeId.FindOrAdd(ChangeId);
	TransientState.ChangeId = ChangeId;
	TransientState.Action = Action;
	TransientState.Status = Status;
	TransientState.Message = Message;
}

void FBlueprintHelperReviewPanelStateService::ClearTransientActionState(
	FBlueprintHelperReviewPanelState& State,
	const FString& ChangeId)
{
	if (!ChangeId.IsEmpty())
	{
		State.TransientActionStatesByChangeId.Remove(ChangeId);
	}
}

bool FBlueprintHelperReviewPanelStateService::IsTransientActionInProgress(
	const FBlueprintHelperReviewPanelState& State,
	const FString& ChangeId)
{
	if (ChangeId.IsEmpty())
	{
		return false;
	}
	return State.TransientActionStatesByChangeId.Contains(ChangeId);
}

void FBlueprintHelperReviewPanelStateService::SetPresenterErrorState(
	FBlueprintHelperReviewPanelState& State,
	const FString& ChangeId,
	EBlueprintHelperReviewChangeStatus Status,
	const FString& Message)
{
	if (ChangeId.IsEmpty())
	{
		return;
	}

	FBlueprintHelperReviewPresenterErrorState& ErrorState =
		State.PresenterErrorStatesByChangeId.FindOrAdd(ChangeId);
	ErrorState.ChangeId = ChangeId;
	ErrorState.Status = Status;
	ErrorState.Message = Message;
}

void FBlueprintHelperReviewPanelStateService::ClearPresenterErrorState(
	FBlueprintHelperReviewPanelState& State,
	const FString& ChangeId)
{
	if (!ChangeId.IsEmpty())
	{
		State.PresenterErrorStatesByChangeId.Remove(ChangeId);
	}
}

void FBlueprintHelperReviewPanelStateService::CollectTargetKeysForSurface(
	const FBlueprintHelperReviewVisibleChange& Change,
	EBlueprintHelperReviewSurface Surface,
	TArray<FString>& OutKeys)
{
	for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
	{
		if (Target.Surface != Surface)
		{
			continue;
		}
		AddTargetKey(Target.TargetKey, OutKeys);
		AddTargetKey(Target.PropertyPath, OutKeys);
		AddTargetKey(Target.ComponentPath, OutKeys);
		AddTargetKey(Target.DisplayLabel, OutKeys);
	}
	AddTargetKey(Change.LocationKey, OutKeys);
	AddTargetKey(Change.DisplayLabel, OutKeys);
}

void FBlueprintHelperReviewPanelStateService::AddTargetKey(const FString& Key, TArray<FString>& OutKeys)
{
	FString Trimmed = Key;
	Trimmed.TrimStartAndEndInline();
	if (!Trimmed.IsEmpty())
	{
		OutKeys.AddUnique(Trimmed);
	}
}

FString FBlueprintHelperReviewPanelStateService::MakeAtomicTargetId(
	const FBlueprintHelperReviewVisibleChange& Change,
	const FBlueprintHelperReviewAtomicTarget* Target,
	EBlueprintHelperReviewSurface Surface,
	const FString& TargetKey)
{
	if (Target)
	{
		if (!Target->LatestEvidenceId.IsEmpty() && Target->AtomicIndex != INDEX_NONE)
		{
			return FString::Printf(TEXT("%s:%d"), *Target->LatestEvidenceId, Target->AtomicIndex);
		}
		if (!Target->VisualGroupKey.IsEmpty())
		{
			return Target->VisualGroupKey;
		}
	}

	return FString::Printf(
		TEXT("%s|%s|%s"),
		*Change.ChangeId,
		BlueprintHelperReviewSurfaceToString(Surface),
		*TargetKey);
}
