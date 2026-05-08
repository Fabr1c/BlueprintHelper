// BlueprintHelper TaskPlan adapter - Cleanup / rollback / ownership lifecycle capability.

#include "Runtime/TaskRuntime/TaskPlanAdapters/CleanupOwnership/BlueprintHelperCleanupOwnershipTaskPlanAdapter.h"

#include "Dom/JsonValue.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"

namespace
{
	constexpr const TCHAR* OpAliasCleanupBlueprintHelperBlock = TEXT("cleanup_blueprinthelper_block");
	constexpr const TCHAR* OpAliasConvertBlockToUserOwned = TEXT("convert_block_to_user_owned");

	FString StepFieldPath(const TCHAR* Field)
	{
		return FString::Printf(TEXT("task_plan.steps[0].%s"), Field);
	}

	FString OpFieldPath(const TCHAR* Field)
	{
		return FString::Printf(TEXT("task_plan.steps[0].write.ops[0].%s"), Field);
	}

	FBlueprintHelperToolError MakeCleanupOwnershipTaskPlanError(
		const FString& Code,
		const FString& Message,
		const FString& Field)
	{
		FBlueprintHelperToolError Error;
		Error.Code = Code;
		Error.Stage = EBlueprintHelperToolStage::ParseInput;
		Error.Message = Message;
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		Error.Field = Field;
		return Error;
	}

	bool TryReadStepTarget(
		const TSharedPtr<FJsonObject>& StepObject,
		TSharedPtr<FJsonObject>& OutTargetObject,
		FBlueprintHelperToolError& OutError)
	{
		const TSharedPtr<FJsonObject>* TargetObjectPtr = nullptr;
		if (!StepObject.IsValid() ||
			!StepObject->TryGetObjectField(TEXT("target"), TargetObjectPtr) ||
			!TargetObjectPtr || !TargetObjectPtr->IsValid())
		{
			OutError = MakeCleanupOwnershipTaskPlanError(
				TEXT("invalid_cleanup_ownership_target"),
				TEXT("graph_cleanup_ownership TaskPlan step requires target object."),
				StepFieldPath(TEXT("target")));
			return false;
		}

		OutTargetObject = *TargetObjectPtr;
		return true;
	}

	bool TryReadSingleLifecycleOp(
		const TSharedPtr<FJsonObject>& StepObject,
		TSharedPtr<FJsonObject>& OutOpObject,
		FBlueprintHelperToolError& OutError)
	{
		const TSharedPtr<FJsonObject>* WriteObjectPtr = nullptr;
		if (!StepObject.IsValid() ||
			!StepObject->TryGetObjectField(TEXT("write"), WriteObjectPtr) ||
			!WriteObjectPtr || !WriteObjectPtr->IsValid())
		{
			OutError = MakeCleanupOwnershipTaskPlanError(
				TEXT("invalid_cleanup_ownership_write"),
				TEXT("graph_cleanup_ownership TaskPlan step requires write object."),
				StepFieldPath(TEXT("write")));
			return false;
		}

		FString Strategy;
		if (!(*WriteObjectPtr)->TryGetStringField(TEXT("strategy"), Strategy) ||
			Strategy != FBlueprintHelperCleanupOwnershipTaskPlanAdapter::StrategyOwnedBlockLifecycle)
		{
			OutError = MakeCleanupOwnershipTaskPlanError(
				TEXT("unsupported_cleanup_ownership_strategy"),
				TEXT("graph_cleanup_ownership TaskPlan step supports owned_block_lifecycle strategy only."),
				StepFieldPath(TEXT("write.strategy")));
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* OpsArray = nullptr;
		if (!(*WriteObjectPtr)->TryGetArrayField(TEXT("ops"), OpsArray) || !OpsArray)
		{
			OutError = MakeCleanupOwnershipTaskPlanError(
				TEXT("invalid_cleanup_ownership_ops"),
				TEXT("graph_cleanup_ownership TaskPlan step requires write.ops array."),
				StepFieldPath(TEXT("write.ops")));
			return false;
		}

		if (OpsArray->Num() != 1)
		{
			OutError = MakeCleanupOwnershipTaskPlanError(
				TEXT("invalid_cleanup_ownership_ops"),
				TEXT("graph_cleanup_ownership TaskPlan step currently supports exactly one lifecycle op."),
				StepFieldPath(TEXT("write.ops")));
			return false;
		}

		OutOpObject = (*OpsArray)[0].IsValid() ? (*OpsArray)[0]->AsObject() : nullptr;
		if (!OutOpObject.IsValid())
		{
			OutError = MakeCleanupOwnershipTaskPlanError(
				TEXT("invalid_cleanup_ownership_op"),
				TEXT("graph_cleanup_ownership write op must be an object."),
				TEXT("task_plan.steps[0].write.ops[0]"));
			return false;
		}

		return true;
	}

	bool TryGetStringFromFirstAvailable(
		const TSharedPtr<FJsonObject>& Primary,
		const TSharedPtr<FJsonObject>& Secondary,
		const TCHAR* FieldName,
		FString& OutValue)
	{
		OutValue.Reset();
		return (Primary.IsValid() && Primary->TryGetStringField(FieldName, OutValue) && !OutValue.IsEmpty()) ||
			(Secondary.IsValid() && Secondary->TryGetStringField(FieldName, OutValue) && !OutValue.IsEmpty());
	}

	bool TryCopyStringFromFirstAvailable(
		const TSharedPtr<FJsonObject>& Primary,
		const TSharedPtr<FJsonObject>& Secondary,
		const TCHAR* SourceFieldName,
		const TSharedRef<FJsonObject>& Payload,
		const TCHAR* PayloadFieldName)
	{
		FString Value;
		if (!TryGetStringFromFirstAvailable(Primary, Secondary, SourceFieldName, Value))
		{
			return false;
		}

		Payload->SetStringField(PayloadFieldName, Value);
		return true;
	}

	FString NormalizeAdapterOperation(const FString& OpName)
	{
		if (OpName == OpAliasCleanupBlueprintHelperBlock ||
			OpName == FBlueprintHelperCleanupOwnershipTaskPlanAdapter::AdapterOperationCleanupBlueprintHelperBlock)
		{
			return FBlueprintHelperCleanupOwnershipTaskPlanAdapter::AdapterOperationCleanupBlueprintHelperBlock;
		}

		if (OpName == OpAliasConvertBlockToUserOwned ||
			OpName == FBlueprintHelperCleanupOwnershipTaskPlanAdapter::AdapterOperationConvertBlueprintHelperBlockToUserOwned)
		{
			return FBlueprintHelperCleanupOwnershipTaskPlanAdapter::AdapterOperationConvertBlueprintHelperBlockToUserOwned;
		}

		if (OpName == FBlueprintHelperCleanupOwnershipTaskPlanAdapter::AdapterOperationRollbackCleanupTransaction)
		{
			return FBlueprintHelperCleanupOwnershipTaskPlanAdapter::AdapterOperationRollbackCleanupTransaction;
		}

		return FString();
	}

	bool IsCleanupOperation(const FString& AdapterOperation)
	{
		return AdapterOperation == FBlueprintHelperCleanupOwnershipTaskPlanAdapter::AdapterOperationCleanupBlueprintHelperBlock;
	}

	bool IsConvertOperation(const FString& AdapterOperation)
	{
		return AdapterOperation == FBlueprintHelperCleanupOwnershipTaskPlanAdapter::AdapterOperationConvertBlueprintHelperBlockToUserOwned;
	}

	bool IsRollbackOperation(const FString& AdapterOperation)
	{
		return AdapterOperation == FBlueprintHelperCleanupOwnershipTaskPlanAdapter::AdapterOperationRollbackCleanupTransaction;
	}

	bool TryRequireAssetPathForBlockOperation(
		const TSharedPtr<FJsonObject>& TargetObject,
		const TSharedRef<FJsonObject>& Payload,
		FBlueprintHelperToolError& OutError)
	{
		FString AssetPath;
		if (!TargetObject.IsValid() ||
			!TargetObject->TryGetStringField(TEXT("asset_path"), AssetPath) ||
			AssetPath.IsEmpty())
		{
			OutError = MakeCleanupOwnershipTaskPlanError(
				TEXT("invalid_cleanup_ownership_target"),
				TEXT("graph_cleanup_ownership block lifecycle op requires target.asset_path."),
				StepFieldPath(TEXT("target.asset_path")));
			return false;
		}

		Payload->SetStringField(TEXT("asset_path"), AssetPath);
		return true;
	}

	bool TryBuildBlockLifecyclePayload(
		const FString& AdapterOperation,
		const TSharedPtr<FJsonObject>& TargetObject,
		const TSharedPtr<FJsonObject>& OpObject,
		const bool bDryRun,
		TSharedRef<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		if (!TryRequireAssetPathForBlockOperation(TargetObject, Payload, OutError))
		{
			return false;
		}

		TryCopyStringFromFirstAvailable(OpObject, TargetObject, TEXT("graph"), Payload, TEXT("graph"));
		TryCopyStringFromFirstAvailable(OpObject, TargetObject, TEXT("graph_id"), Payload, TEXT("graph_id"));
		TryCopyStringFromFirstAvailable(OpObject, TargetObject, TEXT("block_ref"), Payload, TEXT("block_ref"));
		TryCopyStringFromFirstAvailable(OpObject, TargetObject, TEXT("block_id"), Payload, TEXT("block_id"));

		FString BlockId;
		FString BlockRef;
		FString GraphName;
		FString GraphId;
		Payload->TryGetStringField(TEXT("block_id"), BlockId);
		Payload->TryGetStringField(TEXT("block_ref"), BlockRef);
		Payload->TryGetStringField(TEXT("graph"), GraphName);
		Payload->TryGetStringField(TEXT("graph_id"), GraphId);
		if (BlockId.IsEmpty() && (BlockRef.IsEmpty() || (GraphName.IsEmpty() && GraphId.IsEmpty())))
		{
			OutError = MakeCleanupOwnershipTaskPlanError(
				TEXT("invalid_cleanup_ownership_block_ref"),
				TEXT("Block lifecycle ops require block_id or graph/graph_id plus block_ref."),
				OpFieldPath(TEXT("block_id")));
			return false;
		}

		if (IsCleanupOperation(AdapterOperation))
		{
			FString CleanupScope;
			if (TryGetStringFromFirstAvailable(OpObject, TargetObject, TEXT("cleanup_scope"), CleanupScope))
			{
				if (CleanupScope != TEXT("block"))
				{
					OutError = MakeCleanupOwnershipTaskPlanError(
						TEXT("unsupported_cleanup_scope"),
						TEXT("cleanup_blueprint_helper_block supports cleanup_scope=block only."),
						OpFieldPath(TEXT("cleanup_scope")));
					return false;
				}
				Payload->SetStringField(TEXT("cleanup_scope"), CleanupScope);
			}
			TryCopyStringFromFirstAvailable(OpObject, TargetObject, TEXT("missing_policy"), Payload, TEXT("missing_policy"));
		}
		else if (IsConvertOperation(AdapterOperation))
		{
			FString OwnershipScope;
			if (TryGetStringFromFirstAvailable(OpObject, TargetObject, TEXT("ownership_scope"), OwnershipScope))
			{
				if (OwnershipScope != TEXT("block"))
				{
					OutError = MakeCleanupOwnershipTaskPlanError(
						TEXT("unsupported_ownership_scope"),
						TEXT("convert_blueprint_helper_block_to_user_owned supports ownership_scope=block only."),
						OpFieldPath(TEXT("ownership_scope")));
					return false;
				}
				Payload->SetStringField(TEXT("ownership_scope"), OwnershipScope);
			}
			TryCopyStringFromFirstAvailable(OpObject, TargetObject, TEXT("already_user_owned_policy"), Payload, TEXT("already_user_owned_policy"));
		}

		Payload->SetBoolField(TEXT("dry_run"), bDryRun);
		OutPayload = Payload;
		return true;
	}

	bool TryBuildRollbackPayload(
		const TSharedPtr<FJsonObject>& TargetObject,
		const TSharedPtr<FJsonObject>& OpObject,
		const bool bDryRun,
		TSharedRef<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();

		FString TransactionId;
		if (!TryGetStringFromFirstAvailable(OpObject, TargetObject, TEXT("transaction_id"), TransactionId))
		{
			OutError = MakeCleanupOwnershipTaskPlanError(
				TEXT("invalid_cleanup_ownership_rollback_ref"),
				TEXT("rollback_cleanup_transaction requires transaction_id."),
				OpFieldPath(TEXT("transaction_id")));
			return false;
		}
		Payload->SetStringField(TEXT("transaction_id"), TransactionId);

		TryCopyStringFromFirstAvailable(TargetObject, OpObject, TEXT("asset_path"), Payload, TEXT("asset_path"));

		FString RollbackScope;
		if (TryGetStringFromFirstAvailable(OpObject, TargetObject, TEXT("rollback_scope"), RollbackScope))
		{
			if (RollbackScope != TEXT("cleanup_transaction"))
			{
				OutError = MakeCleanupOwnershipTaskPlanError(
					TEXT("unsupported_rollback_scope"),
					TEXT("rollback_cleanup_transaction supports rollback_scope=cleanup_transaction only."),
					OpFieldPath(TEXT("rollback_scope")));
				return false;
			}
			Payload->SetStringField(TEXT("rollback_scope"), RollbackScope);
		}
		else
		{
			Payload->SetStringField(TEXT("rollback_scope"), TEXT("cleanup_transaction"));
		}

		TryCopyStringFromFirstAvailable(OpObject, TargetObject, TEXT("already_rolled_back_policy"), Payload, TEXT("already_rolled_back_policy"));

		Payload->SetBoolField(TEXT("dry_run"), bDryRun);
		OutPayload = Payload;
		return true;
	}
}

bool FBlueprintHelperCleanupOwnershipTaskPlanAdapter::IsSupportedCapability(const FString& Capability)
{
	return Capability == CapabilityName;
}

bool FBlueprintHelperCleanupOwnershipTaskPlanAdapter::IsSupportedOp(const FString& OpName)
{
	return !NormalizeAdapterOperation(OpName).IsEmpty();
}

bool FBlueprintHelperCleanupOwnershipTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
	const TSharedPtr<FJsonObject>& StepObject,
	bool bDryRun,
	FBlueprintHelperCleanupOwnershipTaskPlanPayload& OutPayload,
	FBlueprintHelperToolError& OutError)
{
	OutPayload = FBlueprintHelperCleanupOwnershipTaskPlanPayload();
	OutError = FBlueprintHelperToolError();

	if (!StepObject.IsValid())
	{
		OutError = MakeCleanupOwnershipTaskPlanError(
			TEXT("invalid_cleanup_ownership_step"),
			TEXT("graph_cleanup_ownership TaskPlan step must be an object."),
			TEXT("task_plan.steps[0]"));
		return false;
	}

	StepObject->TryGetStringField(TEXT("step_id"), OutPayload.StepId);
	if (OutPayload.StepId.IsEmpty())
	{
		OutPayload.StepId = TEXT("step_001");
	}

	FString Capability;
	StepObject->TryGetStringField(TEXT("capability"), Capability);
	if (!IsSupportedCapability(Capability))
	{
		OutError = MakeCleanupOwnershipTaskPlanError(
			TEXT("unsupported_cleanup_ownership_capability"),
			TEXT("CleanupOwnership adapter requires capability=graph_cleanup_ownership."),
			StepFieldPath(TEXT("capability")));
		return false;
	}

	FString OperationField;
	if (StepObject->TryGetStringField(TEXT("operation"), OperationField))
	{
		OutError = MakeCleanupOwnershipTaskPlanError(
			TEXT("unsupported_cleanup_ownership_operation_field"),
			TEXT("graph_cleanup_ownership IR TaskPlan steps use capability/write; adapter operation fields are runtime lowering details."),
			StepFieldPath(TEXT("operation")));
		return false;
	}

	TSharedPtr<FJsonObject> TargetObject;
	if (!TryReadStepTarget(StepObject, TargetObject, OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> OpObject;
	if (!TryReadSingleLifecycleOp(StepObject, OpObject, OutError))
	{
		return false;
	}

	FString OpName;
	if (!OpObject->TryGetStringField(TEXT("op"), OpName) || OpName.IsEmpty())
	{
		OutError = MakeCleanupOwnershipTaskPlanError(
			TEXT("invalid_cleanup_ownership_op"),
			TEXT("graph_cleanup_ownership lifecycle op requires op name."),
			OpFieldPath(TEXT("op")));
		return false;
	}

	const FString AdapterOperation = NormalizeAdapterOperation(OpName);
	if (AdapterOperation.IsEmpty())
	{
		OutError = MakeCleanupOwnershipTaskPlanError(
			TEXT("unsupported_cleanup_ownership_op"),
			TEXT("graph_cleanup_ownership supports cleanup_blueprint_helper_block, convert_blueprint_helper_block_to_user_owned, and rollback_cleanup_transaction."),
			OpFieldPath(TEXT("op")));
		return false;
	}

	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	if (IsRollbackOperation(AdapterOperation))
	{
		if (!TryBuildRollbackPayload(TargetObject, OpObject, bDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else
	{
		if (!TryBuildBlockLifecyclePayload(AdapterOperation, TargetObject, OpObject, bDryRun, Payload, OutError))
		{
			return false;
		}
	}

	OutPayload.Capability = CapabilityName;
	OutPayload.RuntimeOperation = RuntimeOperationName;
	OutPayload.AdapterOperation = AdapterOperation;
	OutPayload.Payload = Payload;
	OutPayload.bAdapterDryRunSupported = true;
	return true;
}

bool FBlueprintHelperCleanupOwnershipTaskPlanAdapter::TryLowerTaskPlanStep(
	const TSharedPtr<FJsonObject>& TaskPlan,
	const TSharedPtr<FJsonObject>& StepObject,
	bool bDryRun,
	FBlueprintHelperTaskRuntimeLoweredStep& OutLoweredStep,
	FBlueprintHelperToolError& OutError)
{
	static_cast<void>(TaskPlan);
	OutLoweredStep = FBlueprintHelperTaskRuntimeLoweredStep();

	FBlueprintHelperCleanupOwnershipTaskPlanPayload BuiltPayload;
	if (!TryBuildPayloadFromTaskPlanStep(StepObject, bDryRun, BuiltPayload, OutError))
	{
		return false;
	}

	OutLoweredStep.StepId = BuiltPayload.StepId;
	OutLoweredStep.Capability = BuiltPayload.Capability;
	OutLoweredStep.RuntimeOperation = BuiltPayload.RuntimeOperation;
	OutLoweredStep.AdapterOperation = BuiltPayload.AdapterOperation;
	OutLoweredStep.Payload = BuiltPayload.Payload;
	OutLoweredStep.bAdapterDryRunSupported = BuiltPayload.bAdapterDryRunSupported;
	return true;
}
