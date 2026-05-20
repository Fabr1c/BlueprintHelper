// BlueprintHelper TaskRuntime main-thread commit service.

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeCommitService.h"

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeClusterHub.h"
#include "Shared/Debug/BlueprintHelperSaveAssetTypes.h"
#include "Systems/Debug/BlueprintHelperAssetBrowseService.h"
#include "Systems/Debug/BlueprintHelperCompileAssetService.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutCoordinator.h"

#include "Dom/JsonObject.h"

FBlueprintHelperTaskRuntimeCommitService::FBlueprintHelperTaskRuntimeCommitService(
	const FBlueprintHelperTaskRuntimeClusterHub& InClusterHub,
	const FBlueprintHelperCompileAssetService& InCompileAssetService,
	const FBlueprintHelperAssetBrowseService& InAssetBrowseService)
	: ClusterHub(InClusterHub)
	, CompileAssetService(InCompileAssetService)
	, AssetBrowseService(InAssetBrowseService)
{
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeCommitService::ExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
	bool bDryRun) const
{
	return ClusterHub.ExecuteStep(LoweredStep, bDryRun);
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeCommitService::CompileAsset(
	const FString& AssetPath) const
{
	TSharedRef<FJsonObject> CompilePayload = MakeShared<FJsonObject>();
	CompilePayload->SetStringField(TEXT("asset_path"), AssetPath);
	return CompileAssetService.Execute(CompilePayload);
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeCommitService::SaveAsset(
	const FString& AssetPath) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
	Target->SetStringField(TEXT("asset_path"), AssetPath);

	const FBlueprintHelperSaveResult SaveResult = AssetBrowseService.SaveAsset(AssetPath);
	if (!SaveResult.bSuccess)
	{
		FBlueprintHelperToolError Error;
		Error.Code = TEXT("save_failed");
		Error.Stage = EBlueprintHelperToolStage::Execute;
		Error.Message = SaveResult.ErrorMessage;
		Error.bRetryable = true;

		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Failure(
			TEXT("save_asset"),
			TraceId,
			Error);
		Result.CustomTargetJson = Target;
		return Result;
	}

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
		TEXT("save_asset"),
		TraceId);
	Result.bModified = false;
	Result.CustomTargetJson = Target;

	FBlueprintHelperSaveAssetResultData Data;
	Data.SaveResult.bSaved = true;
	Data.SaveResult.bWasDirty = true;
	Result.Data = Data.ToJson();
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeCommitService::MakeSkippedPostOperationResult(
	const FString& Operation,
	const FString& AssetPath,
	const FString& Reason) const
{
	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
		Operation,
		FBlueprintHelperToolResultBuilder::GenerateTraceId());
	Result.Status = EBlueprintHelperToolStatus::Skipped;
	Result.bModified = false;

	TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
	Target->SetStringField(TEXT("asset_path"), AssetPath);
	Result.CustomTargetJson = Target;

	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("skipped"), true);
	Data->SetStringField(TEXT("skip_reason"), Reason);
	Data->SetStringField(TEXT("asset_path"), AssetPath);
	Result.Data = Data;
	return Result;
}

void FBlueprintHelperTaskRuntimeCommitService::FlushGraphLayout() const
{
	FBlueprintHelperGraphLayoutCoordinator::FlushPendingTaskLayouts();
}
