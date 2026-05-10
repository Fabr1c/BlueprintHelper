#include "Systems/Debug/BlueprintHelperDebugEntryService.h"

#include "Dom/JsonObject.h"
#include "Systems/Debug/BlueprintHelperDebugCaseStoreService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"

FBlueprintHelperDebugEntryService::FBlueprintHelperDebugEntryService(
	const FBlueprintHelperDebugCaseStoreService& InStore,
	const FBlueprintHelperReviewStoreService* InReviewStore)
	: Store(InStore)
	, ReviewStore(InReviewStore)
{
}

FString FBlueprintHelperDebugEntryService::NewDebugCaseId()
{
	return TEXT("dbg_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
}

FString FBlueprintHelperDebugEntryService::NewDebugEventId()
{
	return TEXT("ev_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
}

FString FBlueprintHelperDebugEntryService::UtcTimestamp()
{
	return FDateTime::UtcNow().ToIso8601();
}

FBlueprintHelperDebugEntryRecordResult FBlueprintHelperDebugEntryService::RecordEventBestEffort(
	const FBlueprintHelperDebugEntryEventInput& Input) const
{
	FBlueprintHelperDebugEntryRecordResult Result;
	Result.DebugCaseId = NewDebugCaseId();
	Result.DebugEventId = NewDebugEventId();
	const FString Now = UtcTimestamp();

	FBlueprintHelperDebugEvent Event;
	Event.DebugEventId = Result.DebugEventId;
	Event.DebugCaseId = Result.DebugCaseId;
	Event.CreatedAt = Now;
	Event.SourceLayer = Input.SourceLayer;
	Event.Source = Input.Source;
	Event.Operation = Input.Operation;
	Event.Stage = Input.Stage;
	Event.Severity = Input.Severity;
	Event.Status = EBlueprintHelperDebugEventStatus::Captured;
	Event.TraceId = Input.TraceId;
	Event.TaskRunId = Input.TaskRunId;
	Event.AssetPaths = Input.AssetPaths;
	Event.ReviewRecordIds = Input.ReviewRecordIds;
	Event.TransactionLinks = Input.TransactionLinks;
	Event.Error = Input.Error;
	Event.RecommendedNext = Input.RecommendedNext;
	Event.ToolResultSummary = Input.ToolResultSummary;

	FBlueprintHelperDebugCase DebugCase;
	DebugCase.DebugCaseId = Result.DebugCaseId;
	DebugCase.CreatedAt = Now;
	DebugCase.UpdatedAt = Now;
	DebugCase.Source = Input.Source;
	DebugCase.Severity = Input.Severity;
	DebugCase.Status = EBlueprintHelperDebugCaseStatus::Open;
	DebugCase.Operation = Input.Operation;
	DebugCase.Stage = Input.Stage;
	if (!Input.TraceId.IsEmpty())
	{
		DebugCase.TraceIds.Add(Input.TraceId);
	}
	DebugCase.TaskRunId = Input.TaskRunId;
	DebugCase.AssetPaths = Input.AssetPaths;
	DebugCase.ReviewRecordIds = Input.ReviewRecordIds;
	DebugCase.TransactionLinks = Input.TransactionLinks;
	DebugCase.Error = Input.Error;
	DebugCase.RecommendedNext = Input.RecommendedNext;
	DebugCase.Events.Add(Event);

	FString SaveError;
	Result.bRecorded = Store.SaveCase(DebugCase, &SaveError);
	Result.ErrorMessage = SaveError;
	return Result;
}

void FBlueprintHelperDebugEntryService::AttachDebugCaseToFailureBestEffort(
	FBlueprintHelperToolResultBase& Result,
	const FBlueprintHelperDebugEntryEventInput& Input) const
{
	if (Result.bOk)
	{
		return;
	}

	FBlueprintHelperDebugEntryEventInput EffectiveInput = Input;
	if (EffectiveInput.Operation.IsEmpty())
	{
		EffectiveInput.Operation = Result.Operation;
	}
	if (EffectiveInput.TraceId.IsEmpty())
	{
		EffectiveInput.TraceId = Result.TraceId;
	}
	if (EffectiveInput.Stage.IsEmpty() && Result.Error.IsSet())
	{
		EffectiveInput.Stage = ToolStageToString(Result.Error->Stage);
	}
	if (EffectiveInput.Error.Code.IsEmpty() && Result.Error.IsSet())
	{
		EffectiveInput.Error.Code = Result.Error->Code;
		EffectiveInput.Error.Message = Result.Error->Message;
	}
	if (!EffectiveInput.ToolResultSummary.IsValid())
	{
		EffectiveInput.ToolResultSummary = Result.ToJson();
	}

	const FBlueprintHelperDebugEntryRecordResult RecordResult = RecordEventBestEffort(EffectiveInput);
	if (RecordResult.bRecorded && !RecordResult.DebugCaseId.IsEmpty())
	{
		Result.DebugCaseIds.AddUnique(RecordResult.DebugCaseId);
	}
}

FBlueprintHelperToolResultBase FBlueprintHelperDebugEntryService::GetDebugCaseSummaryResult(
	const TSharedPtr<FJsonObject>& Payload) const
{
	FString DebugCaseId;
	if (!Payload.IsValid() || !Payload->TryGetStringField(TEXT("debug_case_id"), DebugCaseId) || DebugCaseId.IsEmpty())
	{
		FBlueprintHelperToolError Error;
		Error.Code = TEXT("invalid_request");
		Error.Stage = EBlueprintHelperToolStage::ParseInput;
		Error.Message = TEXT("debug_case_id is required.");
		return FBlueprintHelperToolResultBuilder::Failure(
			TEXT("get_debug_case"),
			FBlueprintHelperToolResultBuilder::GenerateTraceId(),
			Error);
	}

	FBlueprintHelperDebugCaseSummary Summary;
	FString QueryError;
	if (!Store.QueryCaseSummary(DebugCaseId, Summary, &QueryError))
	{
		FBlueprintHelperToolError Error;
		Error.Code = TEXT("debug_case_not_found");
		Error.Stage = EBlueprintHelperToolStage::ResolveTarget;
		Error.Message = QueryError.IsEmpty() ? TEXT("debug case not found.") : QueryError;
		Error.Field = TEXT("debug_case_id");
		Error.Actual = DebugCaseId;
		return FBlueprintHelperToolResultBuilder::Failure(
			TEXT("get_debug_case"),
			FBlueprintHelperToolResultBuilder::GenerateTraceId(),
			Error);
	}

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
		TEXT("get_debug_case"),
		FBlueprintHelperToolResultBuilder::GenerateTraceId());
	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetObjectField(TEXT("debug_case"), Summary.ToJson());
	Result.Data = Data;
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperDebugEntryService::GetDebugCaseListResult(
	const TSharedPtr<FJsonObject>& Payload) const
{
	TArray<FBlueprintHelperDebugCaseSummary> Summaries;
	FString QueryError;
	if (!Store.QueryCaseSummaries(Summaries, &QueryError))
	{
		FBlueprintHelperToolError Error;
		Error.Code = TEXT("debug_case_list_failed");
		Error.Stage = EBlueprintHelperToolStage::Execute;
		Error.Message = QueryError.IsEmpty() ? TEXT("debug case list failed.") : QueryError;
		return FBlueprintHelperToolResultBuilder::Failure(
			TEXT("debug_case_list"),
			FBlueprintHelperToolResultBuilder::GenerateTraceId(),
			Error);
	}

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
		TEXT("debug_case_list"),
		FBlueprintHelperToolResultBuilder::GenerateTraceId());
	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.DebugCaseList.v1"));
	Data->SetNumberField(TEXT("count"), Summaries.Num());
	TArray<TSharedPtr<FJsonValue>> CaseValues;
	for (const FBlueprintHelperDebugCaseSummary& Summary : Summaries)
	{
		CaseValues.Add(MakeShared<FJsonValueObject>(Summary.ToJson()));
	}
	Data->SetArrayField(TEXT("debug_cases"), CaseValues);
	Result.Data = Data;
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperDebugEntryService::ExportDebugBundleSummaryResult(
	const TSharedPtr<FJsonObject>& Payload) const
{
	FString DebugCaseId;
	if (!Payload.IsValid() || !Payload->TryGetStringField(TEXT("debug_case_id"), DebugCaseId) || DebugCaseId.IsEmpty())
	{
		FBlueprintHelperToolError Error;
		Error.Code = TEXT("invalid_request");
		Error.Stage = EBlueprintHelperToolStage::ParseInput;
		Error.Message = TEXT("debug_case_id is required.");
		return FBlueprintHelperToolResultBuilder::Failure(
			TEXT("debug_bundle_summary_export"),
			FBlueprintHelperToolResultBuilder::GenerateTraceId(),
			Error);
	}

	FBlueprintHelperDebugBundleManifest Manifest;
	FString ExportError;
	if (!Store.ExportDebugBundleSummary(DebugCaseId, ReviewStore, Manifest, &ExportError))
	{
		FBlueprintHelperToolError Error;
		Error.Code = TEXT("debug_bundle_export_failed");
		Error.Stage = EBlueprintHelperToolStage::Execute;
		Error.Message = ExportError.IsEmpty() ? TEXT("debug bundle summary export failed.") : ExportError;
		Error.Field = TEXT("debug_case_id");
		Error.Actual = DebugCaseId;
		return FBlueprintHelperToolResultBuilder::Failure(
			TEXT("debug_bundle_summary_export"),
			FBlueprintHelperToolResultBuilder::GenerateTraceId(),
			Error);
	}

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
		TEXT("debug_bundle_summary_export"),
		FBlueprintHelperToolResultBuilder::GenerateTraceId());
	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.DebugBundleSummaryExport.v1"));
	Data->SetObjectField(TEXT("manifest"), Manifest.ToJson());
	Result.Data = Data;
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperDebugEntryService::CleanupDebugCasesResult(
	const TSharedPtr<FJsonObject>& Payload) const
{
	TArray<FString> ArchivedCaseIds;
	FString CleanupError;
	if (!Store.CleanupResolvedLowSeverityCases(ArchivedCaseIds, &CleanupError))
	{
		FBlueprintHelperToolError Error;
		Error.Code = TEXT("debug_case_cleanup_failed");
		Error.Stage = EBlueprintHelperToolStage::Execute;
		Error.Message = CleanupError.IsEmpty() ? TEXT("debug case cleanup failed.") : CleanupError;
		return FBlueprintHelperToolResultBuilder::Failure(
			TEXT("debug_case_cleanup"),
			FBlueprintHelperToolResultBuilder::GenerateTraceId(),
			Error);
	}

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
		TEXT("debug_case_cleanup"),
		FBlueprintHelperToolResultBuilder::GenerateTraceId());
	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.DebugCaseCleanup.v1"));
	Data->SetNumberField(TEXT("archived_count"), ArchivedCaseIds.Num());
	Data->SetArrayField(TEXT("archived_debug_case_ids"), FBlueprintHelperDebugJson::StringArrayToJson(ArchivedCaseIds));
	Result.Data = Data;
	return Result;
}
