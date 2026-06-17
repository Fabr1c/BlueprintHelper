// BlueprintHelper MaterialInstance TaskPlan lowering adapter.

#include "Runtime/TaskRuntime/TaskPlanAdapters/MaterialInstance/BlueprintHelperMaterialInstanceTaskPlanAdapter.h"

#include "Dom/JsonValue.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"

class FBlueprintHelperMaterialInstanceTaskPlanAdapterLocalUtils
{
public:
	static FBlueprintHelperToolError MakeError(
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

	static bool TryReadTargetAssetPath(
		const TSharedPtr<FJsonObject>& StepObject,
		FString& OutAssetPath,
		FBlueprintHelperToolError& OutError)
	{
		const TSharedPtr<FJsonObject>* TargetObjectPtr = nullptr;
		if (!StepObject.IsValid() ||
			!StepObject->TryGetObjectField(TEXT("target"), TargetObjectPtr) ||
			!TargetObjectPtr || !TargetObjectPtr->IsValid())
		{
			OutError = MakeError(
				TEXT("invalid_material_instance_target"),
				TEXT("material_instance TaskPlan step target object is required."),
				TEXT("task_plan.steps[0].target"));
			return false;
		}

		if (!(*TargetObjectPtr)->TryGetStringField(TEXT("asset_path"), OutAssetPath) || OutAssetPath.IsEmpty())
		{
			OutError = MakeError(
				TEXT("invalid_material_instance_target"),
				TEXT("material_instance TaskPlan step target requires asset_path."),
				TEXT("task_plan.steps[0].target.asset_path"));
			return false;
		}
		return true;
	}

	static bool TryReadOps(
		const TSharedPtr<FJsonObject>& StepObject,
		const TArray<TSharedPtr<FJsonValue>>*& OutOpsArray,
		FBlueprintHelperToolError& OutError)
	{
		const TSharedPtr<FJsonObject>* WriteObjectPtr = nullptr;
		if (!StepObject.IsValid() ||
			!StepObject->TryGetObjectField(TEXT("write"), WriteObjectPtr) ||
			!WriteObjectPtr || !WriteObjectPtr->IsValid())
		{
			OutError = MakeError(
				TEXT("invalid_material_instance_write"),
				TEXT("material_instance TaskPlan step requires write object."),
				TEXT("task_plan.steps[0].write"));
			return false;
		}

		FString Strategy;
		if (!(*WriteObjectPtr)->TryGetStringField(TEXT("strategy"), Strategy) ||
			Strategy != FBlueprintHelperMaterialInstanceTaskPlanAdapter::StrategyMaterialInstanceEdit)
		{
			OutError = MakeError(
				TEXT("unsupported_material_instance_strategy"),
				TEXT("MaterialInstance TaskPlan adapter supports material_instance_edit only."),
				TEXT("task_plan.steps[0].write.strategy"));
			return false;
		}

		if (!(*WriteObjectPtr)->TryGetArrayField(TEXT("ops"), OutOpsArray) ||
			!OutOpsArray ||
			OutOpsArray->Num() == 0)
		{
			OutError = MakeError(
				TEXT("invalid_material_instance_ops"),
				TEXT("material_instance TaskPlan step requires non-empty write.ops array."),
				TEXT("task_plan.steps[0].write.ops"));
			return false;
		}

		for (int32 Index = 0; Index < OutOpsArray->Num(); ++Index)
		{
			if (!(*OutOpsArray)[Index].IsValid() || !(*OutOpsArray)[Index]->AsObject().IsValid())
			{
				OutError = MakeError(
					TEXT("invalid_material_instance_op"),
					TEXT("material_instance write op must be an object."),
					FString::Printf(TEXT("task_plan.steps[0].write.ops[%d]"), Index));
				return false;
			}
		}
		return true;
	}
};

bool FBlueprintHelperMaterialInstanceTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
	const TSharedPtr<FJsonObject>& StepObject,
	bool bDryRun,
	FBlueprintHelperMaterialInstanceTaskPlanPayload& OutPayload,
	FBlueprintHelperToolError& OutError)
{
	OutPayload = FBlueprintHelperMaterialInstanceTaskPlanPayload();
	OutError = FBlueprintHelperToolError();

	if (!StepObject.IsValid())
	{
		OutError = FBlueprintHelperMaterialInstanceTaskPlanAdapterLocalUtils::MakeError(
			TEXT("invalid_material_instance_step"),
			TEXT("MaterialInstance TaskPlan step must be an object."),
			TEXT("task_plan.steps[0]"));
		return false;
	}

	StepObject->TryGetStringField(TEXT("step_id"), OutPayload.StepId);
	if (OutPayload.StepId.IsEmpty())
	{
		OutPayload.StepId = TEXT("step_material_instance");
	}

	FString Capability;
	StepObject->TryGetStringField(TEXT("capability"), Capability);
	if (Capability != CapabilityMaterialInstance)
	{
		OutError = FBlueprintHelperMaterialInstanceTaskPlanAdapterLocalUtils::MakeError(
			TEXT("unsupported_material_instance_capability"),
			TEXT("MaterialInstance adapter requires capability=material_instance."),
			TEXT("task_plan.steps[0].capability"));
		return false;
	}

	FString LegacyOperation;
	if (StepObject->TryGetStringField(TEXT("operation"), LegacyOperation))
	{
		OutError = FBlueprintHelperMaterialInstanceTaskPlanAdapterLocalUtils::MakeError(
			TEXT("unsupported_material_instance_operation_field"),
			TEXT("MaterialInstance IR TaskPlan steps use capability/write; adapter operation fields are runtime lowering details."),
			TEXT("task_plan.steps[0].operation"));
		return false;
	}

	FString AssetPath;
	if (!FBlueprintHelperMaterialInstanceTaskPlanAdapterLocalUtils::TryReadTargetAssetPath(
		StepObject,
		AssetPath,
		OutError))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* OpsArray = nullptr;
	if (!FBlueprintHelperMaterialInstanceTaskPlanAdapterLocalUtils::TryReadOps(
		StepObject,
		OpsArray,
		OutError))
	{
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> PayloadOps;
	for (const TSharedPtr<FJsonValue>& OpValue : *OpsArray)
	{
		PayloadOps.Add(OpValue);
	}

	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("asset_path"), AssetPath);
	Payload->SetArrayField(TEXT("ops"), MoveTemp(PayloadOps));
	Payload->SetBoolField(TEXT("dry_run"), bDryRun);

	OutPayload.Capability = CapabilityMaterialInstance;
	OutPayload.RuntimeOperation = RuntimeOperationMaterialInstance;
	OutPayload.AdapterOperation = AdapterOperationMaterialInstanceEdit;
	OutPayload.Payload = Payload;
	OutPayload.bAdapterDryRunSupported = true;
	return true;
}

bool FBlueprintHelperMaterialInstanceTaskPlanAdapter::TryLowerTaskPlanStep(
	const TSharedPtr<FJsonObject>& TaskPlan,
	const TSharedPtr<FJsonObject>& StepObject,
	bool bDryRun,
	FBlueprintHelperTaskRuntimeLoweredStep& OutLoweredStep,
	FBlueprintHelperToolError& OutError)
{
	(void)TaskPlan;
	OutLoweredStep = FBlueprintHelperTaskRuntimeLoweredStep();

	FBlueprintHelperMaterialInstanceTaskPlanPayload BuiltPayload;
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
