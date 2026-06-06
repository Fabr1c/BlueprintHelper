#include "Systems/ToolClusters/GraphWrite/UnitOfWork/BlueprintHelperGraphWriteUnitOfWork.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

FString FBlueprintHelperGraphWriteUnitOfWork::StageToString(EBlueprintHelperGraphWriteUnitOfWorkStage Stage)
{
	switch (Stage)
	{
	case EBlueprintHelperGraphWriteUnitOfWorkStage::CaptureBefore:
		return TEXT("capture_before");
	case EBlueprintHelperGraphWriteUnitOfWorkStage::BuildMutationPlan:
		return TEXT("build_mutation_plan");
	case EBlueprintHelperGraphWriteUnitOfWorkStage::ApplyMutation:
		return TEXT("apply_mutation");
	case EBlueprintHelperGraphWriteUnitOfWorkStage::ProjectPreview:
		return TEXT("project_preview");
	case EBlueprintHelperGraphWriteUnitOfWorkStage::WriteOwnershipDelta:
		return TEXT("write_ownership_delta");
	case EBlueprintHelperGraphWriteUnitOfWorkStage::Commit:
		return TEXT("commit");
	case EBlueprintHelperGraphWriteUnitOfWorkStage::Rollback:
		return TEXT("rollback");
	case EBlueprintHelperGraphWriteUnitOfWorkStage::BuildDiagnostics:
		return TEXT("build_diagnostics");
	default:
		return TEXT("unknown");
	}
}

static void BlueprintHelperGraphWriteAddUnitOfWorkStage(
	TArray<FString>& StageTrace,
	EBlueprintHelperGraphWriteUnitOfWorkStage Stage)
{
	StageTrace.Add(FBlueprintHelperGraphWriteUnitOfWork::StageToString(Stage));
}

static void BlueprintHelperGraphWriteAttachUnitOfWorkTrace(
	FBlueprintHelperToolResultBase& ToolResult,
	const FBlueprintHelperGraphWriteUnitOfWorkRequest& Request,
	const TArray<FString>& StageTrace,
	bool bRolledBack)
{
	if (!ToolResult.Data.IsValid())
	{
		ToolResult.Data = MakeShared<FJsonObject>();
	}

	TArray<TSharedPtr<FJsonValue>> TraceValues;
	for (const FString& Stage : StageTrace)
	{
		TraceValues.Add(MakeShared<FJsonValueString>(Stage));
	}
	ToolResult.Data->SetArrayField(TEXT("unit_of_work_stage_trace"), TraceValues);
	ToolResult.Data->SetStringField(TEXT("unit_of_work_mode"),
		Request.Mode == EBlueprintHelperGraphWriteUnitOfWorkMode::Preview ? TEXT("preview") : TEXT("execute"));
	ToolResult.Data->SetStringField(TEXT("runtime_adapter_id"), Request.RuntimeAdapterId);
	ToolResult.Data->SetStringField(
		TEXT("graph_body_kind"),
		FBlueprintHelperGraphBodyBoundaryModelUtils::BodyKindToString(Request.BoundaryModel.BodyKind));
	ToolResult.Data->SetBoolField(TEXT("unit_of_work_rolled_back"), bRolledBack);
}

FBlueprintHelperGraphWriteUnitOfWorkResult FBlueprintHelperGraphWriteUnitOfWork::Run(
	const FBlueprintHelperGraphWriteUnitOfWorkRequest& Request)
{
	FBlueprintHelperGraphWriteUnitOfWorkResult Result;
	BlueprintHelperGraphWriteAddUnitOfWorkStage(Result.StageTrace, EBlueprintHelperGraphWriteUnitOfWorkStage::CaptureBefore);
	BlueprintHelperGraphWriteAddUnitOfWorkStage(Result.StageTrace, EBlueprintHelperGraphWriteUnitOfWorkStage::BuildMutationPlan);
	BlueprintHelperGraphWriteAddUnitOfWorkStage(Result.StageTrace, EBlueprintHelperGraphWriteUnitOfWorkStage::ApplyMutation);

	if (Request.ApplyMutation)
	{
		Result.ToolResult = Request.ApplyMutation();
	}
	else
	{
		FBlueprintHelperToolError Error;
		Error.Code = TEXT("unit_of_work_apply_missing");
		Error.Stage = EBlueprintHelperToolStage::Execute;
		Error.Message = TEXT("GraphWrite UnitOfWork requires an ApplyMutation callback.");
		Error.Field = TEXT("unit_of_work.apply_mutation");
		Result.ToolResult = FBlueprintHelperToolResultBuilder::Failure(
			Request.RuntimeAdapterId,
			FBlueprintHelperToolResultBuilder::GenerateTraceId(),
			Error);
	}

	if (Request.Mode == EBlueprintHelperGraphWriteUnitOfWorkMode::Preview)
	{
		BlueprintHelperGraphWriteAddUnitOfWorkStage(Result.StageTrace, EBlueprintHelperGraphWriteUnitOfWorkStage::ProjectPreview);
		if (Request.ProjectPreview)
		{
			Result.ToolResult = Request.ProjectPreview();
		}
	}
	else if (Result.ToolResult.bOk)
	{
		BlueprintHelperGraphWriteAddUnitOfWorkStage(Result.StageTrace, EBlueprintHelperGraphWriteUnitOfWorkStage::WriteOwnershipDelta);
		BlueprintHelperGraphWriteAddUnitOfWorkStage(Result.StageTrace, EBlueprintHelperGraphWriteUnitOfWorkStage::Commit);
	}
	else if (Request.Rollback)
	{
		BlueprintHelperGraphWriteAddUnitOfWorkStage(Result.StageTrace, EBlueprintHelperGraphWriteUnitOfWorkStage::Rollback);
		Request.Rollback();
		Result.bRolledBack = true;
	}

	BlueprintHelperGraphWriteAddUnitOfWorkStage(Result.StageTrace, EBlueprintHelperGraphWriteUnitOfWorkStage::BuildDiagnostics);
	BlueprintHelperGraphWriteAttachUnitOfWorkTrace(Result.ToolResult, Request, Result.StageTrace, Result.bRolledBack);
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperGraphWriteUnitOfWork::RunExistingOperation(
	EBlueprintHelperGraphWriteUnitOfWorkMode Mode,
	const FString& RuntimeAdapterId,
	const FString& TaskSpecStrategy,
	EBlueprintHelperGraphBodyKind BodyKind,
	TFunction<FBlueprintHelperToolResultBase()> ExistingOperation)
{
	FBlueprintHelperGraphWriteUnitOfWorkRequest Request;
	Request.Mode = Mode;
	Request.RuntimeAdapterId = RuntimeAdapterId;
	Request.BoundaryModel.RuntimeAdapterId = RuntimeAdapterId;
	Request.BoundaryModel.TaskSpecStrategy = TaskSpecStrategy;
	Request.BoundaryModel.BodyKind = BodyKind;
	Request.ApplyMutation = MoveTemp(ExistingOperation);
	return Run(Request).ToolResult;
}
