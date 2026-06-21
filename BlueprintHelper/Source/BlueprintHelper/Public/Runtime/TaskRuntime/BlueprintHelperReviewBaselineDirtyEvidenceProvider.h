// BlueprintHelper Review baseline dirty evidence provider.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperReviewBaselineDirtyClassifier.h"

class FJsonObject;

class BLUEPRINTHELPER_API FBlueprintHelperReviewBaselineDirtyEvidenceProvider
{
public:
	FBlueprintHelperReviewBaselineDirtyClassifyRequest BuildClassifyRequest(
		const TArray<FString>& TargetAssets,
		const TArray<FString>& DirtyAssets) const;

	FBlueprintHelperReviewBaselineDirtyClassifyRequest BuildClassifyRequest(
		const TArray<FString>& TargetAssets,
		const TArray<FString>& DirtyAssets,
		const TMap<FString, TSharedPtr<FJsonObject>>& TaskRunJournals) const;

	TArray<FString> CollectActiveReviewEvidenceRefsForTargetAssets(
		const TArray<FString>& TargetAssets) const;

private:
	void ApplySourceControlEvidence(
		const TArray<FString>& TargetAssets,
		FBlueprintHelperReviewBaselineDirtyClassifyRequest& InOutRequest) const;
	void AddPreRunDirtyEvidenceRefs(
		const TArray<FString>& DirtyAssets,
		TArray<FString>& InOutDiagnosticEvidenceRefs) const;
	void ApplyFailedTaskRunEvidence(
		const TArray<FString>& DirtyAssets,
		const TMap<FString, TSharedPtr<FJsonObject>>& TaskRunJournals,
		FBlueprintHelperReviewBaselineDirtyClassifyRequest& InOutRequest) const;
	void ApplyFailedTaskRunEvidence(
		const TArray<FString>& DirtyAssets,
		const TArray<TSharedPtr<FJsonObject>>& TaskRunJournals,
		FBlueprintHelperReviewBaselineDirtyClassifyRequest& InOutRequest) const;
	void ApplySequentialReviewSessionEvidence(
		const TArray<FString>& TargetAssets,
		const TArray<FString>& DirtyAssets,
		FBlueprintHelperReviewBaselineDirtyClassifyRequest& InOutRequest) const;
	static void AddUniqueNonEmptyString(TArray<FString>& Values, const FString& Value);
	static bool ReadStringArrayField(
		const TSharedPtr<FJsonObject>& Json,
		const FString& FieldName,
		TArray<FString>& OutValues);
	static bool HasAnySharedString(
		const TArray<FString>& Left,
		const TArray<FString>& Right);
};
