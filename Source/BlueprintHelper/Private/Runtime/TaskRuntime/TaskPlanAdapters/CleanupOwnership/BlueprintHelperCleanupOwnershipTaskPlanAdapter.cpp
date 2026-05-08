// BlueprintHelper TaskPlan adapter - Cleanup / rollback / ownership lifecycle capability.

#include "Runtime/TaskRuntime/TaskPlanAdapters/CleanupOwnership/BlueprintHelperCleanupOwnershipTaskPlanAdapter.h"

#include "Dom/JsonValue.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"

namespace
{
	constexpr const TCHAR* OpAliasCleanupBlueprintHelperBlock = TEXT("cleanup_blueprinthelper_block");
	constexpr const TCHAR* OpAliasConvertBlockToUserOwned = TEXT("convert_block_to_user_owned");

	FString CleanupStepFieldPath(const TCHAR* Field)
	{
		return FString::Printf(TEXT("task_plan.steps[0].%s"), Field);
	}

	FString CleanupOpFieldPath(const TCHAR* Field)
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

	bool CleanupTryReadStepTarget(
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
				CleanupStepFieldPath(TEXT("target")));
			return false;
		}

		OutTargetObject = *TargetObjectPtr;
		return true;
	}

	bool CleanupTryReadSingleLifecycleOp(
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
				CleanupStepFieldPath(TEXT("write")));
			return false;
		}

		FString Strategy;
		if (!(*WriteObjectPtr)->TryGetStringField(TEXT("strategy"), Strategy) ||
			Strategy != FBlueprintHelperCleanupOwnershipTaskPlanAdapter::StrategyOwnedBlockLifecycle)
		{
			OutError = MakeCleanupOwnershipTaskPlanError(
				TEXT("unsupported_cleanup_ownership_strategy"),
				TEXT("graph_cleanup_ownership TaskPlan step supports owned_block_lifecycle strategy only."),
				CleanupStepFieldPath(TEXT("write.strategy")));
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* OpsArray = nullptr;
		if (!(*WriteObjectPtr)->TryGetArrayField(TEXT("ops"), OpsArray) || !OpsArray)
		{
			OutError = MakeCleanupOwnershipTaskPlanError(
				TEXT("invalid_cleanup_ownership_ops"),
				TEXT("graph_cleanup_ownership TaskPlan step requires write.ops array."),
				CleanupStepFieldPath(TEXT("write.ops")));
			return false;
		}

		if (OpsArray->Num() != 1)
		{
			OutError = MakeCleanupOwnershipTaskPlanError(
				TEXT("invalid_cleanup_ownership_ops"),
				TEXT("graph_cleanup_ownership TaskPlan step currently supports exactly one lifecycle op."),
				CleanupStepFieldPath(TEXT("write.ops")));
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

	bool CleanupTryGetStringFromFirstAvailable(
		const TSharedPtr<FJsonObject>& Primary,
		const TSharedPtr<FJsonObject>& Secondary,
		const TCHAR* FieldName,
		FString& OutValue)
	{
		OutValue.Reset();
		return (Primary.IsValid() && Primary->TryGetStringField(FieldName, OutValue) && !OutValue.IsEmpty()) ||
			(Secondary.IsValid() && Secondary->TryGetStringField(FieldName, OutValue) && !OutValue.IsEmpty());
	}

	bool CleanupTryCopyStringFromFirstAvailable(
		const TSharedPtr<FJsonObject>& Primary,
		const TSharedPtr<FJsonObject>& Secondary,
		const TCHAR* SourceFieldName,
		const TSharedRef<FJsonObject>& Payload,
		const TCHAR* PayloadFieldName)
	{
		FString Value;
		if (!CleanupTryGetStringFromFirstAvailable(Primary, Secondary, SourceFieldName, Value))
		{
			return false;
		}

		Payload->SetStringField(PayloadFieldName, Value);
		return true;
	}

	FString CleanupNormalizeAdapterOperation(const FString& OpName)
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

	bool CleanupIsCleanupOperation(const FString& AdapterOperation)
	{
		return AdapterOperation == FBlueprintHelperCleanupOwnershipTaskPlanAdapter::AdapterOperationCleanupBlueprintHelperBlock;
	}

	bool CleanupIsConvertOperation(const FString& AdapterOperation)
	{
		return AdapterOperation == FBlueprintHelperCleanupOwnershipTaskPlanAdapter::AdapterOperationConvertBlueprintHelperBlockToUserOwned;
	}

	bool CleanupIsRollbackOperation(const FString& AdapterOperation)
	{
		return AdapterOperation == FBlueprintHelperCleanupOwnershipTaskPlanAdapter::AdapterOperationRollbackCleanupTransaction;
	}

	bool CleanupTryRequireAssetPathForBlockOperation(
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
				CleanupStepFieldPath(TEXT("target.asset_path")));
			return false;
		}

		Payload->SetStringField(TEXT("asset_path"), AssetPath);
		return true;
	}

	bool CleanupTryBuildBlockLifecyclePayload(
		const FString& AdapterOperation,
		const TSharedPtr<FJsonObject>& TargetObject,
		const TSharedPtr<FJsonObject>& OpObject,
		const bool bDryRun,
		TSharedRef<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		if (!CleanupTryRequireAssetPathForBlockOperation(TargetObject, Payload, OutError))
		{
			return false;
		}

		CleanupTryCopyStringFromFirstAvailable(OpObject, TargetObject, TEXT("graph"), Payload, TEXT("graph"));
		CleanupTryCopyStringFromFirstAvailable(OpObject, TargetObject, TEXT("graph_id"), Payload, TEXT("graph_id"));
		CleanupTryCopyStringFromFirstAvailable(OpObject, TargetObject, TEXT("block_ref"), Payload, TEXT("block_ref"));
		CleanupTryCopyStringFromFirstAvailable(OpObject, TargetObject, TEXT("block_id"), Payload, TEXT("block_id"));

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
				CleanupOpFieldPath(TEXT("block_id")));
			return false;
		}

		if (CleanupIsCleanupOperation(AdapterOperation))
		{
			FString CleanupScope;
			if (CleanupTryGetStringFromFirstAvailable(OpObject, TargetObject, TEXT("cleanup_scope"), CleanupScope))
			{
				if (CleanupScope != TEXT("block"))
				{
					OutError = MakeCleanupOwnershipTaskPlanError(
						TEXT("unsupported_cleanup_scope"),
						TEXT("cleanup_blueprint_helper_block supports cleanup_scope=block only."),
						CleanupOpFieldPath(TEXT("cleanup_scope")));
					return false;
				}
				Payload->SetStringField(TEXT("cleanup_scope"), CleanupScope);
			}
			CleanupTryCopyStringFromFirstAvailable(OpObject, TargetObject, TEXT("missing_policy"), Payload, TEXT("missing_policy"));
		}
		else if (CleanupIsConvertOperation(AdapterOperation))
		{
			FString OwnershipScope;
			if (CleanupTryGetStringFromFirstAvailable(OpObject, TargetObject, TEXT("ownership_scope"), OwnershipScope))
			{
				if (OwnershipScope != TEXT("block"))
				{
					OutError = MakeCleanupOwnershipTaskPlanError(
						TEXT("unsupported_ownership_scope"),
						TEXT("convert_blueprint_helper_block_to_user_owned supports ownership_scope=block only."),
						CleanupOpFieldPath(TEXT("ownership_scope")));
					return false;
				}
				Payload->SetStringField(TEXT("ownership_scope"), OwnershipScope);
			}
			CleanupTryCopyStringFromFirstAvailable(OpObject, TargetObject, TEXT("already_user_owned_policy"), Payload, TEXT("already_user_owned_policy"));
		}

		Payload->SetBoolField(TEXT("dry_run"), bDryRun);
		OutPayload = Payload;
		return true;
	}

	bool CleanupTryBuildRollbackPayload(
		const TSharedPtr<FJsonObject>& TargetObject,
		const TSharedPtr<FJsonObject>& OpObject,
		const bool bDryRun,
		TSharedRef<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();

		FString TransactionId;
		if (!CleanupTryGetStringFromFirstAvailable(OpObject, TargetObject, TEXT("transaction_id"), TransactionId))
		{
			OutError = MakeCleanupOwnershipTaskPlanError(
				TEXT("invalid_cleanup_ownership_rollback_ref"),
				TEXT("rollback_cleanup_transaction requires transaction_id."),
				CleanupOpFieldPath(TEXT("transaction_id")));
			return false;
		}
		Payload->SetStringField(TEXT("transaction_id"), TransactionId);

		CleanupTryCopyStringFromFirstAvailable(TargetObject, OpObject, TEXT("asset_path"), Payload, TEXT("asset_path"));

		FString RollbackScope;
		if (CleanupTryGetStringFromFirstAvailable(OpObject, TargetObject, TEXT("rollback_scope"), RollbackScope))
		{
			if (RollbackScope != TEXT("cleanup_transaction"))
			{
				OutError = MakeCleanupOwnershipTaskPlanError(
					TEXT("unsupported_rollback_scope"),
					TEXT("rollback_cleanup_transaction supports rollback_scope=cleanup_transaction only."),
					CleanupOpFieldPath(TEXT("rollback_scope")));
				return false;
			}
			Payload->SetStringField(TEXT("rollback_scope"), RollbackScope);
		}
		else
		{
			Payload->SetStringField(TEXT("rollback_scope"), TEXT("cleanup_transaction"));
		}

		CleanupTryCopyStringFromFirstAvailable(OpObject, TargetObject, TEXT("already_rolled_back_policy"), Payload, TEXT("already_rolled_back_policy"));

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
	return !CleanupNormalizeAdapterOperation(OpName).IsEmpty();
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
			CleanupStepFieldPath(TEXT("capability")));
		return false;
	}

	FString OperationField;
	if (StepObject->TryGetStringField(TEXT("operation"), OperationField))
	{
		OutError = MakeCleanupOwnershipTaskPlanError(
			TEXT("unsupported_cleanup_ownership_operation_field"),
			TEXT("graph_cleanup_ownership IR TaskPlan steps use capability/write; adapter operation fields are runtime lowering details."),
			CleanupStepFieldPath(TEXT("operation")));
		return false;
	}

	TSharedPtr<FJsonObject> TargetObject;
	if (!CleanupTryReadStepTarget(StepObject, TargetObject, OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> OpObject;
	if (!CleanupTryReadSingleLifecycleOp(StepObject, OpObject, OutError))
	{
		return false;
	}

	FString OpName;
	if (!OpObject->TryGetStringField(TEXT("op"), OpName) || OpName.IsEmpty())
	{
		OutError = MakeCleanupOwnershipTaskPlanError(
			TEXT("invalid_cleanup_ownership_op"),
			TEXT("graph_cleanup_ownership lifecycle op requires op name."),
			CleanupOpFieldPath(TEXT("op")));
		return false;
	}

	const FString AdapterOperation = CleanupNormalizeAdapterOperation(OpName);
	if (AdapterOperation.IsEmpty())
	{
		OutError = MakeCleanupOwnershipTaskPlanError(
			TEXT("unsupported_cleanup_ownership_op"),
			TEXT("graph_cleanup_ownership supports cleanup_blueprint_helper_block, convert_blueprint_helper_block_to_user_owned, and rollback_cleanup_transaction."),
			CleanupOpFieldPath(TEXT("op")));
		return false;
	}

	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	if (CleanupIsRollbackOperation(AdapterOperation))
	{
		if (!CleanupTryBuildRollbackPayload(TargetObject, OpObject, bDryRun, Payload, OutError))
		{
			return false;
		}
	}
	else
	{
		if (!CleanupTryBuildBlockLifecyclePayload(AdapterOperation, TargetObject, OpObject, bDryRun, Payload, OutError))
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
