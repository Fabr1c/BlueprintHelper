// BlueprintHelper Review Store service.

#pragma once

#include "CoreMinimal.h"
#include "Structure/Review/BlueprintHelperReviewTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewStoreService
{
public:
	static FString NormalizeGraphBlockTargetId(
		const FString& GraphName,
		const FString& BlockRefOrId);

	TArray<FBlueprintHelperReviewVisibleChange> BuildVisibleChanges(
		const TArray<FBlueprintHelperReviewTransactionInput>& Transactions) const;

	TArray<FBlueprintHelperReviewVisibleChange> LoadPendingVisibleChanges(
		const FString& AssetPathFilter = TEXT("")) const;

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
};
