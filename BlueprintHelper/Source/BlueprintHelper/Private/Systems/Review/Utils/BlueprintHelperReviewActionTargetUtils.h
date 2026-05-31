// BlueprintHelper Review FBlueprintHelperReviewActionTargetUtils declarations.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/DataTable.h"
#include "Systems/Review/BlueprintHelperReviewActionService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"

class FBlueprintHelperReviewActionTargetUtils
{
public:
	struct FPersistedReviewTargetMatch
	{
		FString ReviewRecordId;
		TArray<FString> TargetKeys;
	};

	static TArray<FString> CollectPendingTargetKeys(const FBlueprintHelperReviewRecord& Record);
	static TArray<FString> CollectTargetKeysFromVisibleChange(const FBlueprintHelperReviewVisibleChange& Change);
	static TArray<FString> CollectScopeIdentitiesFromVisibleChange(const FBlueprintHelperReviewVisibleChange& Change);
	static FString MakeReviewPackageKey(FString AssetPath);
	static bool ReviewAssetPathMatches(const FString& Left, const FString& Right);
	static bool IntersectTargetKeys(
				const TArray<FString>& RequestedTargetKeys,
				const TArray<FString>& CandidateTargetKeys,
				TArray<FString>& OutMatchedTargetKeys);
	static void AddPersistedReviewTargetMatch(
				TArray<FPersistedReviewTargetMatch>& Matches,
				const FString& ReviewRecordId,
				const TArray<FString>& TargetKeys);
	static TArray<FPersistedReviewTargetMatch> ResolvePersistedReviewTargetMatches(
				const FBlueprintHelperReviewVisibleChange& Change);
	static TArray<FPersistedReviewTargetMatch> ResolvePersistedReviewTargetMatchesBatch(
				const TArray<FBlueprintHelperReviewVisibleChange>& Changes);
	static bool TryResolvePersistedReviewChange(
				const FBlueprintHelperReviewVisibleChange& Change,
				FString& OutReviewRecordId,
				TArray<FString>& OutTargetKeys);
	static bool TryFindReviewAtomicTarget(
				const FBlueprintHelperReviewRecord& Record,
				const FString& TargetKey,
				FBlueprintHelperReviewAtomicTarget& OutTarget);
};
