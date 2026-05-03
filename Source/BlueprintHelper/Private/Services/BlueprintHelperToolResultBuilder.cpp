// BlueprintHelper Service Layer — ToolResultBuilder 实现

#include "Services/BlueprintHelperToolResultTypes.h"
#include "HAL/PlatformTime.h"

int32 FBlueprintHelperToolResultBuilder::TraceCounter = 0;
int32 FBlueprintHelperToolResultBuilder::TransactionCounter = 0;

FString FBlueprintHelperToolResultBuilder::GenerateTraceId()
{
	const FString TimePart = FString::Printf(TEXT("%lld"), static_cast<int64>(FPlatformTime::Seconds()));
	const FString CounterPart = FString::Printf(TEXT("%04d"), ++TraceCounter);
	return FString::Printf(TEXT("trace_%s_%s"), *TimePart, *CounterPart);
}

FString FBlueprintHelperToolResultBuilder::GenerateTransactionId(const FString& Prefix)
{
	const FString TimePart = FString::Printf(TEXT("%lld"), static_cast<int64>(FPlatformTime::Seconds()));
	const FString CounterPart = FString::Printf(TEXT("%04d"), ++TransactionCounter);
	return FString::Printf(TEXT("%s_%s_%s"), *Prefix, *TimePart, *CounterPart);
}

FBlueprintHelperToolResultBase FBlueprintHelperToolResultBuilder::Success(
	const FString& Operation, const FString& TraceId)
{
	FBlueprintHelperToolResultBase Result;
	Result.bOk = true;
	Result.Schema = DefaultSchema;
	Result.Operation = Operation;
	Result.TraceId = TraceId;
	Result.Status = EBlueprintHelperToolStatus::Applied;
	Result.bModified = true;
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperToolResultBuilder::Failure(
	const FString& Operation, const FString& TraceId, const FBlueprintHelperToolError& Error)
{
	FBlueprintHelperToolResultBase Result;
	Result.bOk = false;
	Result.Schema = DefaultSchema;
	Result.Operation = Operation;
	Result.TraceId = TraceId;
	Result.Status = EBlueprintHelperToolStatus::Failed;
	Result.bModified = false;
	Result.Error = Error;
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperToolResultBuilder::DryRun(
	const FString& Operation, const FString& TraceId)
{
	FBlueprintHelperToolResultBase Result;
	Result.bOk = true;
	Result.Schema = DefaultSchema;
	Result.Operation = Operation;
	Result.TraceId = TraceId;
	Result.Status = EBlueprintHelperToolStatus::DryRun;
	Result.bModified = false;
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperToolResultBuilder::NoOp(
	const FString& Operation, const FString& TraceId)
{
	FBlueprintHelperToolResultBase Result;
	Result.bOk = true;
	Result.Schema = DefaultSchema;
	Result.Operation = Operation;
	Result.TraceId = TraceId;
	Result.Status = EBlueprintHelperToolStatus::NoOp;
	Result.bModified = false;
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperToolResultBuilder::Applied(
	const FString& Operation, const FString& TraceId)
{
	FBlueprintHelperToolResultBase Result;
	Result.bOk = true;
	Result.Schema = DefaultSchema;
	Result.Operation = Operation;
	Result.TraceId = TraceId;
	Result.Status = EBlueprintHelperToolStatus::Applied;
	Result.bModified = true;
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperToolResultBuilder::Completed(
	const FString& Operation, const FString& TraceId)
{
	FBlueprintHelperToolResultBase Result;
	Result.bOk = true;
	Result.Schema = DefaultSchema;
	Result.Operation = Operation;
	Result.TraceId = TraceId;
	Result.Status = EBlueprintHelperToolStatus::Completed;
	Result.bModified = false;
	return Result;
}
