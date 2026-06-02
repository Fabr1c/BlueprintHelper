#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphWriteExecutionStats.h"

class UFunction;

struct FBlueprintHelperCandidateFunctionGroup
{
	FString Target;
	TArray<FBlueprintHelperCallFunctionCandidateInfo> Candidates;
};

struct FUnresolvedNodeItem
{
	FString DisplayText;
	FString Reason;
	TArray<FBlueprintHelperCandidateFunctionGroup> CandidateFunctions;
};

struct FBlueprintGeneratorDiagnostic
{
	FString Severity;
	FString Code;
	FString NodeId;
	FString PinName;
	FString Message;

	bool IsError() const
	{
		return Severity.Equals(TEXT("error"), ESearchCase::IgnoreCase);
	}
};

struct FEngineFunctionItem
{
	UFunction* FunctionPtr = nullptr;
	FString FunctionName;
	FString SearchMode;
	FString AmbiguityPolicy;
	TArray<FString> CategoryPriority;
	FString ActionContextStatementId;
	FString NativeFunctionName;
	FString Category;
};

struct FBlueprintGenerateResult
{
	bool bSucceed = false;
	int32 GeneratedNodeCount = 0;
	int32 RequestedDefaultValueCount = 0;
	int32 AppliedDefaultValueCount = 0;
	TArray<FBlueprintGeneratorDiagnostic> DefaultValueDiagnostics;
	int32 RequestedPinTypeCount = 0;
	int32 ResolvedPinTypeCount = 0;
	TArray<FBlueprintGeneratorDiagnostic> PinTypeDiagnostics;
	int32 RequestedConnectionCount = 0;
	int32 CreatedConnectionCount = 0;
	TArray<FBlueprintGeneratorDiagnostic> ConnectionDiagnostics;
	int32 ConnectivityViolationCount = 0;
	TArray<FBlueprintGeneratorDiagnostic> ConnectivityDiagnostics;
	int32 UnresolvedNodeCount = 0;
	FString Message;
	FBlueprintGraphWriteExecutionStats ExecutionStats;
};
