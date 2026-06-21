// BlueprintHelper Review baseline dirty classifier.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperReviewBaselineDirtyState.h"

struct BLUEPRINTHELPER_API FBlueprintHelperReviewBaselineDirtyClassifyRequest
{
	TArray<FString> TargetAssets;
	TArray<FString> DirtyAssets;
	FString CurrentTaskRunId;
	FString FailedTaskRunId;
	FString SourceControlStatus;
	FString ActiveSequentialReviewSessionId;
	FString ActiveSequentialReviewArchiveSessionId;
	TArray<FString> ActiveReviewEvidenceRefs;
	TArray<FString> DiagnosticEvidenceRefs;
	bool bDirtyPreexistingBeforeRun = false;
	bool bDirtyFromFailedExecute = false;
	bool bDirtyFromExternalUserChange = false;
	bool bDirtyFromActiveSequentialReviewSession = false;
	bool bSequentialReviewSessionHasLastGoodSnapshot = false;
	bool bSequentialReviewSessionHasUnresolvedFailedExecute = false;
	bool bSequentialReviewSessionHasExternalConflict = false;
	bool bEditorSessionOwnershipKnown = false;
	bool bEditorSessionAgentOwned = false;
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewBaselineDirtyClassifier
{
public:
	FBlueprintHelperReviewBaselineDirtyDecision Classify(
		const FBlueprintHelperReviewBaselineDirtyClassifyRequest& Request) const;
};
