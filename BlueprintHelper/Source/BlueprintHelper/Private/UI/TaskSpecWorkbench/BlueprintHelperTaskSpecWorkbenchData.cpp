// BlueprintHelper TaskSpec / ReadContext workbench data models.

#include "UI/TaskSpecWorkbench/BlueprintHelperTaskSpecWorkbenchData.h"

const FBlueprintHelperTaskSpecWorkbenchSnapshot& FBlueprintHelperTaskSpecWorkbenchStore::GetSnapshot() const
{
	return Snapshot;
}

void FBlueprintHelperTaskSpecWorkbenchStore::ReplaceSnapshot(
	const FBlueprintHelperTaskSpecWorkbenchSnapshot& InSnapshot)
{
	Snapshot = InSnapshot;
	DataChanged.Broadcast();
}

void FBlueprintHelperTaskSpecWorkbenchStore::SelectCandidate(
	const FString& CardId,
	const FString& CandidateId)
{
	Snapshot.SelectedCardId = CardId;
	Snapshot.SelectedCandidateId = CandidateId;

	for (FBlueprintHelperCallFunctionCardModel& Card : Snapshot.CandidateCards)
	{
		const bool bCardSelected = Card.CardId == CardId;
		for (FBlueprintHelperCallFunctionCandidateRowModel& Candidate : Card.Candidates)
		{
			Candidate.bSelected = bCardSelected && Candidate.CandidateId == CandidateId;
		}
	}

	FString SelectedSourcePath;
	for (const FBlueprintHelperCallFunctionCardModel& Card : Snapshot.CandidateCards)
	{
		if (Card.CardId == CardId)
		{
			SelectedSourcePath = Card.SourcePath;
			break;
		}
	}

	for (FBlueprintHelperTaskSpecPreviewBlock& Block : Snapshot.Preview.Blocks)
	{
		Block.bSelected = !SelectedSourcePath.IsEmpty() && Block.SourcePath == SelectedSourcePath;
	}

	DataChanged.Broadcast();
}

FBlueprintHelperTaskSpecWorkbenchDataChanged& FBlueprintHelperTaskSpecWorkbenchStore::OnDataChanged()
{
	return DataChanged;
}
