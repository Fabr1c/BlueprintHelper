#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeTypes.h"

struct FBlueprintHelperWriteReviewEvidence;

struct BLUEPRINTHELPER_API FBlueprintHelperWidgetTreeReviewEvidenceBuildInput
{
	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolResultBase StepResult;
	FString ArchiveSessionId;
	FString TaskRunId;
	int32 StepIndex = INDEX_NONE;
};

class BLUEPRINTHELPER_API FBlueprintHelperWidgetTreeReviewEvidenceBuilder
{
public:
	static bool Build(
		const FBlueprintHelperWidgetTreeReviewEvidenceBuildInput& Input,
		FBlueprintHelperWriteReviewEvidence& OutEvidence);

private:
	static FString ReadStringField(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName);
	static TOptional<int32> ReadOptionalIntField(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName);
	static FString ReadOperationKind(const FBlueprintHelperWidgetTreeReviewEvidenceBuildInput& Input);
	static FString ReadTargetWidgetName(
		const FString& OperationKind,
		const FBlueprintHelperWidgetTreeReviewEvidenceBuildInput& Input);
	static FString SerializeJsonObject(const TSharedRef<FJsonObject>& Object);
	static TSharedRef<FJsonObject> BuildAnchorJson(
		const FString& OperationKind,
		const FBlueprintHelperWidgetTreeReviewEvidenceBuildInput& Input);
};
