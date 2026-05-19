// BlueprintHelper TaskRuntime post IO batch DTO.

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Systems/Debug/BlueprintHelperDebugEntryService.h"

class FJsonObject;

struct FBlueprintHelperTaskRuntimePostIoDiagnostic
{
	FString Code;
	FString Message;
	FString Field;
};

struct FBlueprintHelperTaskRuntimePostIoFlushResult
{
	bool bOk = true;
	TArray<FBlueprintHelperTaskRuntimePostIoDiagnostic> Diagnostics;

	TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperTaskRuntimePostIoBatch
{
	TOptional<FBlueprintHelperReviewArchiveSession> ArchiveSession;
	TArray<FBlueprintHelperWriteReviewEvidence> ReviewEvidences;
	TSharedPtr<FJsonObject> TaskRunJournal;
	FString TaskRunId;
	TOptional<FBlueprintHelperDebugEntryEventInput> DebugEventInput;
	bool bAttachDebugCaseToFailure = false;
	bool bNotifyPendingReviewChanged = false;
};
