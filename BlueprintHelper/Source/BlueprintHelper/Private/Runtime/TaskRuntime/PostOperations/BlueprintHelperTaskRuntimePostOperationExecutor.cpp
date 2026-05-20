#include "Runtime/TaskRuntime/PostOperations/BlueprintHelperTaskRuntimePostOperationExecutor.h"

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCommitService.h"
#include "Runtime/TaskRuntime/PostOperations/BlueprintHelperTaskRuntimeAssetStateService.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformTime.h"

class FBlueprintHelperTaskRuntimePostOperationExecutorLocalUtils
{
public:
	static double DurationMsSince(double StartSeconds)
	{
		return (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
	}

	static void FinalizeRecordDuration(
		FBlueprintHelperTaskRuntimePostOperationRecordEx& Record,
		double StartSeconds)
	{
		Record.DurationMs = DurationMsSince(StartSeconds);
	}

	static void SetResultTarget(
		FBlueprintHelperToolResultBase& Result,
		const FString& AssetPath)
	{
		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);
		Result.CustomTargetJson = Target;
	}

	static void SetSkippedData(
		FBlueprintHelperToolResultBase& Result,
		const FString& AssetPath,
		const FString& Reason)
	{
		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetBoolField(TEXT("skipped"), true);
		Data->SetStringField(TEXT("skip_reason"), Reason);
		Data->SetStringField(TEXT("asset_path"), AssetPath);
		Result.Data = Data;
	}
};

FBlueprintHelperTaskRuntimePostOperationExecutionResult FBlueprintHelperTaskRuntimePostOperationExecutor::Execute(
	const FBlueprintHelperTaskRuntimePostOperationPlan& Plan,
	const FBlueprintHelperTaskRuntimeCommitService* CommitService) const
{
	FBlueprintHelperTaskRuntimePostOperationExecutionResult ExecutionResult;

	for (const FBlueprintHelperTaskRuntimePostOperationPlanItem& Item : Plan.Items)
	{
		const double ItemStart = FPlatformTime::Seconds();
		FBlueprintHelperTaskRuntimePostOperationRecordEx Record;
		Record.Kind = Item.Kind;
		Record.Operation = Item.Operation;
		Record.AssetPath = Item.AssetPath;

		if (Item.Kind == EBlueprintHelperTaskRuntimePostOperationKind::Compile)
		{
			const FBlueprintHelperTaskRuntimeAssetState State =
				FBlueprintHelperTaskRuntimeAssetStateService::ReadState(Item.AssetPath);
			if (!State.bIsBlueprint)
			{
				Record.Status = EBlueprintHelperTaskRuntimePostOperationStatus::Skipped;
				Record.Reason = TEXT("asset_not_blueprint");
				Record.Result = CommitService
					? CommitService->MakeSkippedPostOperationResult(Record.Operation, Record.AssetPath, Record.Reason)
					: MakeFallbackSkippedResult(Record.Operation, Record.AssetPath, Record.Reason);
				FBlueprintHelperTaskRuntimePostOperationExecutorLocalUtils::FinalizeRecordDuration(Record, ItemStart);
				ExecutionResult.Records.Add(Record);
				continue;
			}

			if (!CommitService)
			{
				Record.Status = EBlueprintHelperTaskRuntimePostOperationStatus::Skipped;
				Record.Reason = TEXT("commit_service_unavailable");
				Record.Result = MakeFallbackSkippedResult(Record.Operation, Record.AssetPath, Record.Reason);
				FBlueprintHelperTaskRuntimePostOperationExecutorLocalUtils::FinalizeRecordDuration(Record, ItemStart);
				ExecutionResult.Records.Add(Record);
				continue;
			}

			Record.Result = CommitService->CompileAsset(Item.AssetPath);
			Record.Status = Record.Result.bOk
				? EBlueprintHelperTaskRuntimePostOperationStatus::Executed
				: EBlueprintHelperTaskRuntimePostOperationStatus::Failed;
		}
		else if (Item.Kind == EBlueprintHelperTaskRuntimePostOperationKind::Save)
		{
			const FBlueprintHelperTaskRuntimeAssetState State =
				FBlueprintHelperTaskRuntimeAssetStateService::ReadState(Item.AssetPath);
			if (!State.bPackageLoaded || !State.bPackageDirty)
			{
				Record.Status = EBlueprintHelperTaskRuntimePostOperationStatus::Skipped;
				Record.Reason = TEXT("package_not_loaded_or_clean");
				Record.Result = CommitService
					? CommitService->MakeSkippedPostOperationResult(Record.Operation, Record.AssetPath, Record.Reason)
					: MakeFallbackSkippedResult(Record.Operation, Record.AssetPath, Record.Reason);
				FBlueprintHelperTaskRuntimePostOperationExecutorLocalUtils::FinalizeRecordDuration(Record, ItemStart);
				ExecutionResult.Records.Add(Record);
				continue;
			}

			if (!CommitService)
			{
				Record.Status = EBlueprintHelperTaskRuntimePostOperationStatus::Skipped;
				Record.Reason = TEXT("commit_service_unavailable");
				Record.Result = MakeFallbackSkippedResult(Record.Operation, Record.AssetPath, Record.Reason);
				FBlueprintHelperTaskRuntimePostOperationExecutorLocalUtils::FinalizeRecordDuration(Record, ItemStart);
				ExecutionResult.Records.Add(Record);
				continue;
			}

			const FString SavePath = State.ObjectPath.IsEmpty() ? Item.AssetPath : State.ObjectPath;
			Record.Result = CommitService->SaveAsset(SavePath);
			Record.Status = Record.Result.bOk
				? EBlueprintHelperTaskRuntimePostOperationStatus::Executed
				: EBlueprintHelperTaskRuntimePostOperationStatus::Failed;
		}

		if (!Record.Result.bOk)
		{
			ExecutionResult.bOk = false;
			if (Record.Result.Error.IsSet())
			{
				ExecutionResult.FirstError = *Record.Result.Error;
			}
			FBlueprintHelperTaskRuntimePostOperationExecutorLocalUtils::FinalizeRecordDuration(Record, ItemStart);
			ExecutionResult.Records.Add(Record);
			return ExecutionResult;
		}

		FBlueprintHelperTaskRuntimePostOperationExecutorLocalUtils::FinalizeRecordDuration(Record, ItemStart);
		ExecutionResult.Records.Add(Record);
	}

	return ExecutionResult;
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimePostOperationExecutor::MakeFallbackSkippedResult(
	const FString& Operation,
	const FString& AssetPath,
	const FString& Reason)
{
	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
		Operation,
		FBlueprintHelperToolResultBuilder::GenerateTraceId());
	Result.Status = EBlueprintHelperToolStatus::Skipped;
	Result.bModified = false;
	FBlueprintHelperTaskRuntimePostOperationExecutorLocalUtils::SetResultTarget(Result, AssetPath);
	FBlueprintHelperTaskRuntimePostOperationExecutorLocalUtils::SetSkippedData(Result, AssetPath, Reason);
	return Result;
}
