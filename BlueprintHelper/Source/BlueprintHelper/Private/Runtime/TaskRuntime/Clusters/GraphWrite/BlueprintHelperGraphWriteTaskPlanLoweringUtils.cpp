#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskPlanLoweringUtils.h"

#include "Dom/JsonValue.h"

namespace BlueprintHelperGraphWriteLowering
{
	FBlueprintHelperToolError MakeToolError(
		const FString& Code,
		EBlueprintHelperToolStage Stage,
		const FString& Message,
		const FString& Field)
	{
		FBlueprintHelperToolError Error;
		Error.Code = Code;
		Error.Stage = Stage;
		Error.Message = Message;
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		Error.Field = Field;
		return Error;
	}

	FString BuildStepFieldPath(const FString& Suffix)
	{
		return Suffix.IsEmpty()
			? FString(TEXT("task_plan.steps[0]"))
			: FString::Printf(TEXT("task_plan.steps[0].%s"), *Suffix);
	}

	FString BuildOpFieldPath(int32 OpIndex, const FString& Suffix)
	{
		return Suffix.IsEmpty()
			? FString::Printf(TEXT("task_plan.steps[0].write.ops[%d]"), OpIndex)
			: FString::Printf(TEXT("task_plan.steps[0].write.ops[%d].%s"), OpIndex, *Suffix);
	}

	FString ToIdSegment(const FString& Value)
	{
		FString Result;
		Result.Reserve(Value.Len());
		for (const TCHAR Ch : Value)
		{
			Result.AppendChar(FChar::IsAlnum(Ch) || Ch == TCHAR('_') ? Ch : TCHAR('_'));
		}
		return Result.IsEmpty() ? FString(TEXT("entry")) : Result;
	}

	TSharedPtr<FJsonObject> AsJsonObjectIfObject(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid() || Value->Type != EJson::Object)
		{
			return nullptr;
		}
		return Value->AsObject();
	}

	TSharedRef<FJsonObject> CopyJsonObject(const TSharedPtr<FJsonObject>& Source)
	{
		TSharedRef<FJsonObject> Copy = MakeShared<FJsonObject>();
		CopyObjectFields(Source, Copy);
		return Copy;
	}

	void CopyObjectFields(
		const TSharedPtr<FJsonObject>& Source,
		const TSharedRef<FJsonObject>& Destination)
	{
		if (!Source.IsValid())
		{
			return;
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Source->Values)
		{
			Destination->SetField(Field.Key, Field.Value);
		}
	}

	bool TryReadStepTarget(
		const TSharedPtr<FJsonObject>& StepObject,
		TSharedPtr<FJsonObject>& OutTargetObject,
		FString& OutAssetPath,
		FString& OutGraphName,
		FBlueprintHelperToolError& OutError)
	{
		const TSharedPtr<FJsonObject>* TargetObjectPtr = nullptr;
		if (!StepObject.IsValid() ||
			!StepObject->TryGetObjectField(TEXT("target"), TargetObjectPtr) ||
			!TargetObjectPtr || !TargetObjectPtr->IsValid())
		{
			OutError = MakeToolError(
				TEXT("invalid_taskplan_step"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("TaskPlan step target object is required."),
				BuildStepFieldPath(TEXT("target")));
			return false;
		}

		(*TargetObjectPtr)->TryGetStringField(TEXT("asset_path"), OutAssetPath);
		(*TargetObjectPtr)->TryGetStringField(TEXT("graph"), OutGraphName);
		if (OutAssetPath.IsEmpty() || OutGraphName.IsEmpty())
		{
			OutError = MakeToolError(
				TEXT("invalid_taskplan_step_target"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("TaskPlan step target requires asset_path and graph."),
				BuildStepFieldPath(TEXT("target")));
			return false;
		}

		OutTargetObject = *TargetObjectPtr;
		return true;
	}

	bool TryReadWriteOps(
		const TSharedPtr<FJsonObject>& StepObject,
		const TArray<TSharedPtr<FJsonValue>>*& OutOpsArray,
		FBlueprintHelperToolError& OutError)
	{
		OutOpsArray = nullptr;
		const TSharedPtr<FJsonObject>* WriteObjectPtr = nullptr;
		if (!StepObject.IsValid() ||
			!StepObject->TryGetObjectField(TEXT("write"), WriteObjectPtr) ||
			!WriteObjectPtr || !WriteObjectPtr->IsValid())
		{
			OutError = MakeToolError(
				TEXT("invalid_taskplan_step_write"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("graph_write TaskPlan step requires write object."),
				BuildStepFieldPath(TEXT("write")));
			return false;
		}

		if (!(*WriteObjectPtr)->TryGetArrayField(TEXT("ops"), OutOpsArray) ||
			!OutOpsArray || OutOpsArray->Num() == 0)
		{
			OutError = MakeToolError(
				TEXT("invalid_graph_write_ops"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("graph_write TaskPlan step requires non-empty write.ops array."),
				BuildStepFieldPath(TEXT("write.ops")));
			return false;
		}
		return true;
	}

	bool TryReadRequiredObject(
		const TSharedPtr<FJsonObject>& Source,
		const FString& FieldName,
		const FString& FieldPath,
		TSharedPtr<FJsonObject>& OutObject,
		FBlueprintHelperToolError& OutError)
	{
		const TSharedPtr<FJsonObject>* ObjectPtr = nullptr;
		if (!Source.IsValid() ||
			!Source->TryGetObjectField(FieldName, ObjectPtr) ||
			!ObjectPtr || !ObjectPtr->IsValid())
		{
			OutError = MakeToolError(
				TEXT("invalid_graph_write_ir_op"),
				EBlueprintHelperToolStage::ParseInput,
				FString::Printf(TEXT("GraphWrite structural op requires %s object."), *FieldName),
				FieldPath);
			return false;
		}

		OutObject = *ObjectPtr;
		return true;
	}

	TSharedRef<FJsonObject> BuildTargetPayload(
		const TSharedPtr<FJsonObject>& TargetObject,
		const FString& AssetPath,
		const FString& GraphName)
	{
		TSharedRef<FJsonObject> BridgeTarget = MakeShared<FJsonObject>();
		CopyObjectFields(TargetObject, BridgeTarget);
		BridgeTarget->SetStringField(TEXT("asset_path"), AssetPath);
		BridgeTarget->SetStringField(TEXT("graph"), GraphName);
		return BridgeTarget;
	}

	TArray<FString> ReadStepDependsOn(const TSharedPtr<FJsonObject>& StepObject)
	{
		TArray<FString> DependsOn;
		const TArray<TSharedPtr<FJsonValue>>* DependsOnArray = nullptr;
		if (!StepObject.IsValid() ||
			!StepObject->TryGetArrayField(TEXT("depends_on"), DependsOnArray) ||
			!DependsOnArray)
		{
			return DependsOn;
		}

		for (const TSharedPtr<FJsonValue>& Value : *DependsOnArray)
		{
			FString Dependency;
			if (Value.IsValid() && Value->TryGetString(Dependency) && !Dependency.IsEmpty())
			{
				DependsOn.Add(Dependency);
			}
		}
		return DependsOn;
	}

	bool TryReadExecutionPolicyBool(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const TCHAR* FieldName,
		bool& OutValue)
	{
		OutValue = false;
		const TSharedPtr<FJsonObject>* ExecutionPolicyPtr = nullptr;
		return TaskPlan.IsValid() &&
			TaskPlan->TryGetObjectField(TEXT("execution_policy"), ExecutionPolicyPtr) &&
			ExecutionPolicyPtr && ExecutionPolicyPtr->IsValid() &&
			(*ExecutionPolicyPtr)->TryGetBoolField(FieldName, OutValue);
	}
}
