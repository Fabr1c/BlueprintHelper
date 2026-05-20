#include "Runtime/TaskRuntime/Utils/BlueprintHelperBlueprintVariableTaskPlanAdapterUtils.h"

#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintVariables/BlueprintHelperBlueprintVariableTaskPlanAdapter.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

FBlueprintHelperToolError FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::MakeVariableTaskPlanError(
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

FString FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::BuildStepFieldPath(const FString& Suffix)
{
	return Suffix.IsEmpty()
		? FString(TEXT("task_plan.steps[0]"))
		: FString::Printf(TEXT("task_plan.steps[0].%s"), *Suffix);
}

FString FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::BuildOpFieldPath(
	int32 OpIndex,
	const FString& Suffix)
{
	return Suffix.IsEmpty()
		? FString::Printf(TEXT("task_plan.steps[0].write.ops[%d]"), OpIndex)
		: FString::Printf(TEXT("task_plan.steps[0].write.ops[%d].%s"), OpIndex, *Suffix);
}

bool FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::TryReadRequiredString(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	const FString& FieldPath,
	FString& OutValue,
	FBlueprintHelperToolError& OutError)
{
	OutValue.Empty();
	if (!Object.IsValid() || !Object->TryGetStringField(FieldName, OutValue) || OutValue.IsEmpty())
	{
		OutError = MakeVariableTaskPlanError(
			TEXT("invalid_variable_op"),
			FString::Printf(TEXT("Blueprint variable op requires %s."), FieldName),
			FieldPath);
		return false;
	}
	return true;
}

bool FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::TryReadRequiredObject(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	const FString& FieldPath,
	FBlueprintHelperToolError& OutError)
{
	const TSharedPtr<FJsonObject>* ObjectPtr = nullptr;
	if (!Object.IsValid() ||
		!Object->TryGetObjectField(FieldName, ObjectPtr) ||
		!ObjectPtr || !ObjectPtr->IsValid())
	{
		OutError = MakeVariableTaskPlanError(
			TEXT("invalid_variable_op"),
			FString::Printf(TEXT("Blueprint variable op requires %s object."), FieldName),
			FieldPath);
		return false;
	}
	return true;
}

bool FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::TryReadRequiredArray(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	const FString& FieldPath,
	FBlueprintHelperToolError& OutError)
{
	const TArray<TSharedPtr<FJsonValue>>* ArrayPtr = nullptr;
	if (!Object.IsValid() ||
		!Object->TryGetArrayField(FieldName, ArrayPtr) ||
		!ArrayPtr || ArrayPtr->Num() == 0)
	{
		OutError = MakeVariableTaskPlanError(
			TEXT("invalid_variable_op"),
			FString::Printf(TEXT("Blueprint variable op requires non-empty %s array."), FieldName),
			FieldPath);
		return false;
	}
	return true;
}

bool FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::TryRequireValueField(
	const TSharedPtr<FJsonObject>& Object,
	const FString& FieldPath,
	FBlueprintHelperToolError& OutError)
{
	if (!Object.IsValid() || !Object->Values.Contains(TEXT("value")))
	{
		OutError = MakeVariableTaskPlanError(
			TEXT("invalid_variable_op"),
			TEXT("set_member_default requires value."),
			FieldPath);
		return false;
	}
	return true;
}

void FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::CopyObjectFieldsExceptOp(
	const TSharedPtr<FJsonObject>& Source,
	const TSharedRef<FJsonObject>& Destination)
{
	if (!Source.IsValid())
	{
		return;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Source->Values)
	{
		if (Field.Key != TEXT("op"))
		{
			Destination->SetField(Field.Key, Field.Value);
		}
	}
}

TSharedRef<FJsonObject> FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::CopyOpForBatch(
	const TSharedPtr<FJsonObject>& Source,
	const FString& LocalFunctionName)
{
	TSharedRef<FJsonObject> Destination = MakeShared<FJsonObject>();
	if (Source.IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Source->Values)
		{
			Destination->SetField(Field.Key, Field.Value);
		}
	}
	if (!LocalFunctionName.IsEmpty() && !Destination->HasField(TEXT("function_name")))
	{
		Destination->SetStringField(TEXT("function_name"), LocalFunctionName);
	}
	return Destination;
}

bool FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::TryValidateMemberVariableOp(
	const FString& OpName,
	const TSharedPtr<FJsonObject>& OpObject,
	int32 OpIndex,
	FBlueprintHelperToolError& OutError)
{
	FString Ignored;
	if (OpName == FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpEnsureMemberVariable)
	{
		return TryReadRequiredString(OpObject, TEXT("name"), BuildOpFieldPath(OpIndex, TEXT("name")), Ignored, OutError) &&
			TryReadRequiredObject(OpObject, TEXT("pin_type"), BuildOpFieldPath(OpIndex, TEXT("pin_type")), OutError);
	}
	if (OpName == FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpSetMemberVariableProperties)
	{
		return TryReadRequiredString(OpObject, TEXT("name"), BuildOpFieldPath(OpIndex, TEXT("name")), Ignored, OutError) &&
			TryReadRequiredArray(OpObject, TEXT("settings"), BuildOpFieldPath(OpIndex, TEXT("settings")), OutError);
	}
	if (OpName == FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpRemoveMemberVariable)
	{
		return TryReadRequiredString(OpObject, TEXT("name"), BuildOpFieldPath(OpIndex, TEXT("name")), Ignored, OutError);
	}

	OutError = MakeVariableTaskPlanError(
		TEXT("unsupported_variable_op"),
		FString::Printf(TEXT("Unsupported member variable op: %s."), *OpName),
		BuildOpFieldPath(OpIndex, TEXT("op")));
	return false;
}

bool FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::TryValidateMemberDefaultOp(
	const FString& OpName,
	const TSharedPtr<FJsonObject>& OpObject,
	int32 OpIndex,
	FBlueprintHelperToolError& OutError)
{
	if (OpName != FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpSetMemberDefault)
	{
		OutError = MakeVariableTaskPlanError(
			TEXT("unsupported_variable_op"),
			FString::Printf(TEXT("Unsupported member default op: %s."), *OpName),
			BuildOpFieldPath(OpIndex, TEXT("op")));
		return false;
	}

	FString Ignored;
	return TryReadRequiredString(OpObject, TEXT("name"), BuildOpFieldPath(OpIndex, TEXT("name")), Ignored, OutError) &&
		TryRequireValueField(OpObject, BuildOpFieldPath(OpIndex, TEXT("value")), OutError);
}

bool FBlueprintHelperBlueprintVariableTaskPlanAdapterUtils::TryValidateLocalVariableOp(
	const FString& OpName,
	const TSharedPtr<FJsonObject>& OpObject,
	const FString& FunctionName,
	int32 OpIndex,
	FBlueprintHelperToolError& OutError)
{
	FString OpFunctionName;
	if (OpObject.IsValid() &&
		OpObject->TryGetStringField(TEXT("function_name"), OpFunctionName) &&
		!OpFunctionName.IsEmpty() &&
		OpFunctionName != FunctionName)
	{
		OutError = MakeVariableTaskPlanError(
			TEXT("invalid_variable_op"),
			TEXT("Local variable op function_name must match target.function_name."),
			BuildOpFieldPath(OpIndex, TEXT("function_name")));
		return false;
	}

	FString Ignored;
	if (OpName == FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpEnsureLocalVariable)
	{
		return TryReadRequiredString(OpObject, TEXT("name"), BuildOpFieldPath(OpIndex, TEXT("name")), Ignored, OutError) &&
			TryReadRequiredObject(OpObject, TEXT("pin_type"), BuildOpFieldPath(OpIndex, TEXT("pin_type")), OutError);
	}
	if (OpName == FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpSetLocalVariableProperties)
	{
		return TryReadRequiredString(OpObject, TEXT("name"), BuildOpFieldPath(OpIndex, TEXT("name")), Ignored, OutError) &&
			TryReadRequiredArray(OpObject, TEXT("settings"), BuildOpFieldPath(OpIndex, TEXT("settings")), OutError);
	}
	if (OpName == FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpRemoveLocalVariable)
	{
		return TryReadRequiredString(OpObject, TEXT("name"), BuildOpFieldPath(OpIndex, TEXT("name")), Ignored, OutError);
	}

	OutError = MakeVariableTaskPlanError(
		TEXT("unsupported_variable_op"),
		FString::Printf(TEXT("Unsupported local variable op: %s."), *OpName),
		BuildOpFieldPath(OpIndex, TEXT("op")));
	return false;
}
