// BlueprintHelper Review BlueprintHelperReviewActionTargetUtils implementation.

#include "Systems/Review/Utils/BlueprintHelperReviewActionTargetUtils.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ActorComponent.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "DataTableEditorUtils.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphUtilities.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "HAL/FileManager.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/StructureEditorUtils.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Shared/Review/BlueprintHelperReviewStatusUtils.h"
#include "Shared/Review/BlueprintHelperReviewTargetKindRegistry.h"
#include "Systems/Debug/BlueprintHelperDebugCaseStoreService.h"
#include "Systems/Debug/BlueprintHelperDebugEntryService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Systems/Review/Utils/BlueprintHelperReviewStoreTargetUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/ToolClusters/ObjectProperty/BlueprintHelperPropertyReflectionService.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "WidgetBlueprint.h"

using FPersistedReviewTargetMatch = FBlueprintHelperReviewActionTargetUtils::FPersistedReviewTargetMatch;

TArray<FString> FBlueprintHelperReviewActionTargetUtils::CollectPendingTargetKeys(const FBlueprintHelperReviewRecord& Record)
	{
		TArray<FString> TargetKeys;
		for (const FBlueprintHelperReviewVisibleChange& Change : Record.VisibleChanges)
		{
			for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
			{
				if (Target.Status == EBlueprintHelperReviewChangeStatus::Pending)
				{
					TargetKeys.AddUnique(Target.TargetKey);
				}
			}
		}
		return TargetKeys;
	}
TArray<FString> FBlueprintHelperReviewActionTargetUtils::CollectTargetKeysFromVisibleChange(const FBlueprintHelperReviewVisibleChange& Change)
	{
		TArray<FString> TargetKeys;
		for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			if (!Target.TargetKey.IsEmpty())
			{
				TargetKeys.AddUnique(Target.TargetKey);
			}
		}
		return TargetKeys;
	}
TArray<FString> FBlueprintHelperReviewActionTargetUtils::CollectScopeIdentitiesFromVisibleChange(const FBlueprintHelperReviewVisibleChange& Change)
	{
		TArray<FString> ScopeIdentities;
		if (!Change.ScopeIdentity.IsEmpty())
		{
			ScopeIdentities.AddUnique(Change.ScopeIdentity);
		}
		for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
		{
			const FString ScopeIdentity = FBlueprintHelperReviewStoreTargetUtils::MakeReviewScopeIdentity(Target, FString());
			if (!ScopeIdentity.IsEmpty())
			{
				ScopeIdentities.AddUnique(ScopeIdentity);
			}
		}
		return ScopeIdentities;
	}
FString FBlueprintHelperReviewActionTargetUtils::MakeReviewPackageKey(FString AssetPath)
	{
		AssetPath.TrimStartAndEndInline();
		if (AssetPath.IsEmpty())
		{
			return FString();
		}
		if (FPackageName::IsValidObjectPath(AssetPath))
		{
			return FPackageName::ObjectPathToPackageName(AssetPath);
		}

		int32 SubObjectIndex = INDEX_NONE;
		if (AssetPath.FindChar(TEXT(':'), SubObjectIndex))
		{
			AssetPath = AssetPath.Left(SubObjectIndex);
		}

		int32 ObjectIndex = INDEX_NONE;
		if (AssetPath.FindChar(TEXT('.'), ObjectIndex))
		{
			AssetPath = AssetPath.Left(ObjectIndex);
		}
		return AssetPath;
	}
bool FBlueprintHelperReviewActionTargetUtils::ReviewAssetPathMatches(const FString& Left, const FString& Right)
	{
		const FString LeftKey = MakeReviewPackageKey(Left);
		const FString RightKey = MakeReviewPackageKey(Right);
		return !LeftKey.IsEmpty() && !RightKey.IsEmpty() && LeftKey == RightKey;
	}
bool FBlueprintHelperReviewActionTargetUtils::IntersectTargetKeys(
		const TArray<FString>& RequestedTargetKeys,
		const TArray<FString>& CandidateTargetKeys,
		TArray<FString>& OutMatchedTargetKeys)
	{
		OutMatchedTargetKeys.Reset();
		for (const FString& CandidateTargetKey : CandidateTargetKeys)
		{
			if (!CandidateTargetKey.IsEmpty() && RequestedTargetKeys.Contains(CandidateTargetKey))
			{
				OutMatchedTargetKeys.AddUnique(CandidateTargetKey);
			}
		}
		return OutMatchedTargetKeys.Num() > 0;
	}
void FBlueprintHelperReviewActionTargetUtils::AddPersistedReviewTargetMatch(
		TArray<FPersistedReviewTargetMatch>& Matches,
		const FString& ReviewRecordId,
		const TArray<FString>& TargetKeys)
	{
		if (ReviewRecordId.IsEmpty() || TargetKeys.Num() == 0)
		{
			return;
		}

		FPersistedReviewTargetMatch* Existing = Matches.FindByPredicate(
			[&ReviewRecordId](const FPersistedReviewTargetMatch& Candidate)
			{
				return Candidate.ReviewRecordId == ReviewRecordId;
			});
		if (!Existing)
		{
			FPersistedReviewTargetMatch NewMatch;
			NewMatch.ReviewRecordId = ReviewRecordId;
			Matches.Add(NewMatch);
			Existing = &Matches.Last();
		}

		for (const FString& TargetKey : TargetKeys)
		{
			if (!TargetKey.IsEmpty())
			{
				Existing->TargetKeys.AddUnique(TargetKey);
			}
		}
	}
TArray<FPersistedReviewTargetMatch> FBlueprintHelperReviewActionTargetUtils::ResolvePersistedReviewTargetMatches(
		const FBlueprintHelperReviewVisibleChange& Change)
	{
		TArray<FPersistedReviewTargetMatch> Matches;
		if (Change.AssetPath.IsEmpty())
		{
			return Matches;
		}

		const TArray<FString> RequestedTargetKeys = CollectTargetKeysFromVisibleChange(Change);
		const TArray<FString> RequestedScopeIdentities = CollectScopeIdentitiesFromVisibleChange(Change);
		FBlueprintHelperReviewStoreService Store;
		FBlueprintHelperReviewPendingIndexQuery Query;
		Query.bPendingOnly = true;
		Query.bSkipMissingAssetRecords = false;
		const TArray<FBlueprintHelperReviewPendingVisibleChangeSummary> Summaries =
			Store.QueryPendingVisibleChangeSummaries(Query);
		for (const FBlueprintHelperReviewPendingVisibleChangeSummary& Summary : Summaries)
		{
			if (!ReviewAssetPathMatches(Change.AssetPath, Summary.RecordAssetPath)
				&& !ReviewAssetPathMatches(Change.AssetPath, Summary.Change.AssetPath))
			{
				continue;
			}

			const FBlueprintHelperReviewVisibleChange& Candidate = Summary.Change;
			if (!ReviewAssetPathMatches(Change.AssetPath, Candidate.AssetPath))
			{
				continue;
			}
			if (Candidate.Status != EBlueprintHelperReviewChangeStatus::Pending
				&& Candidate.Status != EBlueprintHelperReviewChangeStatus::NeedsAction
				&& Candidate.Status != EBlueprintHelperReviewChangeStatus::RejectFailed)
			{
				continue;
			}

			const TArray<FString> CandidateTargetKeys = CollectTargetKeysFromVisibleChange(Candidate);
			TArray<FString> MatchedTargetKeys;
			const bool bHasRequestedTarget = RequestedTargetKeys.Num() > 0
				&& IntersectTargetKeys(RequestedTargetKeys, CandidateTargetKeys, MatchedTargetKeys);
			if (RequestedTargetKeys.Num() > 0)
			{
				if (bHasRequestedTarget)
				{
					AddPersistedReviewTargetMatch(Matches, Summary.ReviewRecordId, MatchedTargetKeys);
				}
				continue;
			}

			if (RequestedScopeIdentities.Num() == 0)
			{
				continue;
			}

			const TArray<FString> CandidateScopeIdentities = CollectScopeIdentitiesFromVisibleChange(Candidate);
			bool bScopeMatched = false;
			for (const FString& RequestedScopeIdentity : RequestedScopeIdentities)
			{
				if (!RequestedScopeIdentity.IsEmpty() && CandidateScopeIdentities.Contains(RequestedScopeIdentity))
				{
					bScopeMatched = true;
					break;
				}
			}
			if (!bScopeMatched)
			{
				continue;
			}

			for (const FBlueprintHelperReviewAtomicTarget& CandidateTarget : Candidate.AtomicTargets)
			{
				if (CandidateTarget.Status != EBlueprintHelperReviewChangeStatus::Pending)
				{
					continue;
				}
				const FString CandidateTargetScopeIdentity =
					FBlueprintHelperReviewStoreTargetUtils::MakeReviewScopeIdentity(
						CandidateTarget,
						Candidate.ScopeIdentity);
				if (!CandidateTargetScopeIdentity.IsEmpty() && RequestedScopeIdentities.Contains(CandidateTargetScopeIdentity))
				{
					MatchedTargetKeys.AddUnique(CandidateTarget.TargetKey);
				}
			}
			if (MatchedTargetKeys.Num() == 0)
			{
				continue;
			}

			AddPersistedReviewTargetMatch(Matches, Summary.ReviewRecordId, MatchedTargetKeys);
		}

		return Matches;
	}
TArray<FPersistedReviewTargetMatch> FBlueprintHelperReviewActionTargetUtils::ResolvePersistedReviewTargetMatchesBatch(
		const TArray<FBlueprintHelperReviewVisibleChange>& Changes)
	{
		struct FPendingRequest
		{
			const FBlueprintHelperReviewVisibleChange* Change = nullptr;
			TArray<FString> TargetKeys;
			TArray<FString> ScopeIdentities;
		};

		TArray<FPendingRequest> Requests;
		TMap<FString, TArray<int32>> RequestIndicesByTargetKey;
		TMap<FString, TArray<int32>> RequestIndicesByScopeIdentity;
		for (const FBlueprintHelperReviewVisibleChange& Change : Changes)
		{
			if (Change.AssetPath.IsEmpty())
			{
				continue;
			}

			FPendingRequest Request;
			Request.Change = &Change;
			Request.TargetKeys = CollectTargetKeysFromVisibleChange(Change);
			Request.ScopeIdentities = CollectScopeIdentitiesFromVisibleChange(Change);
			const int32 RequestIndex = Requests.Add(MoveTemp(Request));
			for (const FString& TargetKey : Requests[RequestIndex].TargetKeys)
			{
				if (!TargetKey.IsEmpty())
				{
					RequestIndicesByTargetKey.FindOrAdd(TargetKey).Add(RequestIndex);
				}
			}
			if (Requests[RequestIndex].TargetKeys.Num() == 0)
			{
				for (const FString& ScopeIdentity : Requests[RequestIndex].ScopeIdentities)
				{
					if (!ScopeIdentity.IsEmpty())
					{
						RequestIndicesByScopeIdentity.FindOrAdd(ScopeIdentity).Add(RequestIndex);
					}
				}
			}
		}

		TArray<FPersistedReviewTargetMatch> Matches;
		if (Requests.Num() == 0)
		{
			return Matches;
		}

		FBlueprintHelperReviewStoreService Store;
		FBlueprintHelperReviewPendingIndexQuery Query;
		Query.bPendingOnly = true;
		Query.bSkipMissingAssetRecords = false;
		const TArray<FBlueprintHelperReviewPendingVisibleChangeSummary> Summaries =
			Store.QueryPendingVisibleChangeSummaries(Query);
		for (const FBlueprintHelperReviewPendingVisibleChangeSummary& Summary : Summaries)
		{
			const FBlueprintHelperReviewVisibleChange& Candidate = Summary.Change;
			if (Candidate.Status != EBlueprintHelperReviewChangeStatus::Pending
				&& Candidate.Status != EBlueprintHelperReviewChangeStatus::NeedsAction
				&& Candidate.Status != EBlueprintHelperReviewChangeStatus::RejectFailed)
			{
				continue;
			}

			for (const FBlueprintHelperReviewAtomicTarget& CandidateTarget : Candidate.AtomicTargets)
			{
				if (!CandidateTarget.TargetKey.IsEmpty())
				{
					if (const TArray<int32>* RequestIndices =
						RequestIndicesByTargetKey.Find(CandidateTarget.TargetKey))
					{
						for (const int32 RequestIndex : *RequestIndices)
						{
							if (!Requests.IsValidIndex(RequestIndex) || !Requests[RequestIndex].Change)
							{
								continue;
							}
							const FBlueprintHelperReviewVisibleChange& RequestedChange =
								*Requests[RequestIndex].Change;
							if (!ReviewAssetPathMatches(RequestedChange.AssetPath, Summary.RecordAssetPath)
								&& !ReviewAssetPathMatches(RequestedChange.AssetPath, Candidate.AssetPath))
							{
								continue;
							}
							AddPersistedReviewTargetMatch(
								Matches,
								Summary.ReviewRecordId,
								{ CandidateTarget.TargetKey });
						}
					}
				}

				if (RequestIndicesByScopeIdentity.Num() == 0
					|| CandidateTarget.Status != EBlueprintHelperReviewChangeStatus::Pending)
				{
					continue;
				}

				const FString CandidateTargetScopeIdentity =
					FBlueprintHelperReviewStoreTargetUtils::MakeReviewScopeIdentity(
						CandidateTarget,
						Candidate.ScopeIdentity);
				if (CandidateTargetScopeIdentity.IsEmpty())
				{
					continue;
				}

				const TArray<int32>* RequestIndices =
					RequestIndicesByScopeIdentity.Find(CandidateTargetScopeIdentity);
				if (!RequestIndices)
				{
					continue;
				}
				for (const int32 RequestIndex : *RequestIndices)
				{
					if (!Requests.IsValidIndex(RequestIndex) || !Requests[RequestIndex].Change)
					{
						continue;
					}
					const FBlueprintHelperReviewVisibleChange& RequestedChange =
						*Requests[RequestIndex].Change;
					if (!ReviewAssetPathMatches(RequestedChange.AssetPath, Summary.RecordAssetPath)
						&& !ReviewAssetPathMatches(RequestedChange.AssetPath, Candidate.AssetPath))
					{
						continue;
					}
					AddPersistedReviewTargetMatch(
						Matches,
						Summary.ReviewRecordId,
						{ CandidateTarget.TargetKey });
				}
			}
		}

		return Matches;
	}
bool FBlueprintHelperReviewActionTargetUtils::TryResolvePersistedReviewChange(
		const FBlueprintHelperReviewVisibleChange& Change,
		FString& OutReviewRecordId,
		TArray<FString>& OutTargetKeys)
	{
		const TArray<FPersistedReviewTargetMatch> Matches = ResolvePersistedReviewTargetMatches(Change);
		if (Matches.Num() == 0)
		{
			return false;
		}

		OutReviewRecordId = Matches[0].ReviewRecordId;
		OutTargetKeys = Matches[0].TargetKeys;
		return !OutReviewRecordId.IsEmpty() && OutTargetKeys.Num() > 0;
	}
bool FBlueprintHelperReviewActionTargetUtils::TryFindReviewAtomicTarget(
		const FBlueprintHelperReviewRecord& Record,
		const FString& TargetKey,
		FBlueprintHelperReviewAtomicTarget& OutTarget)
	{
		for (const FBlueprintHelperReviewVisibleChange& Change : Record.VisibleChanges)
		{
			for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
			{
				if (Target.TargetKey == TargetKey)
				{
					OutTarget = Target;
					return true;
				}
			}
		}
		return false;
	}
