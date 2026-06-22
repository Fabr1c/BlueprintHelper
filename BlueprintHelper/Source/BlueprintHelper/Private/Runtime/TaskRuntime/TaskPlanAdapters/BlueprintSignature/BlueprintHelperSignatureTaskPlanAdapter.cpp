// BlueprintHelper TaskPlan adapter - Blueprint Function/Event signature capability.

#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintSignature/BlueprintHelperSignatureTaskPlanAdapter.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"

class FBlueprintHelperSignatureTaskPlanAdapterLocalUtils
{
public:
	static FString SignatureStepFieldPath(const TCHAR* Field)
	{
		return FString::Printf(TEXT("task_plan.steps[0].%s"), Field);
	}

	static FString SignatureOpFieldPath(const TCHAR* Field)
	{
		return FString::Printf(TEXT("task_plan.steps[0].write.ops[0].%s"), Field);
	}

	static FBlueprintHelperToolError MakeSignatureAdapterError(
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

	static bool SignatureTryReadTargetAssetPath(
		const TSharedPtr<FJsonObject>& StepObject,
		FString& OutAssetPath,
		FBlueprintHelperToolError& OutError)
	{
		const TSharedPtr<FJsonObject>* TargetObjectPtr = nullptr;
		if (!StepObject.IsValid() ||
			!StepObject->TryGetObjectField(TEXT("target"), TargetObjectPtr) ||
			!TargetObjectPtr || !TargetObjectPtr->IsValid())
		{
			OutError = MakeSignatureAdapterError(
				TEXT("invalid_signature_target"),
				TEXT("blueprint_signature TaskPlan step requires target object."),
				SignatureStepFieldPath(TEXT("target")));
			return false;
		}

		if (!(*TargetObjectPtr)->TryGetStringField(TEXT("asset_path"), OutAssetPath) || OutAssetPath.IsEmpty())
		{
			OutError = MakeSignatureAdapterError(
				TEXT("invalid_signature_target"),
				TEXT("blueprint_signature TaskPlan step target requires asset_path."),
				SignatureStepFieldPath(TEXT("target.asset_path")));
			return false;
		}
		return true;
	}

	static bool SignatureTryReadSingleOp(
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
			OutError = MakeSignatureAdapterError(
				TEXT("invalid_signature_write"),
				TEXT("blueprint_signature TaskPlan step requires write object."),
				SignatureStepFieldPath(TEXT("write")));
			return false;
		}

		if (!(*WriteObjectPtr)->TryGetStringField(TEXT("strategy"), OutStrategy) ||
			(OutStrategy != FBlueprintHelperSignatureTaskPlanAdapter::StrategyFunctionSignature &&
				OutStrategy != FBlueprintHelperSignatureTaskPlanAdapter::StrategyCustomEventSignature &&
				OutStrategy != FBlueprintHelperSignatureTaskPlanAdapter::StrategyMacroSignature &&
				OutStrategy != FBlueprintHelperSignatureTaskPlanAdapter::StrategyEventDispatcherSignature &&
				OutStrategy != FBlueprintHelperSignatureTaskPlanAdapter::StrategyOverrideEventSignature))
		{
			OutError = MakeSignatureAdapterError(
				TEXT("unsupported_signature_strategy"),
				TEXT("blueprint_signature TaskPlan step supports function_signature, custom_event_signature, macro_signature, event_dispatcher_signature, and override_event_signature strategies."),
				SignatureStepFieldPath(TEXT("write.strategy")));
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* OpsArray = nullptr;
		if (!(*WriteObjectPtr)->TryGetArrayField(TEXT("ops"), OpsArray) || !OpsArray)
		{
			OutError = MakeSignatureAdapterError(
				TEXT("invalid_signature_ops"),
				TEXT("blueprint_signature TaskPlan step requires write.ops array."),
				SignatureStepFieldPath(TEXT("write.ops")));
			return false;
		}

		if (OpsArray->Num() != 1)
		{
			OutError = MakeSignatureAdapterError(
				TEXT("invalid_signature_ops"),
				TEXT("blueprint_signature TaskPlan step currently supports exactly one op."),
				SignatureStepFieldPath(TEXT("write.ops")));
			return false;
		}

		OutOpObject = (*OpsArray)[0].IsValid() ? (*OpsArray)[0]->AsObject() : nullptr;
		if (!OutOpObject.IsValid())
		{
			OutError = MakeSignatureAdapterError(
				TEXT("invalid_signature_op"),
				TEXT("blueprint_signature op must be an object."),
				TEXT("task_plan.steps[0].write.ops[0]"));
			return false;
		}
		return true;
	}

};

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
		OutError = FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::MakeSignatureAdapterError(
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
		OutError = FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::MakeSignatureAdapterError(
			TEXT("unsupported_taskplan_capability"),
			TEXT("Signature adapter supports blueprint_signature capability only."),
			FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::SignatureStepFieldPath(TEXT("capability")));
		return false;
	}

	FString OperationField;
	if (StepObject->TryGetStringField(TEXT("operation"), OperationField))
	{
		OutError = FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::MakeSignatureAdapterError(
			TEXT("unsupported_signature_operation_field"),
			TEXT("blueprint_signature IR TaskPlan steps use capability/write; adapter operation fields are runtime lowering details."),
			FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::SignatureStepFieldPath(TEXT("operation")));
		return false;
	}

	FString AssetPath;
	if (!FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::SignatureTryReadTargetAssetPath(StepObject, AssetPath, OutError))
	{
		return false;
	}

	FString Strategy;
	TSharedPtr<FJsonObject> OpObject;
	if (!FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::SignatureTryReadSingleOp(StepObject, Strategy, OpObject, OutError))
	{
		return false;
	}

	FString OpName;
	if (!OpObject->TryGetStringField(TEXT("op"), OpName) || OpName.IsEmpty())
	{
		OutError = FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::MakeSignatureAdapterError(
			TEXT("invalid_signature_op"),
			TEXT("blueprint_signature op requires op name."),
			FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::SignatureOpFieldPath(TEXT("op")));
		return false;
	}

	const bool bEnsureFunction = OpName == AdapterOperationEnsureFunction;
	const bool bEnsureCustomEvent = OpName == AdapterOperationEnsureCustomEvent;
	const bool bEnsureMacro = OpName == AdapterOperationEnsureMacro;
	const bool bRemoveSignature = OpName == AdapterOperationRemoveSignature;
	const bool bEnsureEventDispatcher = OpName == AdapterOperationEnsureEventDispatcher;
	const bool bEnsureOverrideEvent = OpName == AdapterOperationEnsureOverrideEvent;
	if (!bEnsureFunction && !bEnsureCustomEvent && !bEnsureMacro && !bRemoveSignature && !bEnsureEventDispatcher && !bEnsureOverrideEvent)
	{
		OutError = FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::MakeSignatureAdapterError(
			TEXT("unsupported_signature_op"),
			TEXT("blueprint_signature currently supports ensure_function, ensure_custom_event, ensure_macro, ensure_event_dispatcher, ensure_override_event, and remove_signature."),
			FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::SignatureOpFieldPath(TEXT("op")));
		return false;
	}

	if (bEnsureFunction && Strategy != StrategyFunctionSignature)
	{
		OutError = FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::MakeSignatureAdapterError(
			TEXT("unsupported_signature_strategy"),
			TEXT("ensure_function requires function_signature strategy."),
			FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::SignatureStepFieldPath(TEXT("write.strategy")));
		return false;
	}

	if (bEnsureCustomEvent && Strategy != StrategyCustomEventSignature)
	{
		OutError = FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::MakeSignatureAdapterError(
			TEXT("unsupported_signature_strategy"),
			TEXT("ensure_custom_event requires custom_event_signature strategy."),
			FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::SignatureStepFieldPath(TEXT("write.strategy")));
		return false;
	}

	if (bEnsureMacro && Strategy != StrategyMacroSignature)
	{
		OutError = FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::MakeSignatureAdapterError(
			TEXT("unsupported_signature_strategy"),
			TEXT("ensure_macro requires macro_signature strategy."),
			FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::SignatureStepFieldPath(TEXT("write.strategy")));
		return false;
	}

	if (bRemoveSignature &&
		Strategy != StrategyFunctionSignature &&
		Strategy != StrategyCustomEventSignature &&
		Strategy != StrategyEventDispatcherSignature &&
		Strategy != StrategyOverrideEventSignature)
	{
		OutError = FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::MakeSignatureAdapterError(
			TEXT("unsupported_signature_strategy"),
			TEXT("remove_signature requires a signature strategy matching signature_kind."),
			FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::SignatureStepFieldPath(TEXT("write.strategy")));
		return false;
	}

	if (bEnsureEventDispatcher && Strategy != StrategyEventDispatcherSignature)
	{
		OutError = FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::MakeSignatureAdapterError(
			TEXT("unsupported_signature_strategy"),
			TEXT("ensure_event_dispatcher requires event_dispatcher_signature strategy."),
			FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::SignatureStepFieldPath(TEXT("write.strategy")));
		return false;
	}

	if (bEnsureOverrideEvent && Strategy != StrategyOverrideEventSignature)
	{
		OutError = FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::MakeSignatureAdapterError(
			TEXT("unsupported_signature_strategy"),
			TEXT("ensure_override_event requires override_event_signature strategy."),
			FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::SignatureStepFieldPath(TEXT("write.strategy")));
		return false;
	}

	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("asset_path"), AssetPath);
	Payload->SetBoolField(TEXT("dry_run"), bDryRun);

	if (bEnsureFunction)
	{
		FString FunctionName;
		if (!OpObject->TryGetStringField(TEXT("function_name"), FunctionName) || FunctionName.IsEmpty())
		{
			OutError = FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::MakeSignatureAdapterError(
				TEXT("invalid_signature_op"),
				TEXT("ensure_function requires function_name."),
				FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::SignatureOpFieldPath(TEXT("function_name")));
			return false;
		}
		Payload->SetStringField(TEXT("function_name"), FunctionName);

		FString InterfacePath;
		if (OpObject->TryGetStringField(TEXT("interface_path"), InterfacePath) && !InterfacePath.IsEmpty())
		{
			Payload->SetStringField(TEXT("interface_path"), InterfacePath);
		}

		FString InterfaceEntryKind;
		if (OpObject->TryGetStringField(TEXT("interface_entry_kind"), InterfaceEntryKind) && !InterfaceEntryKind.IsEmpty())
		{
			Payload->SetStringField(TEXT("interface_entry_kind"), InterfaceEntryKind);
		}

		bool bIsPure = false;
		if (OpObject->TryGetBoolField(TEXT("is_pure"), bIsPure))
		{
			Payload->SetBoolField(TEXT("is_pure"), bIsPure);
		}

		FString SignatureMismatchPolicy;
		if (OpObject->TryGetStringField(TEXT("signature_mismatch_policy"), SignatureMismatchPolicy) && !SignatureMismatchPolicy.IsEmpty())
		{
			Payload->SetStringField(TEXT("signature_mismatch_policy"), SignatureMismatchPolicy);
		}

		const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr;
		if (OpObject->TryGetArrayField(TEXT("inputs"), Inputs) && Inputs)
		{
			Payload->SetArrayField(TEXT("inputs"), *Inputs);
		}

		const TArray<TSharedPtr<FJsonValue>>* Outputs = nullptr;
		if (OpObject->TryGetArrayField(TEXT("outputs"), Outputs) && Outputs)
		{
			Payload->SetArrayField(TEXT("outputs"), *Outputs);
		}
	}
	else if (bEnsureCustomEvent)
	{
		FString EventName;
		if (!OpObject->TryGetStringField(TEXT("event_name"), EventName) || EventName.IsEmpty())
		{
			OutError = FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::MakeSignatureAdapterError(
				TEXT("invalid_signature_op"),
				TEXT("ensure_custom_event requires event_name."),
				FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::SignatureOpFieldPath(TEXT("event_name")));
			return false;
		}

		FString GraphName;
		if (!OpObject->TryGetStringField(TEXT("graph_name"), GraphName) || GraphName.IsEmpty())
		{
			OutError = FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::MakeSignatureAdapterError(
				TEXT("invalid_signature_op"),
				TEXT("ensure_custom_event requires graph_name."),
				FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::SignatureOpFieldPath(TEXT("graph_name")));
			return false;
		}

		Payload->SetStringField(TEXT("event_name"), EventName);
		Payload->SetStringField(TEXT("graph_name"), GraphName);

		FString InterfacePath;
		if (OpObject->TryGetStringField(TEXT("interface_path"), InterfacePath) && !InterfacePath.IsEmpty())
		{
			Payload->SetStringField(TEXT("interface_path"), InterfacePath);
		}

		FString InterfaceEntryKind;
		if (OpObject->TryGetStringField(TEXT("interface_entry_kind"), InterfaceEntryKind) && !InterfaceEntryKind.IsEmpty())
		{
			Payload->SetStringField(TEXT("interface_entry_kind"), InterfaceEntryKind);
		}

		const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr;
		if (OpObject->TryGetArrayField(TEXT("inputs"), Inputs) && Inputs)
		{
			Payload->SetArrayField(TEXT("inputs"), *Inputs);
		}
	}
	else if (bEnsureMacro)
	{
		FString MacroName;
		if (!OpObject->TryGetStringField(TEXT("macro_name"), MacroName) || MacroName.IsEmpty())
		{
			OutError = FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::MakeSignatureAdapterError(
				TEXT("invalid_signature_op"),
				TEXT("ensure_macro requires macro_name."),
				FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::SignatureOpFieldPath(TEXT("macro_name")));
			return false;
		}

		Payload->SetStringField(TEXT("macro_name"), MacroName);

		const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr;
		if (OpObject->TryGetArrayField(TEXT("inputs"), Inputs) && Inputs)
		{
			Payload->SetArrayField(TEXT("inputs"), *Inputs);
		}

		const TArray<TSharedPtr<FJsonValue>>* Outputs = nullptr;
		if (OpObject->TryGetArrayField(TEXT("outputs"), Outputs) && Outputs)
		{
			Payload->SetArrayField(TEXT("outputs"), *Outputs);
		}
	}
	else if (bEnsureEventDispatcher)
	{
		FString DispatcherName;
		if (!OpObject->TryGetStringField(TEXT("dispatcher_name"), DispatcherName) || DispatcherName.IsEmpty())
		{
			OutError = FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::MakeSignatureAdapterError(
				TEXT("invalid_signature_op"),
				TEXT("ensure_event_dispatcher requires dispatcher_name."),
				FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::SignatureOpFieldPath(TEXT("dispatcher_name")));
			return false;
		}

		Payload->SetStringField(TEXT("dispatcher_name"), DispatcherName);

		FString SignatureMismatchPolicy;
		if (OpObject->TryGetStringField(TEXT("signature_mismatch_policy"), SignatureMismatchPolicy) && !SignatureMismatchPolicy.IsEmpty())
		{
			Payload->SetStringField(TEXT("signature_mismatch_policy"), SignatureMismatchPolicy);
		}

		const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr;
		if (OpObject->TryGetArrayField(TEXT("inputs"), Inputs) && Inputs)
		{
			Payload->SetArrayField(TEXT("inputs"), *Inputs);
		}
	}
	else if (bEnsureOverrideEvent)
	{
		FString EventName;
		if (!OpObject->TryGetStringField(TEXT("event_name"), EventName) || EventName.IsEmpty())
		{
			OutError = FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::MakeSignatureAdapterError(
				TEXT("invalid_signature_op"),
				TEXT("ensure_override_event requires event_name."),
				FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::SignatureOpFieldPath(TEXT("event_name")));
			return false;
		}

		Payload->SetStringField(TEXT("event_name"), EventName);

		FString GraphName;
		if (OpObject->TryGetStringField(TEXT("graph_name"), GraphName) && !GraphName.IsEmpty())
		{
			Payload->SetStringField(TEXT("graph_name"), GraphName);
		}

		FString EventKind;
		if (OpObject->TryGetStringField(TEXT("event_kind"), EventKind) && !EventKind.IsEmpty())
		{
			Payload->SetStringField(TEXT("event_kind"), EventKind);
		}

		FString ExecutePolicy;
		if (OpObject->TryGetStringField(TEXT("execute_policy"), ExecutePolicy) && !ExecutePolicy.IsEmpty())
		{
			Payload->SetStringField(TEXT("execute_policy"), ExecutePolicy);
		}

		const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr;
		if (OpObject->TryGetArrayField(TEXT("inputs"), Inputs) && Inputs)
		{
			Payload->SetArrayField(TEXT("inputs"), *Inputs);
		}
	}
	else
	{
		FString SignatureKind;
		if (!OpObject->TryGetStringField(TEXT("signature_kind"), SignatureKind) || SignatureKind.IsEmpty())
		{
			OutError = FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::MakeSignatureAdapterError(
				TEXT("signature_remove_kind_required"),
				TEXT("remove_signature requires signature_kind."),
				FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::SignatureOpFieldPath(TEXT("signature_kind")));
			return false;
		}

		if (SignatureKind != TEXT("function") &&
			SignatureKind != TEXT("interface_function") &&
			SignatureKind != TEXT("custom_event") &&
			SignatureKind != TEXT("interface_event") &&
			SignatureKind != TEXT("event_dispatcher") &&
			SignatureKind != TEXT("override_event") &&
			SignatureKind != TEXT("native_event"))
		{
			OutError = FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::MakeSignatureAdapterError(
				TEXT("unsupported_signature_remove_kind"),
				TEXT("remove_signature supports function, interface_function, custom_event, interface_event, event_dispatcher, override_event, or native_event."),
				FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::SignatureOpFieldPath(TEXT("signature_kind")));
			return false;
		}

		if ((SignatureKind == TEXT("function") || SignatureKind == TEXT("interface_function")) && Strategy != StrategyFunctionSignature)
		{
			OutError = FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::MakeSignatureAdapterError(
				TEXT("unsupported_signature_strategy"),
				TEXT("remove_signature kind=function/interface_function requires function_signature strategy."),
				FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::SignatureStepFieldPath(TEXT("write.strategy")));
			return false;
		}

		if ((SignatureKind == TEXT("custom_event") || SignatureKind == TEXT("interface_event")) && Strategy != StrategyCustomEventSignature)
		{
			OutError = FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::MakeSignatureAdapterError(
				TEXT("unsupported_signature_strategy"),
				TEXT("remove_signature kind=custom_event/interface_event requires custom_event_signature strategy."),
				FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::SignatureStepFieldPath(TEXT("write.strategy")));
			return false;
		}

		if (SignatureKind == TEXT("event_dispatcher") && Strategy != StrategyEventDispatcherSignature)
		{
			OutError = FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::MakeSignatureAdapterError(
				TEXT("unsupported_signature_strategy"),
				TEXT("remove_signature kind=event_dispatcher requires event_dispatcher_signature strategy."),
				FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::SignatureStepFieldPath(TEXT("write.strategy")));
			return false;
		}

		if ((SignatureKind == TEXT("override_event") || SignatureKind == TEXT("native_event")) && Strategy != StrategyOverrideEventSignature)
		{
			OutError = FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::MakeSignatureAdapterError(
				TEXT("unsupported_signature_strategy"),
				TEXT("remove_signature kind=override_event/native_event requires override_event_signature strategy."),
				FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::SignatureStepFieldPath(TEXT("write.strategy")));
			return false;
		}

		FString SignatureName;
		if (!OpObject->TryGetStringField(TEXT("signature_name"), SignatureName) || SignatureName.IsEmpty())
		{
			OutError = FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::MakeSignatureAdapterError(
				TEXT("signature_remove_name_required"),
				TEXT("remove_signature requires signature_name."),
				FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::SignatureOpFieldPath(TEXT("signature_name")));
			return false;
		}

		Payload->SetStringField(TEXT("signature_kind"), SignatureKind);
		Payload->SetStringField(TEXT("signature_name"), SignatureName);

		FString ExecutePolicy;
		if (OpObject->TryGetStringField(TEXT("execute_policy"), ExecutePolicy) && !ExecutePolicy.IsEmpty())
		{
			Payload->SetStringField(TEXT("execute_policy"), ExecutePolicy);
		}

		bool bRequireReferenceContext = false;
		if (OpObject->TryGetBoolField(TEXT("require_reference_context"), bRequireReferenceContext))
		{
			if (!bRequireReferenceContext)
			{
				OutError = FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::MakeSignatureAdapterError(
					TEXT("invalid_signature_remove_policy"),
					TEXT("remove_signature require_reference_context must be true in this slice."),
					FBlueprintHelperSignatureTaskPlanAdapterLocalUtils::SignatureOpFieldPath(TEXT("require_reference_context")));
				return false;
			}
			Payload->SetBoolField(TEXT("require_reference_context"), bRequireReferenceContext);
		}

		FString GraphName;
		if (OpObject->TryGetStringField(TEXT("graph_name"), GraphName) && !GraphName.IsEmpty())
		{
			Payload->SetStringField(TEXT("graph_name"), GraphName);
		}
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
