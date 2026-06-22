#pragma once

#include "CoreMinimal.h"

struct BLUEPRINTHELPER_API FBlueprintHelperGraphBodyReadbackProjection
{
	FString ProjectionId;
	FString FunctionName;
	FString EntryNodeRef;
	TArray<FString> ExitNodeRefs;
	TArray<FString> EntryBoundaryRefs;
	TArray<FString> ResultBoundaryRefs;
	TArray<FString> FunctionInputPinRefs;
	TArray<FString> FunctionOutputPinRefs;
	TArray<FString> GeneratedNodeRefs;
	TArray<FString> ExecLinkRefs;
	TArray<FString> DataLinkRefs;
	TArray<FString> FoldedBoundaryNodeRefs;
	TArray<FString> VisibleBoundaryNodeRefs;
	TMap<FString, FString> BoundaryDisplayNames;
	FString BodyEntryNodeGuid;
	FString BodyEntryNodeClass;
	FString BodyEntryStableName;
	FString BodyEntryKind;
	FString BodyEntryMemberName;
	FString BodyEntryFunctionName;
	FString BodyEntryDisplayName;
	FString BodyEntryFingerprint;
	FString BodyFingerprint;
	FString BodyEvidenceStatus;
	FString BodyEvidenceErrorCode;
	FString BodyEvidenceErrorMessage;
	bool bSynthesizeLogicEntry = false;
	bool bSynthesizeLogicResult = false;
};
