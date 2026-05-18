// BlueprintHelper Review BlueprintHelperReviewStoreMergeUtils implementation.

#include "Systems/Review/Utils/BlueprintHelperReviewStoreMergeUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Shared/Review/BlueprintHelperReviewEnumUtils.h"
#include "Shared/Review/BlueprintHelperReviewStatusUtils.h"
#include "Shared/Review/BlueprintHelperReviewTargetKindRegistry.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentEvidence.h"
#include "Systems/Review/Utils/BlueprintHelperReviewStoreTargetUtils.h"

FString FBlueprintHelperReviewStoreMergeUtils::MakeVisibleChangeScopeIdentity(const FBlueprintHelperReviewVisibleChange& Change)
	{
		if (!Change.ScopeIdentity.IsEmpty())
		{
			return Change.ScopeIdentity;
		}
		for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			const FString ScopeIdentity = FBlueprintHelperReviewStoreTargetUtils::MakeReviewScopeIdentity(Target, Change.LocationKey);
			if (!ScopeIdentity.IsEmpty())
			{
				return ScopeIdentity;
			}
		}
		return Change.LocationKey;
	}
FString FBlueprintHelperReviewStoreMergeUtils::MakeLoadedVisibleChangeCollapseKey(const FBlueprintHelperReviewVisibleChange& Change)
	{
		const FString AssetKey = FBlueprintHelperReviewStoreTargetUtils::MakeReviewAssetLinkKey(Change.AssetPath);
		const FString RootPrefix = Change.bIsAssetLifecycleRoot ? TEXT("root") : TEXT("change");
		const FString ScopeIdentity = MakeVisibleChangeScopeIdentity(Change);
		if (!ScopeIdentity.IsEmpty())
		{
			return FString::Printf(
				TEXT("%s|%s|%s"),
				*RootPrefix,
				*AssetKey,
				*ScopeIdentity);
		}

		return FString::Printf(
			TEXT("%s|%s|%s|%s"),
			*RootPrefix,
			*AssetKey,
			*Change.GraphName,
			*Change.ChangeId);
	}
void FBlueprintHelperReviewStoreMergeUtils::AddUniqueReviewStrings(TArray<FString>& Target, const TArray<FString>& Source)
	{
		for (const FString& Value : Source)
		{
			if (!Value.IsEmpty())
			{
				Target.AddUnique(Value);
			}
		}
	}
void FBlueprintHelperReviewStoreMergeUtils::MergeReviewAtomicTargetsLatestWins(
		TArray<FBlueprintHelperReviewAtomicTarget>& ExistingTargets,
		const TArray<FBlueprintHelperReviewAtomicTarget>& IncomingTargets)
	{
		for (const FBlueprintHelperReviewAtomicTarget& IncomingTarget : IncomingTargets)
		{
			const FString IncomingKey = IncomingTarget.ScopeIdentity.IsEmpty()
				? FBlueprintHelperReviewStoreTargetUtils::MakeReviewAtomicLookupKey(IncomingTarget, FString())
				: IncomingTarget.ScopeIdentity;
			bool bReplaced = false;
			for (FBlueprintHelperReviewAtomicTarget& ExistingTarget : ExistingTargets)
			{
				const FString ExistingKey = ExistingTarget.ScopeIdentity.IsEmpty()
					? FBlueprintHelperReviewStoreTargetUtils::MakeReviewAtomicLookupKey(ExistingTarget, FString())
					: ExistingTarget.ScopeIdentity;
				if (!IncomingKey.IsEmpty() && ExistingKey == IncomingKey)
				{
					FBlueprintHelperReviewAtomicTarget MergedTarget = IncomingTarget;
					FBlueprintHelperReviewStoreTargetUtils::PreserveFirstBaselineFields(MergedTarget, ExistingTarget, IncomingTarget);
					ExistingTarget = MergedTarget;
					bReplaced = true;
					break;
				}
			}
			if (!bReplaced)
			{
				ExistingTargets.Add(IncomingTarget);
			}
		}
	}
void FBlueprintHelperReviewStoreMergeUtils::MergeVisibleChangeLatestWins(
		FBlueprintHelperReviewVisibleChange& Existing,
		const FBlueprintHelperReviewVisibleChange& Incoming)
	{
		TArray<FString> SourceTransactionIds = Existing.SourceTransactionIds;
		TArray<FString> LatestTransactionIds = Existing.LatestTransactionIds;
		TArray<FBlueprintHelperReviewAtomicTarget> AtomicTargets = Existing.AtomicTargets;
		const FString ScopeIdentity = Existing.ScopeIdentity.IsEmpty() ? Incoming.ScopeIdentity : Existing.ScopeIdentity;

		AddUniqueReviewStrings(SourceTransactionIds, Incoming.SourceTransactionIds);
		AddUniqueReviewStrings(LatestTransactionIds, Incoming.LatestTransactionIds);
		MergeReviewAtomicTargetsLatestWins(AtomicTargets, Incoming.AtomicTargets);

		Existing = Incoming;
		Existing.ScopeIdentity = ScopeIdentity;
		Existing.SourceTransactionIds = SourceTransactionIds;
		Existing.LatestTransactionIds = LatestTransactionIds;
		Existing.AtomicTargets = AtomicTargets;
		FBlueprintHelperReviewStoreTargetUtils::ApplyAssetLifecycleRootMetadata(Existing);
	}
void FBlueprintHelperReviewStoreMergeUtils::RemoveNetNoChangeVisibleChanges(TArray<FBlueprintHelperReviewVisibleChange>& Changes)
	{
		for (int32 ChangeIndex = Changes.Num() - 1; ChangeIndex >= 0; --ChangeIndex)
		{
			FBlueprintHelperReviewVisibleChange& Change = Changes[ChangeIndex];
			const bool bHadAtomicTargets = Change.AtomicTargets.Num() > 0;
			for (int32 TargetIndex = Change.AtomicTargets.Num() - 1; TargetIndex >= 0; --TargetIndex)
			{
				if (FBlueprintHelperReviewStoreTargetUtils::IsReviewTargetNetNoChange(Change.AtomicTargets[TargetIndex]))
				{
					Change.AtomicTargets.RemoveAt(TargetIndex);
				}
			}

			const bool bChangeHashNoChange = !Change.BeforeHash.IsEmpty()
				&& !Change.AfterHash.IsEmpty()
				&& Change.BeforeHash == Change.AfterHash;
			if ((bHadAtomicTargets && Change.AtomicTargets.Num() == 0) || bChangeHashNoChange)
			{
				Changes.RemoveAt(ChangeIndex);
			}
		}
	}
void FBlueprintHelperReviewStoreMergeUtils::CollapseVisibleChangesLatestWins(TArray<FBlueprintHelperReviewVisibleChange>& Changes)
	{
		TArray<FBlueprintHelperReviewVisibleChange> Collapsed;
		TMap<FString, int32> ExistingIndexByKey;
		for (const FBlueprintHelperReviewVisibleChange& Change : Changes)
		{
			const FString CollapseKey = MakeLoadedVisibleChangeCollapseKey(Change);
			if (int32* ExistingIndex = ExistingIndexByKey.Find(CollapseKey))
			{
				MergeVisibleChangeLatestWins(Collapsed[*ExistingIndex], Change);
				continue;
			}

			ExistingIndexByKey.Add(CollapseKey, Collapsed.Num());
			Collapsed.Add(Change);
		}

		Changes = MoveTemp(Collapsed);
		RemoveNetNoChangeVisibleChanges(Changes);
	}
void FBlueprintHelperReviewStoreMergeUtils::MergeReviewRecord(FBlueprintHelperReviewRecord& Existing, const FBlueprintHelperReviewRecord& Incoming)
	{
		for (const FString& TaskRunId : Incoming.SourceTaskRunIds)
		{
			Existing.SourceTaskRunIds.AddUnique(TaskRunId);
		}
		for (const FString& DebugCaseId : Incoming.DebugCaseIds)
		{
			Existing.DebugCaseIds.AddUnique(DebugCaseId);
		}

		for (const FBlueprintHelperReviewVisibleChange& IncomingChange : Incoming.VisibleChanges)
		{
			FBlueprintHelperReviewVisibleChange* ExistingChange = Existing.VisibleChanges.FindByPredicate(
				[&IncomingChange](const FBlueprintHelperReviewVisibleChange& Candidate)
				{
					return FBlueprintHelperReviewStoreMergeUtils::MakeLoadedVisibleChangeCollapseKey(Candidate)
						== FBlueprintHelperReviewStoreMergeUtils::MakeLoadedVisibleChangeCollapseKey(IncomingChange);
				});
			if (!ExistingChange)
			{
				Existing.VisibleChanges.Add(IncomingChange);
				continue;
			}

			for (const FBlueprintHelperReviewAtomicTarget& IncomingTarget : IncomingChange.AtomicTargets)
			{
				FBlueprintHelperReviewAtomicTarget* ExistingTarget = nullptr;
				if (!IncomingTarget.TargetKey.IsEmpty())
				{
					const FString IncomingTargetScopeIdentity = IncomingTarget.ScopeIdentity.IsEmpty()
						? FBlueprintHelperReviewStoreTargetUtils::MakeReviewAtomicLookupKey(IncomingTarget, FString())
						: IncomingTarget.ScopeIdentity;
					ExistingTarget = ExistingChange->AtomicTargets.FindByPredicate(
						[&IncomingTargetScopeIdentity](const FBlueprintHelperReviewAtomicTarget& Candidate)
						{
							const FString CandidateScopeIdentity = Candidate.ScopeIdentity.IsEmpty()
								? FBlueprintHelperReviewStoreTargetUtils::MakeReviewAtomicLookupKey(Candidate, FString())
								: Candidate.ScopeIdentity;
							return !IncomingTargetScopeIdentity.IsEmpty() && CandidateScopeIdentity == IncomingTargetScopeIdentity;
						});
				}

				if (!ExistingTarget)
				{
					ExistingChange->AtomicTargets.Add(IncomingTarget);
					continue;
				}

				TArray<FString> SourceTransactionIds = ExistingTarget->SourceTransactionIds;
				for (const FString& SourceTransactionId : IncomingTarget.SourceTransactionIds)
				{
					SourceTransactionIds.AddUnique(SourceTransactionId);
				}
				FBlueprintHelperReviewAtomicTarget MergedTarget = IncomingTarget;
				FBlueprintHelperReviewStoreTargetUtils::PreserveFirstBaselineFields(MergedTarget, *ExistingTarget, IncomingTarget);
				*ExistingTarget = MergedTarget;
				ExistingTarget->SourceTransactionIds = SourceTransactionIds;
			}

			for (const FString& SourceTransactionId : IncomingChange.SourceTransactionIds)
			{
				ExistingChange->SourceTransactionIds.AddUnique(SourceTransactionId);
			}
			for (const FString& LatestTransactionId : IncomingChange.LatestTransactionIds)
			{
				ExistingChange->LatestTransactionIds.AddUnique(LatestTransactionId);
			}
			ExistingChange->LatestTransactionId = IncomingChange.LatestTransactionId;
			ExistingChange->ChangeId = IncomingChange.ChangeId;
			ExistingChange->ChangeKind = IncomingChange.ChangeKind;
			ExistingChange->AfterSummary = IncomingChange.AfterSummary;
			ExistingChange->ParentChangeId = IncomingChange.ParentChangeId;
			ExistingChange->ExecutionOrder = IncomingChange.ExecutionOrder;
			ExistingChange->TaskStepIndex = IncomingChange.TaskStepIndex;
			ExistingChange->AtomicIndex = IncomingChange.AtomicIndex;
			ExistingChange->bIsAssetLifecycleRoot = IncomingChange.bIsAssetLifecycleRoot;
			ExistingChange->bRejectRemovesChildren = IncomingChange.bRejectRemovesChildren;
			if (IncomingChange.Status == EBlueprintHelperReviewChangeStatus::NeedsAction
				|| IncomingChange.Status == EBlueprintHelperReviewChangeStatus::RejectFailed)
			{
				ExistingChange->Status = IncomingChange.Status;
				ExistingChange->NeedsActionReason = IncomingChange.NeedsActionReason;
			}
			FBlueprintHelperReviewStoreTargetUtils::ApplyAssetLifecycleRootMetadata(*ExistingChange);
		}

		for (const FString& TaskRunId : Incoming.SourceTransactionSummary.TaskRunIds)
		{
			Existing.SourceTransactionSummary.TaskRunIds.AddUnique(TaskRunId);
		}
		for (const FString& OperationKind : Incoming.SourceTransactionSummary.OperationKinds)
		{
			Existing.SourceTransactionSummary.OperationKinds.AddUnique(OperationKind);
		}
		for (const FString& AssetPath : Incoming.SourceTransactionSummary.AssetPaths)
		{
			Existing.SourceTransactionSummary.AssetPaths.AddUnique(AssetPath);
		}
		for (const FString& TransactionId : Incoming.SourceTransactionSummary.TransactionIds)
		{
			Existing.SourceTransactionSummary.TransactionIds.AddUnique(TransactionId);
		}
		if (!Incoming.SourceTransactionSummary.CreatedAtFirst.IsEmpty()
			&& (Existing.SourceTransactionSummary.CreatedAtFirst.IsEmpty()
				|| Incoming.SourceTransactionSummary.CreatedAtFirst < Existing.SourceTransactionSummary.CreatedAtFirst))
		{
			Existing.SourceTransactionSummary.CreatedAtFirst = Incoming.SourceTransactionSummary.CreatedAtFirst;
		}
		if (!Incoming.SourceTransactionSummary.CreatedAtLast.IsEmpty()
			&& (Existing.SourceTransactionSummary.CreatedAtLast.IsEmpty()
				|| Incoming.SourceTransactionSummary.CreatedAtLast > Existing.SourceTransactionSummary.CreatedAtLast))
		{
			Existing.SourceTransactionSummary.CreatedAtLast = Incoming.SourceTransactionSummary.CreatedAtLast;
		}
		Existing.SourceTransactionSummary.TransactionCount =
			Existing.SourceTransactionSummary.TransactionIds.Num();

		if (Incoming.Status == EBlueprintHelperReviewChangeStatus::NeedsAction
			|| Incoming.Status == EBlueprintHelperReviewChangeStatus::RejectFailed)
		{
			Existing.Status = Incoming.Status;
		}
		Existing.SourceTransactionSummary.FinalReviewStatus = Existing.Status;
	}
