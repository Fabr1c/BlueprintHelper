#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteResultTypes.h"

class UBlueprint;
class UEdGraph;

struct FBlueprintHelperGraphWriteDryRunSandboxInput
{
	UBlueprint* SourceBlueprint = nullptr;
	FString GraphName;
	FString GraphWritePayload;
};

struct FBlueprintHelperGraphWriteDryRunSandboxResult
{
	bool bSucceeded = false;
	FString ErrorCode;
	FString Message;
	int32 GeneratedNodeCount = 0;
	TArray<FBlueprintGeneratorDiagnostic> ConnectivityDiagnostics;
	FBlueprintGraphWriteExecutionStats ExecutionStats;
};

class BLUEPRINTHELPER_API FBlueprintHelperGraphWriteDryRunSandbox
{
public:
	FBlueprintHelperGraphWriteDryRunSandboxResult RunAppendPreview(
		const FBlueprintHelperGraphWriteDryRunSandboxInput& Input) const;
};
