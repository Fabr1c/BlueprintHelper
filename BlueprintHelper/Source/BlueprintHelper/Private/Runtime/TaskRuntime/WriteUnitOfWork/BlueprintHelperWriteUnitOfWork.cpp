#include "Runtime/TaskRuntime/WriteUnitOfWork/BlueprintHelperWriteUnitOfWork.h"

#include "Dom/JsonValue.h"

class FBlueprintHelperWriteUnitOfWorkRunner
{
public:
	static void AddStage(
		FBlueprintHelperWriteUnitOfWorkResult& Result,
		EBlueprintHelperWriteUnitOfWorkStage Stage)
	{
		Result.StageTrace.Add(FBlueprintHelperWriteUnitOfWork::StageToString(Stage));
	}

	static bool RunOptionalStage(
		FBlueprintHelperWriteUnitOfWorkResult& Result,
		EBlueprintHelperWriteUnitOfWorkStage Stage,
		const TFunction<FBlueprintHelperToolResultBase()>& Callback,
		bool bReplaceResult)
	{
		AddStage(Result, Stage);
		if (!Callback)
		{
			return true;
		}

		FBlueprintHelperToolResultBase StageResult = Callback();
		if (bReplaceResult)
		{
			Result.ToolResult = StageResult;
		}
		return StageResult.bOk;
	}
};

FString FBlueprintHelperWriteUnitOfWork::StageToString(EBlueprintHelperWriteUnitOfWorkStage Stage)
{
	switch (Stage)
	{
	case EBlueprintHelperWriteUnitOfWorkStage::CaptureBefore:
		return TEXT("capture_before");
	case EBlueprintHelperWriteUnitOfWorkStage::BuildFamilyMutationPlan:
		return TEXT("build_family_mutation_plan");
	case EBlueprintHelperWriteUnitOfWorkStage::ApplyMutation:
		return TEXT("apply_mutation");
	case EBlueprintHelperWriteUnitOfWorkStage::ProjectDryRun:
		return TEXT("project_dry_run");
	case EBlueprintHelperWriteUnitOfWorkStage::RecordOwnershipDelta:
		return TEXT("record_ownership_delta");
	case EBlueprintHelperWriteUnitOfWorkStage::Commit:
		return TEXT("commit");
	case EBlueprintHelperWriteUnitOfWorkStage::Rollback:
		return TEXT("rollback");
	case EBlueprintHelperWriteUnitOfWorkStage::BuildDiagnostics:
		return TEXT("build_diagnostics");
	default:
		return TEXT("unknown");
	}
}

FBlueprintHelperWriteUnitOfWorkResult FBlueprintHelperWriteUnitOfWork::Run(
	const FBlueprintHelperWriteUnitOfWorkRequest& Request)
{
	FBlueprintHelperWriteUnitOfWorkResult Result;
	Result.WriteFamily = Request.Descriptor.WriteFamily;
	Result.RuntimeAdapterId = Request.Descriptor.RuntimeAdapterId;

	FBlueprintHelperWriteUnitOfWorkRunner::RunOptionalStage(
		Result,
		EBlueprintHelperWriteUnitOfWorkStage::CaptureBefore,
		Request.CaptureBefore,
		false);
	FBlueprintHelperWriteUnitOfWorkRunner::RunOptionalStage(
		Result,
		EBlueprintHelperWriteUnitOfWorkStage::BuildFamilyMutationPlan,
		Request.BuildFamilyMutationPlan,
		false);
	FBlueprintHelperWriteUnitOfWorkRunner::RunOptionalStage(
		Result,
		EBlueprintHelperWriteUnitOfWorkStage::ApplyMutation,
		Request.ApplyMutation,
		true);

	if (!Result.ToolResult.Data.IsValid() && !Result.ToolResult.bOk)
	{
		Result.ToolResult.Data = MakeShared<FJsonObject>();
	}

	if (Request.Mode == EBlueprintHelperWriteUnitOfWorkMode::Preview)
	{
		FBlueprintHelperWriteUnitOfWorkRunner::RunOptionalStage(
			Result,
			EBlueprintHelperWriteUnitOfWorkStage::ProjectDryRun,
			Request.ProjectDryRun,
			false);
	}
	else if (Result.ToolResult.bOk)
	{
		FBlueprintHelperWriteUnitOfWorkRunner::RunOptionalStage(
			Result,
			EBlueprintHelperWriteUnitOfWorkStage::RecordOwnershipDelta,
			Request.RecordOwnershipDelta,
			false);
		FBlueprintHelperWriteUnitOfWorkRunner::RunOptionalStage(
			Result,
			EBlueprintHelperWriteUnitOfWorkStage::Commit,
			Request.Commit,
			false);
		Result.CommitState = TEXT("committed");
	}
	else
	{
		FBlueprintHelperWriteUnitOfWorkRunner::RunOptionalStage(
			Result,
			EBlueprintHelperWriteUnitOfWorkStage::Rollback,
			Request.Rollback,
			false);
		Result.bRolledBack = true;
		Result.RollbackState = TEXT("rolled_back");
	}

	FBlueprintHelperWriteUnitOfWorkRunner::RunOptionalStage(
		Result,
		EBlueprintHelperWriteUnitOfWorkStage::BuildDiagnostics,
		Request.BuildDiagnostics,
		false);

	AttachUnitOfWorkData(Result.ToolResult, Request, Result);
	return Result;
}

void FBlueprintHelperWriteUnitOfWork::AttachUnitOfWorkData(
	FBlueprintHelperToolResultBase& ToolResult,
	const FBlueprintHelperWriteUnitOfWorkRequest& Request,
	const FBlueprintHelperWriteUnitOfWorkResult& Result)
{
	if (!ToolResult.Data.IsValid())
	{
		ToolResult.Data = MakeShared<FJsonObject>();
	}

	TArray<TSharedPtr<FJsonValue>> StageValues;
	for (const FString& Stage : Result.StageTrace)
	{
		StageValues.Add(MakeShared<FJsonValueString>(Stage));
	}
	ToolResult.Data->SetArrayField(TEXT("unit_of_work_stage_trace"), MoveTemp(StageValues));
	ToolResult.Data->SetStringField(TEXT("write_family"), Request.Descriptor.WriteFamily);
	ToolResult.Data->SetStringField(TEXT("runtime_adapter_id"), Request.Descriptor.RuntimeAdapterId);
	ToolResult.Data->SetStringField(TEXT("dry_run_policy_id"), Request.Descriptor.DryRunPolicyId);
	ToolResult.Data->SetStringField(TEXT("readback_projection_key"), Request.Descriptor.ReadbackProjectionMode);
	ToolResult.Data->SetStringField(TEXT("result_projection_policy_id"), Request.Descriptor.ResultProjectionPolicyId);
	ToolResult.Data->SetStringField(
		TEXT("unit_of_work_mode"),
		Request.Mode == EBlueprintHelperWriteUnitOfWorkMode::Preview ? TEXT("preview") : TEXT("execute"));
	ToolResult.Data->SetBoolField(TEXT("unit_of_work_rolled_back"), Result.bRolledBack);
	if (!Result.CommitState.IsEmpty())
	{
		ToolResult.Data->SetStringField(TEXT("unit_of_work_commit_state"), Result.CommitState);
	}
	if (!Result.RollbackState.IsEmpty())
	{
		ToolResult.Data->SetStringField(TEXT("unit_of_work_rollback_state"), Result.RollbackState);
	}
	ToolResult.Data->SetObjectField(
		TEXT("accepted_payload"),
		FBlueprintHelperAcceptedPayloadModelUtils::ToJson(Request.AcceptedPayload));
}
