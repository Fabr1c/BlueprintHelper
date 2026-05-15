// BlueprintHelper Review Store service.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

class FJsonObject;
struct FBlueprintHelperGraphFragmentEvidenceBundle;

class BLUEPRINTHELPER_API FBlueprintHelperReviewStoreService
{
public:
	static FString NormalizeGraphBlockTargetId(
		const FString& GraphName,
		const FString& BlockRefOrId);

	static FString MakeReviewRecordId(
		const FString& ArchiveSessionId,
		const FString& AssetPath);

	TArray<FBlueprintHelperReviewVisibleChange> BuildVisibleChanges(
		const TArray<FBlueprintHelperReviewTransactionInput>& Transactions) const;

	TArray<FBlueprintHelperReviewRecord> BuildReviewRecordsFromEvidence(
		const TArray<FBlueprintHelperWriteReviewEvidence>& Evidences) const;

	TArray<FBlueprintHelperReviewRecord> BuildReviewRecordsFromFragmentEvidence(
		const FBlueprintHelperGraphFragmentEvidenceBundle& FragmentEvidence,
		const FString& ArchiveSessionId,
		const FString& AssetPath,
		const FString& OperationKind,
		const FString& TaskRunId = TEXT(""),
		const FString& TransactionId = TEXT(""),
		const FString& CreatedAt = TEXT("")) const;

	TArray<FBlueprintHelperReviewRecord> QueryReviewRecords(
		const FBlueprintHelperReviewRecordQuery& Query = FBlueprintHelperReviewRecordQuery()) const;

	bool LoadReviewRecordById(
		const FString& ReviewRecordId,
		FBlueprintHelperReviewRecord& OutRecord,
		FString& OutError) const;

	bool DeleteReviewRecord(
		const FString& ReviewRecordId,
		FString& OutError) const;

	bool PurgeReviewTargets(
		const FString& ReviewRecordId,
		const TArray<FString>& TargetKeys,
		TArray<FString>& OutDebugCaseIdsToDelete,
		bool& bOutRecordDeleted,
		FString& OutError) const;

	TSharedRef<FJsonObject> BuildReviewRecordSummaryArtifact(
		const FBlueprintHelperReviewRecord& Record) const;

	bool SaveReviewRecord(
		const FBlueprintHelperReviewRecord& Record,
		FString& OutError) const;

	bool SaveReviewRecords(
		const TArray<FBlueprintHelperReviewRecord>& Records,
		FString& OutError) const;

	bool SaveArchiveSession(
		const FBlueprintHelperReviewArchiveSession& ArchiveSession,
		FString& OutError) const;

	bool LoadArchiveSession(
		const FString& ArchiveSessionId,
		FBlueprintHelperReviewArchiveSession& OutArchiveSession,
		FString& OutError) const;

	TArray<FBlueprintHelperReviewVisibleChange> LoadPendingVisibleChanges(
		const FString& AssetPathFilter = TEXT("")) const;

	FDelegateHandle AddPendingReviewChangedHandler(const FSimpleDelegate& Handler) const;
	void RemovePendingReviewChangedHandler(FDelegateHandle Handle) const;
	void NotifyPendingReviewChanged() const;

private:
	FBlueprintHelperReviewVisibleChange MakeVisibleChange(
		const FBlueprintHelperReviewTransactionInput& Input,
		const FString& ChangeIdSuffix = TEXT("")) const;

	void AddAtomicTargetsForInput(
		const FBlueprintHelperReviewTransactionInput& Input,
		TMap<FString, FBlueprintHelperReviewVisibleChange>& AtomicChanges,
		TArray<FString>& AtomicOrder) const;

	void GroupAtomicVisibleChange(
		const FBlueprintHelperReviewVisibleChange& AtomicChange,
		TMap<FString, int32>& GroupToIndex,
		TArray<FBlueprintHelperReviewVisibleChange>& OutChanges) const;

	TArray<FBlueprintHelperReviewAtomicTarget> MakeAtomicTargetsForInput(
		const FBlueprintHelperReviewTransactionInput& Input) const;

	void AddEvidenceAtomicTargets(
		const FBlueprintHelperWriteReviewEvidence& Evidence,
		FBlueprintHelperReviewRecord& Record) const;

	mutable FSimpleMulticastDelegate PendingReviewChangedDelegate;
};
