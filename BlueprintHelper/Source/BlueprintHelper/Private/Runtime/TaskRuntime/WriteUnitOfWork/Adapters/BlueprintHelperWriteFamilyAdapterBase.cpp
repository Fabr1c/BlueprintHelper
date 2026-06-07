#include "Runtime/TaskRuntime/WriteUnitOfWork/Adapters/BlueprintHelperWriteFamilyAdapterBase.h"

#include "Dom/JsonObject.h"
#include "Runtime/TaskRuntime/WriteContracts/BlueprintHelperWriteFamilyDescriptor.h"

static void BlueprintHelperWriteFamilySetAdapterError(
	FBlueprintHelperToolError& OutError,
	const FString& Code,
	EBlueprintHelperToolStage Stage,
	const FString& Message,
	const FString& Field)
{
	OutError.Code = Code;
	OutError.Stage = Stage;
	OutError.Message = Message;
	OutError.Field = Field;
	OutError.bRetryable = false;
	OutError.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
}

static FString BlueprintHelperWriteFamilyMakeLifecycleOperation(
	const FBlueprintHelperWriteFamilyDescriptor& Descriptor,
	const FBlueprintHelperAcceptedPayloadModel& AcceptedPayload,
	const FString& StageId)
{
	if (!AcceptedPayload.OperationId.IsEmpty())
	{
		return FString::Printf(TEXT("%s.%s"), *AcceptedPayload.OperationId, *StageId);
	}
	if (!Descriptor.ResultProjectionPolicyId.IsEmpty())
	{
		return FString::Printf(TEXT("%s.%s"), *Descriptor.ResultProjectionPolicyId, *StageId);
	}
	return FString::Printf(TEXT("%s.%s"), *Descriptor.WriteFamily, *StageId);
}

static void BlueprintHelperWriteFamilyAttachLifecycleData(
	FBlueprintHelperToolResultBase& Result,
	const FBlueprintHelperWriteFamilyDescriptor& Descriptor,
	const FBlueprintHelperAcceptedPayloadModel& AcceptedPayload,
	const FString& StageId)
{
	if (!Result.Data.IsValid())
	{
		Result.Data = MakeShared<FJsonObject>();
	}
	Result.Data->SetStringField(TEXT("write_family_lifecycle_stage"), StageId);
	Result.Data->SetStringField(TEXT("write_family"), Descriptor.WriteFamily);
	Result.Data->SetStringField(TEXT("runtime_adapter_id"), Descriptor.RuntimeAdapterId);
	Result.Data->SetStringField(TEXT("dry_run_policy_id"), Descriptor.DryRunPolicyId);
	Result.Data->SetStringField(TEXT("readback_projection_key"), Descriptor.ReadbackProjectionMode);
	Result.Data->SetStringField(TEXT("result_projection_policy_id"), Descriptor.ResultProjectionPolicyId);
	if (!AcceptedPayload.TaskId.IsEmpty())
	{
		Result.Data->SetStringField(TEXT("task_id"), AcceptedPayload.TaskId);
	}
	if (!AcceptedPayload.OperationId.IsEmpty())
	{
		Result.Data->SetStringField(TEXT("operation_id"), AcceptedPayload.OperationId);
	}
}

static FBlueprintHelperToolResultBase BlueprintHelperWriteFamilyMakeLifecycleResult(
	const FBlueprintHelperWriteFamilyDescriptor& Descriptor,
	const FBlueprintHelperAcceptedPayloadModel& AcceptedPayload,
	const FString& StageId)
{
	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
		BlueprintHelperWriteFamilyMakeLifecycleOperation(Descriptor, AcceptedPayload, StageId),
		FBlueprintHelperToolResultBuilder::GenerateTraceId());
	BlueprintHelperWriteFamilyAttachLifecycleData(Result, Descriptor, AcceptedPayload, StageId);
	return Result;
}

static FBlueprintHelperToolResultBase BlueprintHelperWriteFamilyMakeDryRunProjectionResult(
	const FBlueprintHelperWriteFamilyDescriptor& Descriptor,
	const FBlueprintHelperAcceptedPayloadModel& AcceptedPayload)
{
	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
		BlueprintHelperWriteFamilyMakeLifecycleOperation(Descriptor, AcceptedPayload, TEXT("project_dry_run")),
		FBlueprintHelperToolResultBuilder::GenerateTraceId());
	BlueprintHelperWriteFamilyAttachLifecycleData(Result, Descriptor, AcceptedPayload, TEXT("project_dry_run"));
	Result.Data->SetStringField(TEXT("unsupported_preview_step"), TEXT(""));
	Result.Data->SetStringField(TEXT("adapter_reason_code"), TEXT("non_mutating_preview_projection"));
	return Result;
}

FBlueprintHelperWriteFamilyAdapterBase::FBlueprintHelperWriteFamilyAdapterBase(
	const FString& InWriteFamily)
{
	bDescriptorResolved =
		FBlueprintHelperWriteFamilyDescriptorRegistry::TryFindByWriteFamily(InWriteFamily, Descriptor) &&
		Descriptor.Status == EBlueprintHelperWriteFamilyCapabilityStatus::Active;
}

FString FBlueprintHelperWriteFamilyAdapterBase::GetWriteFamily() const
{
	return Descriptor.WriteFamily;
}

FString FBlueprintHelperWriteFamilyAdapterBase::GetRuntimeAdapterId() const
{
	return Descriptor.RuntimeAdapterId;
}

bool FBlueprintHelperWriteFamilyAdapterBase::BuildPreflight(
	const FBlueprintHelperAcceptedPayloadModel& AcceptedPayload,
	FBlueprintHelperWriteUnitOfWorkRequest& OutRequest,
	FBlueprintHelperToolError& OutError) const
{
	if (!ValidateDescriptor(OutError) || !ValidateAcceptedPayload(AcceptedPayload, OutError))
	{
		return false;
	}

	OutRequest.AcceptedPayload = AcceptedPayload;
	OutRequest.Descriptor = Descriptor;
	OutRequest.CaptureBefore = [Descriptor = Descriptor, AcceptedPayload]()
	{
		return BlueprintHelperWriteFamilyMakeLifecycleResult(
			Descriptor,
			AcceptedPayload,
			TEXT("capture_before"));
	};
	return true;
}

bool FBlueprintHelperWriteFamilyAdapterBase::BuildMutationPlan(
	const FBlueprintHelperAcceptedPayloadModel& AcceptedPayload,
	FBlueprintHelperWriteUnitOfWorkRequest& OutRequest,
	FBlueprintHelperToolError& OutError) const
{
	if (!ValidateDescriptor(OutError) || !ValidateAcceptedPayload(AcceptedPayload, OutError))
	{
		return false;
	}
	if (!OutRequest.ApplyMutation)
	{
		BlueprintHelperWriteFamilySetAdapterError(
			OutError,
			TEXT("write_family_mutation_executor_missing"),
			EBlueprintHelperToolStage::Execute,
			TEXT("Write-family adapter requires a runtime mutation executor before building ApplyMutation."),
			TEXT("apply_mutation"));
		return false;
	}

	OutRequest.BuildFamilyMutationPlan = [Descriptor = Descriptor, AcceptedPayload]()
	{
		return BlueprintHelperWriteFamilyMakeLifecycleResult(
			Descriptor,
			AcceptedPayload,
			TEXT("build_family_mutation_plan"));
	};

	TFunction<FBlueprintHelperToolResultBase()> RuntimeMutation = OutRequest.ApplyMutation;
	OutRequest.ApplyMutation = [Descriptor = Descriptor, AcceptedPayload, RuntimeMutation]()
	{
		FBlueprintHelperToolResultBase Result = RuntimeMutation();
		BlueprintHelperWriteFamilyAttachLifecycleData(
			Result,
			Descriptor,
			AcceptedPayload,
			TEXT("apply_mutation"));
		return Result;
	};
	return true;
}

bool FBlueprintHelperWriteFamilyAdapterBase::BuildDryRunProjection(
	const FBlueprintHelperAcceptedPayloadModel& AcceptedPayload,
	FBlueprintHelperWriteUnitOfWorkRequest& OutRequest,
	FBlueprintHelperToolError& OutError) const
{
	if (!ValidateDescriptor(OutError) || !ValidateAcceptedPayload(AcceptedPayload, OutError))
	{
		return false;
	}

	OutRequest.ProjectDryRun = [Descriptor = Descriptor, AcceptedPayload]()
	{
		return BlueprintHelperWriteFamilyMakeDryRunProjectionResult(Descriptor, AcceptedPayload);
	};
	return true;
}

bool FBlueprintHelperWriteFamilyAdapterBase::BuildReviewAndReadback(
	const FBlueprintHelperAcceptedPayloadModel& AcceptedPayload,
	FBlueprintHelperWriteUnitOfWorkRequest& OutRequest,
	FBlueprintHelperToolError& OutError) const
{
	if (!ValidateDescriptor(OutError) || !ValidateAcceptedPayload(AcceptedPayload, OutError))
	{
		return false;
	}

	OutRequest.RecordOwnershipDelta = [Descriptor = Descriptor, AcceptedPayload]()
	{
		return BlueprintHelperWriteFamilyMakeLifecycleResult(
			Descriptor,
			AcceptedPayload,
			TEXT("record_ownership_delta"));
	};
	OutRequest.Commit = [Descriptor = Descriptor, AcceptedPayload]()
	{
		return BlueprintHelperWriteFamilyMakeLifecycleResult(Descriptor, AcceptedPayload, TEXT("commit"));
	};
	OutRequest.Rollback = [Descriptor = Descriptor, AcceptedPayload]()
	{
		return BlueprintHelperWriteFamilyMakeLifecycleResult(Descriptor, AcceptedPayload, TEXT("rollback"));
	};
	OutRequest.BuildDiagnostics = [Descriptor = Descriptor, AcceptedPayload]()
	{
		return BlueprintHelperWriteFamilyMakeLifecycleResult(
			Descriptor,
			AcceptedPayload,
			TEXT("build_diagnostics"));
	};
	return true;
}

const FBlueprintHelperWriteFamilyDescriptor&
FBlueprintHelperWriteFamilyAdapterBase::GetDescriptor() const
{
	return Descriptor;
}

bool FBlueprintHelperWriteFamilyAdapterBase::ValidateDescriptor(
	FBlueprintHelperToolError& OutError) const
{
	if (bDescriptorResolved &&
		!Descriptor.WriteFamily.IsEmpty() &&
		!Descriptor.RuntimeAdapterId.IsEmpty())
	{
		return true;
	}

	BlueprintHelperWriteFamilySetAdapterError(
		OutError,
		TEXT("write_family_descriptor_invalid"),
		EBlueprintHelperToolStage::ParseInput,
		TEXT("Write-family adapter must resolve an active descriptor with write_family and runtime_adapter_id."),
		TEXT("write_family_descriptor"));
	return false;
}

bool FBlueprintHelperWriteFamilyAdapterBase::ValidateAcceptedPayload(
	const FBlueprintHelperAcceptedPayloadModel& AcceptedPayload,
	FBlueprintHelperToolError& OutError) const
{
	if (AcceptedPayload.WriteFamily.IsEmpty() ||
		AcceptedPayload.WriteFamily.Equals(Descriptor.WriteFamily, ESearchCase::IgnoreCase))
	{
		return true;
	}

	BlueprintHelperWriteFamilySetAdapterError(
		OutError,
		TEXT("write_family_payload_mismatch"),
		EBlueprintHelperToolStage::ParseInput,
		TEXT("Accepted payload write_family does not match the resolved adapter family."),
		TEXT("accepted_payload.write_family"));
	OutError.Expected = Descriptor.WriteFamily;
	OutError.Actual = AcceptedPayload.WriteFamily;
	return false;
}
