// BlueprintHelper TaskPlan adapter - Blueprint Function/Event signature capability.

#include "TaskRuntime/TaskPlanAdapters/BlueprintSignature/BlueprintHelperSignatureTaskPlanAdapter.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "TaskRuntime/BlueprintHelperTaskRuntimeService.h"

namespace
{
	FString StepFieldPath(const TCHAR* Field)
	{
		return FString::Printf(TEXT("task_plan.steps[0].%s"), Field);
	}

	FString OpFieldPath(const TCHAR* Field)
	{
		return FString::Printf(TEXT("task_plan.steps[0].write.ops[0].%s"), Field);
	}

	FBlueprintHelperToolError MakeAdapterError(
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

	bool TryReadTargetAssetPath(
		const TSharedPtr<FJsonObject>& StepObject,
		FString& OutAssetPath,
		FBlueprintHelperToolError& OutError)
	{
		const TSharedPtr<FJsonObject>* TargetObjectPtr = nullptr;
		if (!StepObject.IsValid() ||
			!StepObject->TryGetObjectField(TEXT("target"), TargetObjectPtr) ||
			!TargetObjectPtr || !TargetObjectPtr->IsValid())
		{
			OutError = MakeAdapterError(
				TEXT("invalid_signature_target"),
				TEXT("blueprint_signature TaskPlan step requires target object."),
				StepFieldPath(TEXT("target")));
			return false;
		}

		if (!(*TargetObjectPtr)->TryGetStringField(TEXT("asset_path"), OutAssetPath) || OutAssetPath.IsEmpty())
		{
			OutError = MakeAdapterError(
				TEXT("invalid_signature_target"),
				TEXT("blueprint_signature TaskPlan step target requires asset_path."),
				StepFieldPath(TEXT("target.asset_path")));
			return false;
		}
		return true;
	}

	bool TryReadSingleOp(
		const TSharedPtr<FJsonObject>& StepObject,
		FString& OutStrategy,
		TSharedPtr<FJsonObject>& OutOpObject,
		FBlueprintHelperToolError& OutError)
	{
		const TSharedPtr<FJsonObject>* WriteObjectPtr = nullptr;
		if (!StepObject.IsValid() ||
			!StepObject->TryGetObjectField(TEXT("write"), WriteObjectPtr) ||
			!WriteObjectPtr || !WriteObjectPtr->IsValid())
		{
			OutError = MakeAdapterError(
				TEXT("invalid_signature_write"),
				TEXT("blueprint_signature TaskPlan step requires write object."),
				StepFieldPath(TEXT("write")));
			return false;
		}

		if (!(*WriteObjectPtr)->TryGetStringField(TEXT("strategy"), OutStrategy) ||
			(OutStrategy != FBlueprintHelperSignatureTaskPlanAdapter::StrategyFunctionSignature &&
				OutStrategy != FBlueprintHelperSignatureTaskPlanAdapter::StrategyCustomEventSignature))
		{
			OutError = MakeAdapterError(
				TEXT("unsupported_signature_strategy"),
				TEXT("blueprint_signature TaskPlan step supports function_signature and custom_event_signature strategies."),
				StepFieldPath(TEXT("write.strategy")));
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* OpsArray = nullptr;
		if (!(*WriteObjectPtr)->TryGetArrayField(TEXT("ops"), OpsArray) || !OpsArray)
		{
			OutError = MakeAdapterError(
				TEXT("invalid_signature_ops"),
				TEXT("blueprint_signature TaskPlan step requires write.ops array."),
				StepFieldPath(TEXT("write.ops")));
			return false;
		}

		if (OpsArray->Num() != 1)
		{
			OutError = MakeAdapterError(
				TEXT("invalid_signature_ops"),
				TEXT("blueprint_signature TaskPlan step currently supports exactly one op."),
				StepFieldPath(TEXT("write.ops")));
			return false;
		}

		OutOpObject = (*OpsArray)[0].IsValid() ? (*OpsArray)[0]->AsObject() : nullptr;
		if (!OutOpObject.IsValid())
		{
			OutError = MakeAdapterError(
				TEXT("invalid_signature_op"),
				TEXT("blueprint_signature op must be an object."),
				TEXT("task_plan.steps[0].write.ops[0]"));
			return false;
		}
		return true;
	}
}

bool FBlueprintHelperSignatureTaskPlanAdapter::TryLowerTaskPlanStep(
	const TSharedPtr<FJsonObject>& TaskPlan,
	const TSharedPtr<FJsonObject>& StepObject,
	bool bDryRun,
	FBlueprintHelperTaskRuntimeLoweredStep& OutLoweredStep,
	FBlueprintHelperToolError& OutError)
{
	static_cast<void>(TaskPlan);
	OutLoweredStep = FBlueprintHelperTaskRuntimeLoweredStep();
	OutError = FBlueprintHelperToolError();

	if (!StepObject.IsValid())
	{
		OutError = MakeAdapterError(
			TEXT("invalid_taskplan_step"),
			TEXT("TaskPlan step must be an object."),
			TEXT("task_plan.steps[0]"));
		return false;
	}

	StepObject->TryGetStringField(TEXT("step_id"), OutLoweredStep.StepId);
	if (OutLoweredStep.StepId.IsEmpty())
	{
		OutLoweredStep.StepId = TEXT("step_001");
	}

	FString Capability;
	StepObject->TryGetStringField(TEXT("capability"), Capability);
	if (Capability != CapabilityName)
	{
		OutError = MakeAdapterError(
			TEXT("unsupported_taskplan_capability"),
			TEXT("Signature adapter supports blueprint_signature capability only."),
			StepFieldPath(TEXT("capability")));
		return false;
	}

	FString OperationField;
	if (StepObject->TryGetStringField(TEXT("operation"), OperationField))
	{
		OutError = MakeAdapterError(
			TEXT("unsupported_signature_operation_field"),
			TEXT("blueprint_signature IR TaskPlan steps use capability/write; adapter operation fields are runtime lowering details."),
			StepFieldPath(TEXT("operation")));
		return false;
	}

	FString AssetPath;
	if (!TryReadTargetAssetPath(StepObject, AssetPath, OutError))
	{
		return false;
	}

	FString Strategy;
	TSharedPtr<FJsonObject> OpObject;
	if (!TryReadSingleOp(StepObject, Strategy, OpObject, OutError))
	{
		return false;
	}

	FString OpName;
	if (!OpObject->TryGetStringField(TEXT("op"), OpName) || OpName.IsEmpty())
	{
		OutError = MakeAdapterError(
			TEXT("invalid_signature_op"),
			TEXT("blueprint_signature op requires op name."),
			OpFieldPath(TEXT("op")));
		return false;
	}

	if (OpName != AdapterOperationEnsureFunction && OpName != AdapterOperationEnsureCustomEvent)
	{
		OutError = MakeAdapterError(
			TEXT("unsupported_signature_op"),
			TEXT("blueprint_signature currently supports ensure_function and ensure_custom_event."),
			OpFieldPath(TEXT("op")));
		return false;
	}

	if (OpName == AdapterOperationEnsureFunction && Strategy != StrategyFunctionSignature)
	{
		OutError = MakeAdapterError(
			TEXT("unsupported_signature_strategy"),
			TEXT("ensure_function requires function_signature strategy."),
			StepFieldPath(TEXT("write.strategy")));
		return false;
	}

	if (OpName == AdapterOperationEnsureCustomEvent && Strategy != StrategyCustomEventSignature)
	{
		OutError = MakeAdapterError(
			TEXT("unsupported_signature_strategy"),
			TEXT("ensure_custom_event requires custom_event_signature strategy."),
			StepFieldPath(TEXT("write.strategy")));
		return false;
	}

	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("asset_path"), AssetPath);
	Payload->SetBoolField(TEXT("dry_run"), bDryRun);

	if (OpName == AdapterOperationEnsureFunction)
	{
		FString FunctionName;
		if (!OpObject->TryGetStringField(TEXT("function_name"), FunctionName) || FunctionName.IsEmpty())
		{
			OutError = MakeAdapterError(
				TEXT("invalid_signature_op"),
				TEXT("ensure_function requires function_name."),
				OpFieldPath(TEXT("function_name")));
			return false;
		}
		Payload->SetStringField(TEXT("function_name"), FunctionName);

		FString InterfacePath;
		if (OpObject->TryGetStringField(TEXT("interface_path"), InterfacePath) && !InterfacePath.IsEmpty())
		{
			Payload->SetStringField(TEXT("interface_path"), InterfacePath);
		}
	}
	else
	{
		FString EventName;
		if (!OpObject->TryGetStringField(TEXT("event_name"), EventName) || EventName.IsEmpty())
		{
			OutError = MakeAdapterError(
				TEXT("invalid_signature_op"),
				TEXT("ensure_custom_event requires event_name."),
				OpFieldPath(TEXT("event_name")));
			return false;
		}

		FString GraphName;
		if (!OpObject->TryGetStringField(TEXT("graph_name"), GraphName) || GraphName.IsEmpty())
		{
			OutError = MakeAdapterError(
				TEXT("invalid_signature_op"),
				TEXT("ensure_custom_event requires graph_name."),
				OpFieldPath(TEXT("graph_name")));
			return false;
		}

		Payload->SetStringField(TEXT("event_name"), EventName);
		Payload->SetStringField(TEXT("graph_name"), GraphName);
	}

	FString NameCollisionPolicy;
	if (OpObject->TryGetStringField(TEXT("name_collision_policy"), NameCollisionPolicy) && !NameCollisionPolicy.IsEmpty())
	{
		Payload->SetStringField(TEXT("name_collision_policy"), NameCollisionPolicy);
	}

	OutLoweredStep.Capability = CapabilityName;
	OutLoweredStep.RuntimeOperation = RuntimeOperationName;
	OutLoweredStep.AdapterOperation = OpName;
	OutLoweredStep.Payload = Payload;
	OutLoweredStep.bAdapterDryRunSupported = true;
	return true;
}
