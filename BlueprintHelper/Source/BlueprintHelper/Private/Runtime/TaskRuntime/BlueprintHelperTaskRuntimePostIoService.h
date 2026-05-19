// BlueprintHelper TaskRuntime post IO service.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimePostIoBatch.h"
#include "Runtime/TaskRuntime/Utils/BlueprintHelperTaskRuntimeTimingUtils.h"

class FBlueprintHelperDebugEntryService;

class FBlueprintHelperTaskRuntimePostIoService
{
public:
	FBlueprintHelperTaskRuntimePostIoFlushResult Flush(
		const FBlueprintHelperTaskRuntimePostIoBatch& Batch,
		TMap<FString, TSharedPtr<FJsonObject>>& TaskRunJournals,
		const FBlueprintHelperDebugEntryService* DebugEntryService,
		FBlueprintHelperToolResultBase* MutableResultForDebugCase = nullptr,
		FBlueprintHelperTaskRuntimeTimingUtils::FTimingTrace* TimingTrace = nullptr) const;

private:
	static void AddDiagnostic(
		FBlueprintHelperTaskRuntimePostIoFlushResult& Result,
		const FString& Code,
		const FString& Message,
		const FString& Field = TEXT(""));
};
