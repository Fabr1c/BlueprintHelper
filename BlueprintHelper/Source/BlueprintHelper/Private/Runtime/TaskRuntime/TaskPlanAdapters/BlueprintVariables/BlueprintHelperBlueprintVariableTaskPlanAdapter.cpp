#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintVariables/BlueprintHelperBlueprintVariableTaskPlanAdapter.h"

#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintVariables/Utils/BlueprintHelperBlueprintVariableTaskPlanAdapterUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

const TCHAR* FBlueprintHelperBlueprintVariableTaskPlanAdapter::CapabilityBlueprintVariable = TEXT("blueprint_variable");
const TCHAR* FBlueprintHelperBlueprintVariableTaskPlanAdapter::RuntimeOperationBlueprintVariable = TEXT("blueprint_variable");

const TCHAR* FBlueprintHelperBlueprintVariableTaskPlanAdapter::StrategyMemberVariables = TEXT("member_variables");
const TCHAR* FBlueprintHelperBlueprintVariableTaskPlanAdapter::StrategyMemberDefaults = TEXT("member_defaults");
const TCHAR* FBlueprintHelperBlueprintVariableTaskPlanAdapter::StrategyLocalVariables = TEXT("local_variables");

const TCHAR* FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationAddMemberVariables = TEXT("add_blueprint_member_variables");
const TCHAR* FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationVariableBatch = TEXT("blueprint_variable_batch");

const TCHAR* FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpEnsureMemberVariable = TEXT("ensure_member_variable");
const TCHAR* FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpSetMemberVariableProperties = TEXT("set_member_variable_properties");
const TCHAR* FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpRemoveMemberVariable = TEXT("remove_member_variable");
const TCHAR* FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpSetMemberDefault = TEXT("set_member_default");
const TCHAR* FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpEnsureLocalVariable = TEXT("ensure_local_variable");
const TCHAR* FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpSetLocalVariableProperties = TEXT("set_local_variable_properties");
const TCHAR* FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpRemoveLocalVariable = TEXT("remove_local_variable");

bool FBlueprintHelperBlueprintVariableTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
	const TSharedPtr<FJsonObject>& StepObject,
	bool bDryRun,
	FBlueprintHelperBlueprintVariableTaskPlanPayload& OutPayload,
	FBlueprintHelperToolError& OutError)
{
	if (!StepObject.IsValid())
	{
		OutError = FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::MakeVariableTaskPlanError(
			TEXT("invalid_taskplan_step"),
			TEXT("TaskPlan step must be an object."),
			FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::BuildStepFieldPath(TEXT("")));
		return false;
	}

	FString AdapterOperation;
	if (StepObject->TryGetStringField(TEXT("operation"), AdapterOperation))
	{
		OutError = FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::MakeVariableTaskPlanError(
			TEXT("unsupported_blueprint_variable_operation_field"),
			TEXT("Blueprint variable IR TaskPlan steps use capability/write; adapter operation fields are runtime lowering details."),
			FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::BuildStepFieldPath(TEXT("operation")));
		return false;
	}

	FString Capability;
	if (!StepObject->TryGetStringField(TEXT("capability"), Capability) || Capability != CapabilityBlueprintVariable)
	{
		OutError = FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::MakeVariableTaskPlanError(
			TEXT("unsupported_blueprint_variable_capability"),
			TEXT("Blueprint variable adapter only supports capability blueprint_variable."),
			FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::BuildStepFieldPath(TEXT("capability")));
		return false;
	}

	const TSharedPtr<FJsonObject>* TargetObjectPtr = nullptr;
	if (!StepObject->TryGetObjectField(TEXT("target"), TargetObjectPtr) ||
		!TargetObjectPtr || !TargetObjectPtr->IsValid())
	{
		OutError = FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::MakeVariableTaskPlanError(
			TEXT("invalid_taskplan_step_target"),
			TEXT("Blueprint variable TaskPlan step target object is required."),
			FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::BuildStepFieldPath(TEXT("target")));
		return false;
	}

	FString AssetPath;
	if (!(*TargetObjectPtr)->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		OutError = FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::MakeVariableTaskPlanError(
			TEXT("invalid_taskplan_step_target"),
			TEXT("Blueprint variable TaskPlan step target requires asset_path."),
			FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::BuildStepFieldPath(TEXT("target.asset_path")));
		return false;
	}

	const TSharedPtr<FJsonObject>* WriteObjectPtr = nullptr;
	if (!StepObject->TryGetObjectField(TEXT("write"), WriteObjectPtr) ||
		!WriteObjectPtr || !WriteObjectPtr->IsValid())
	{
		OutError = FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::MakeVariableTaskPlanError(
			TEXT("invalid_variable_write"),
			TEXT("blueprint_variable TaskPlan step requires write object."),
			FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::BuildStepFieldPath(TEXT("write")));
		return false;
	}

	FString Strategy;
	if (!(*WriteObjectPtr)->TryGetStringField(TEXT("strategy"), Strategy) ||
		(Strategy != StrategyMemberVariables &&
			Strategy != StrategyMemberDefaults &&
			Strategy != StrategyLocalVariables))
	{
		OutError = FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::MakeVariableTaskPlanError(
			TEXT("unsupported_variable_strategy"),
			TEXT("Blueprint variable TaskPlan step requires a supported write.strategy."),
			FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::BuildStepFieldPath(TEXT("write.strategy")));
		return false;
	}

	FString FunctionName;
	if (Strategy == StrategyLocalVariables &&
		(!(*TargetObjectPtr)->TryGetStringField(TEXT("function_name"), FunctionName) || FunctionName.IsEmpty()))
	{
		OutError = FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::MakeVariableTaskPlanError(
			TEXT("invalid_taskplan_step_target"),
			TEXT("local_variables TaskPlan step target requires function_name."),
			FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::BuildStepFieldPath(TEXT("target.function_name")));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* OpsArray = nullptr;
	if (!(*WriteObjectPtr)->TryGetArrayField(TEXT("ops"), OpsArray) || !OpsArray || OpsArray->Num() == 0)
	{
		OutError = FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::MakeVariableTaskPlanError(
			TEXT("invalid_variable_ops"),
			TEXT("blueprint_variable TaskPlan step requires write.ops array."),
			FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::BuildStepFieldPath(TEXT("write.ops")));
		return false;
	}

	bool bAllEnsureMemberVariables = Strategy == StrategyMemberVariables;
	TArray<TSharedPtr<FJsonValue>> BatchOps;
	TArray<TSharedPtr<FJsonValue>> MemberVariables;

	for (int32 OpIndex = 0; OpIndex < OpsArray->Num(); ++OpIndex)
	{
		const TSharedPtr<FJsonObject> OpObject =
			(*OpsArray)[OpIndex].IsValid()
				? (*OpsArray)[OpIndex]->AsObject()
				: nullptr;
		if (!OpObject.IsValid())
		{
			OutError = FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::MakeVariableTaskPlanError(
				TEXT("invalid_variable_op"),
				TEXT("Blueprint variable op must be an object."),
				FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::BuildOpFieldPath(OpIndex, TEXT("")));
			return false;
		}

		FString OpName;
		if (!OpObject->TryGetStringField(TEXT("op"), OpName) || OpName.IsEmpty())
		{
			OutError = FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::MakeVariableTaskPlanError(
				TEXT("invalid_variable_op"),
				TEXT("Blueprint variable op requires op."),
				FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::BuildOpFieldPath(OpIndex, TEXT("op")));
			return false;
		}

		bool bOpValid = false;
		if (Strategy == StrategyMemberVariables)
		{
			bOpValid = FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::TryValidateMemberVariableOp(OpName, OpObject, OpIndex, OutError);
			bAllEnsureMemberVariables = bAllEnsureMemberVariables && OpName == OpEnsureMemberVariable;
		}
		else if (Strategy == StrategyMemberDefaults)
		{
			bOpValid = FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::TryValidateMemberDefaultOp(OpName, OpObject, OpIndex, OutError);
			bAllEnsureMemberVariables = false;
		}
		else
		{
			bOpValid = FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::TryValidateLocalVariableOp(OpName, OpObject, FunctionName, OpIndex, OutError);
			bAllEnsureMemberVariables = false;
		}

		if (!bOpValid)
		{
			return false;
		}

		if (bAllEnsureMemberVariables)
		{
			TSharedRef<FJsonObject> Variable = MakeShared<FJsonObject>();
			FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::CopyObjectFieldsExceptOp(OpObject, Variable);
			MemberVariables.Add(MakeShared<FJsonValueObject>(Variable));
		}

		BatchOps.Add(MakeShared<FJsonValueObject>(
			FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::CopyOpForBatch(OpObject, FunctionName)));
	}

	OutPayload.Capability = CapabilityBlueprintVariable;
	OutPayload.RuntimeOperation = RuntimeOperationBlueprintVariable;
	OutPayload.bAdapterDryRunSupported = Strategy == StrategyLocalVariables;

	if (bAllEnsureMemberVariables)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), AssetPath);
		Payload->SetArrayField(TEXT("variables"), MemberVariables);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);

		OutPayload.AdapterOperation = AdapterOperationAddMemberVariables;
		OutPayload.Payload = Payload;
		return true;
	}

	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("asset_path"), AssetPath);
	Payload->SetStringField(TEXT("strategy"), Strategy);
	Payload->SetBoolField(TEXT("dry_run"), bDryRun);
	Payload->SetArrayField(TEXT("ops"), BatchOps);
	if (!FunctionName.IsEmpty())
	{
		Payload->SetStringField(TEXT("function_name"), FunctionName);
	}

	const TSharedPtr<FJsonObject>* ConstraintsObjectPtr = nullptr;
	if (StepObject->TryGetObjectField(TEXT("constraints"), ConstraintsObjectPtr) &&
		ConstraintsObjectPtr && ConstraintsObjectPtr->IsValid())
	{
		Payload->SetObjectField(TEXT("constraints"), *ConstraintsObjectPtr);
	}

	OutPayload.AdapterOperation = AdapterOperationVariableBatch;
	OutPayload.Payload = Payload;
	return true;
}
