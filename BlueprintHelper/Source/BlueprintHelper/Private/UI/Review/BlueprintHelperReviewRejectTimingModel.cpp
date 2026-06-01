// BlueprintHelper ReviewPanel reject timing model implementation.

#include "UI/Review/BlueprintHelperReviewRejectTimingModel.h"

void FBlueprintHelperReviewRejectTimingModel::Begin(
	const FBlueprintHelperReviewVisibleChange& Change,
	double NowSeconds)
{
	if (Change.ChangeId.IsEmpty())
	{
		return;
	}

	FState State;
	State.ChangeId = Change.ChangeId;
	State.AssetPath = Change.AssetPath;
	State.ChangeSnapshot = Change;
	State.StartedAtSeconds = NowSeconds;
	State.LastStageAtSeconds = NowSeconds;
	StatesByChangeId.Add(Change.ChangeId, MoveTemp(State));
}

bool FBlueprintHelperReviewRejectTimingModel::Contains(const FString& ChangeId) const
{
	return StatesByChangeId.Contains(ChangeId);
}

bool FBlueprintHelperReviewRejectTimingModel::IsWaitingForStoreRefresh(const FString& ChangeId) const
{
	const FState* State = StatesByChangeId.Find(ChangeId);
	return State && State->bWaitingForStoreRefresh;
}

void FBlueprintHelperReviewRejectTimingModel::MarkWaitingForStoreRefresh(const FString& ChangeId)
{
	if (FState* State = StatesByChangeId.Find(ChangeId))
	{
		State->bWaitingForStoreRefresh = true;
	}
}

void FBlueprintHelperReviewRejectTimingModel::CancelWaitingForStoreRefresh(const FString& ChangeId)
{
	if (FState* State = StatesByChangeId.Find(ChangeId))
	{
		State->bWaitingForStoreRefresh = false;
	}
}

void FBlueprintHelperReviewRejectTimingModel::Complete(const FString& ChangeId)
{
	StatesByChangeId.Remove(ChangeId);
}

FBlueprintHelperReviewRejectTimingSample FBlueprintHelperReviewRejectTimingModel::RecordStage(
	const FString& ChangeId,
	const FString& Stage,
	double NowSeconds,
	const FString& Detail)
{
	FState* State = StatesByChangeId.Find(ChangeId);
	if (!State)
	{
		return FBlueprintHelperReviewRejectTimingSample();
	}
	return MakeSample(*State, Stage, NowSeconds, Detail);
}

TArray<FBlueprintHelperReviewRejectTimingSample>
FBlueprintHelperReviewRejectTimingModel::RecordMatchingStoreEvent(
	const FBlueprintHelperReviewStoreChangedEvent& Event,
	const FString& Stage,
	double NowSeconds,
	const FString& Detail,
	bool bCompleteMatches)
{
	TArray<FBlueprintHelperReviewRejectTimingSample> Samples;
	TArray<FString> CompletedChangeIds;
	for (TPair<FString, FState>& Pair : StatesByChangeId)
	{
		if (!Pair.Value.bWaitingForStoreRefresh || !EventMatchesState(Event, Pair.Value))
		{
			continue;
		}
		Samples.Add(MakeSample(Pair.Value, Stage, NowSeconds, Detail));
		if (bCompleteMatches)
		{
			CompletedChangeIds.Add(Pair.Key);
		}
	}
	for (const FString& ChangeId : CompletedChangeIds)
	{
		StatesByChangeId.Remove(ChangeId);
	}
	return Samples;
}

bool FBlueprintHelperReviewRejectTimingModel::EventMatchesState(
	const FBlueprintHelperReviewStoreChangedEvent& Event,
	const FState& State)
{
	if (Event.bRequiresFullReload)
	{
		return true;
	}
	const bool bChangeMatches = Event.ChangeIds.Num() == 0
		|| Event.ChangeIds.Contains(State.ChangeId);
	const bool bAssetMatches = Event.AssetPaths.Num() == 0
		|| Event.AssetPaths.Contains(State.AssetPath);
	return bChangeMatches && bAssetMatches;
}

FBlueprintHelperReviewRejectTimingSample FBlueprintHelperReviewRejectTimingModel::MakeSample(
	FState& State,
	const FString& Stage,
	double NowSeconds,
	const FString& Detail)
{
	FBlueprintHelperReviewRejectTimingSample Sample;
	Sample.bValid = true;
	Sample.Stage = Stage;
	Sample.ChangeId = State.ChangeId;
	Sample.AssetPath = State.AssetPath;
	Sample.Detail = Detail;
	Sample.StageMs = (NowSeconds - State.LastStageAtSeconds) * 1000.0;
	Sample.TotalMs = (NowSeconds - State.StartedAtSeconds) * 1000.0;
	Sample.bHasChangeSnapshot = true;
	Sample.ChangeSnapshot = State.ChangeSnapshot;
	State.LastStageAtSeconds = NowSeconds;
	return Sample;
}
