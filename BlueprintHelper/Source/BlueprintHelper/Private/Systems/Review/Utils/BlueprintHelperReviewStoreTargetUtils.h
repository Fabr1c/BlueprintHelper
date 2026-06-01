// BlueprintHelper Review FBlueprintHelperReviewStoreTargetUtils declarations.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentEvidence.h"

class FBlueprintHelperReviewStoreTargetUtils
{
public:
	static FString ExtractReviewNodeIdentifier(const FString& RawNodePath);
	static FBlueprintHelperReviewAtomicTarget MakeGraphRecordTarget(
				const FBlueprintHelperReviewEvidenceInput& Input,
				const FString& TargetId,
				const FString& TargetPrefix);
	static void AddGraphTargetsFromStringArrayField(
				const TSharedPtr<FJsonObject>& Record,
				const TCHAR* FieldName,
				const FString& TargetPrefix,
				bool bExtractNodeName,
				FBlueprintHelperReviewEvidenceInput& Input);
	static FString MakeReviewInternalMissingAnchorKey(const FString& EvidenceId, int32 Index);
	static FString MakeReviewInternalMissingGroupKey(const FString& EvidenceId, int32 Index);
	static FString MakeReviewAtomicLookupKey(const FBlueprintHelperReviewAtomicTarget& Target, const FString& FallbackKey);
	static FString MakeReviewScopeIdentity(const FBlueprintHelperReviewAtomicTarget& Target, const FString& FallbackKey);
	static bool IsReviewTargetNetNoChange(const FBlueprintHelperReviewAtomicTarget& Target);
	static void PreserveFirstBaselineFields(
				FBlueprintHelperReviewAtomicTarget& Target,
				const FBlueprintHelperReviewAtomicTarget& Existing,
				const FBlueprintHelperReviewAtomicTarget& Incoming);
	static void PreserveFirstBaselineFields(
				FBlueprintHelperReviewVisibleChange& Change,
				const FBlueprintHelperReviewVisibleChange& Existing,
				const FBlueprintHelperReviewVisibleChange& Incoming);
	static FString SanitizeReviewIdSegment(const FString& Value);
	static FString MakeReviewVisibleChangeId(const FString& EvidenceId, const FString& VisualGroupKey);
	static bool ShouldAggregateGraphBodyTarget(const FBlueprintHelperReviewAtomicTarget& Target);
	static void ApplyGraphBodyAggregation(FBlueprintHelperReviewAtomicTarget& Target);
	static FString MakeReviewPackageNameFromAssetPath(const FString& AssetPath);
	static FString MakeReviewAssetLinkKey(const FString& AssetPath);
	static bool DoesReviewAssetPackageExist(const FString& AssetPath);
	static bool IsReviewEvidenceTargetComplete(const FBlueprintHelperReviewAtomicTarget& Target, FString& OutReason);
	static bool IsAssetLifecycleRootTarget(
				const FBlueprintHelperReviewAtomicTarget& Target,
				EBlueprintHelperReviewChangeKind ChangeKind);
	static void EnsureLifecycleMetadata(FBlueprintHelperReviewAtomicTarget& Target);
	static void EnsureLifecycleMetadata(FBlueprintHelperReviewVisibleChange& Change);
	static void ApplyAssetLifecycleRootMetadata(FBlueprintHelperReviewVisibleChange& Change);
	static bool IsPendingLifecycleLinkCandidate(const FBlueprintHelperReviewVisibleChange& Change);
	static void LinkPendingChildrenToLifecycleRoots(TArray<FBlueprintHelperReviewVisibleChange>& Changes);
	static int32 GetReviewSortValue(int32 Value);
	static void SortVisibleChangesByReviewOrder(TArray<FBlueprintHelperReviewVisibleChange>& Changes);
	static FBlueprintHelperReviewVisibleChange MakeVisibleChangeFromEvidence(
				const FBlueprintHelperWriteReviewEvidence& Evidence,
				const FBlueprintHelperReviewAtomicTarget& Target,
				const FString& VisualGroupKey);
};
