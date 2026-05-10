#include "Runtime/TaskRuntime/TaskPlanAdapters/AssetFactory/BlueprintHelperAssetFactoryTaskPlanAdapter.h"

#include "Dom/JsonValue.h"

class FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils
{
public:
	static FBlueprintHelperToolError MakeAssetFactoryTaskPlanError(
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

	static FString AssetFactoryBuildStepFieldPath(const FString& Suffix)
	{
		return Suffix.IsEmpty()
			? FString(TEXT("task_plan.steps[0]"))
			: FString::Printf(TEXT("task_plan.steps[0].%s"), *Suffix);
	}

	static FString AssetFactoryBuildOpFieldPath(const FString& Suffix)
	{
		return Suffix.IsEmpty()
			? FString(TEXT("task_plan.steps[0].write.ops[0]"))
			: FString::Printf(TEXT("task_plan.steps[0].write.ops[0].%s"), *Suffix);
	}

	static bool AssetFactoryTryReadRequiredString(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		const FString& FieldPath,
		const FString& ErrorCode,
		const FString& ErrorMessage,
		FString& OutValue,
		FBlueprintHelperToolError& OutError)
	{
		OutValue.Empty();
		if (!Object.IsValid() ||
			!Object->TryGetStringField(FieldName, OutValue) ||
			OutValue.IsEmpty())
		{
			OutError = MakeAssetFactoryTaskPlanError(
				ErrorCode,
				ErrorMessage,
				FieldPath);
			return false;
		}

		return true;
	}

	static bool AssetFactoryTryCopyOptionalStringField(
		const TSharedPtr<FJsonObject>& Source,
		const TCHAR* FieldName,
		const FString& FieldPath,
		const TSharedRef<FJsonObject>& Destination,
		FBlueprintHelperToolError& OutError)
	{
		if (!Source.IsValid() || !Source->HasField(FieldName))
		{
			return true;
		}

		FString Value;
		if (!Source->TryGetStringField(FieldName, Value))
		{
			OutError = MakeAssetFactoryTaskPlanError(
				TEXT("invalid_asset_factory_create_asset_op"),
				FString::Printf(TEXT("Asset Factory create_asset field %s must be a string."), FieldName),
				FieldPath);
			return false;
		}

		Destination->SetStringField(FieldName, Value);
		return true;
	}

	static bool AssetFactoryTryCopyOptionalArrayField(
		const TSharedPtr<FJsonObject>& Source,
		const TCHAR* FieldName,
		const FString& FieldPath,
		const TSharedRef<FJsonObject>& Destination,
		FBlueprintHelperToolError& OutError)
	{
		if (!Source.IsValid() || !Source->HasField(FieldName))
		{
			return true;
		}

		const TArray<TSharedPtr<FJsonValue>>* Value = nullptr;
		if (!Source->TryGetArrayField(FieldName, Value) || !Value)
		{
			OutError = MakeAssetFactoryTaskPlanError(
				TEXT("invalid_asset_factory_create_asset_op"),
				FString::Printf(TEXT("Asset Factory create_asset field %s must be an array."), FieldName),
				FieldPath);
			return false;
		}

		Destination->SetArrayField(FieldName, *Value);
		return true;
	}

};

bool FBlueprintHelperAssetFactoryTaskPlanAdapter::SupportsStep(
	const TSharedPtr<FJsonObject>& StepObject)
{
	FString Capability;
	return StepObject.IsValid() &&
		StepObject->TryGetStringField(TEXT("capability"), Capability) &&
		Capability == SupportedCapability;
}

bool FBlueprintHelperAssetFactoryTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
	const TSharedPtr<FJsonObject>& /*TaskPlan*/,
	const TSharedPtr<FJsonObject>& StepObject,
	bool bDryRun,
	TSharedPtr<FJsonObject>& OutPayload,
	FBlueprintHelperToolError& OutError)
{
	OutPayload.Reset();
	OutError = FBlueprintHelperToolError();

	if (!StepObject.IsValid())
	{
		OutError = MakeError(
			TEXT("invalid_asset_factory_step"),
			TEXT("Asset Factory TaskPlan step must be an object."),
			FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils::AssetFactoryBuildStepFieldPath(TEXT("")));
		return false;
	}

	FString Capability;
	if (!StepObject->TryGetStringField(TEXT("capability"), Capability) ||
		Capability != SupportedCapability)
	{
		OutError = MakeError(
			TEXT("unsupported_asset_factory_capability"),
			TEXT("Asset Factory adapter supports asset_factory capability only."),
			FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils::AssetFactoryBuildStepFieldPath(TEXT("capability")));
		return false;
	}

	FString AdapterOperationField;
	if (StepObject->TryGetStringField(TEXT("operation"), AdapterOperationField))
	{
		OutError = MakeError(
			TEXT("unsupported_asset_factory_operation_field"),
			TEXT("Asset Factory IR TaskPlan steps use capability/write; adapter operation fields are runtime lowering details."),
			FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils::AssetFactoryBuildStepFieldPath(TEXT("operation")));
		return false;
	}

	const TSharedPtr<FJsonObject>* TargetObjectPtr = nullptr;
	if (!StepObject->TryGetObjectField(TEXT("target"), TargetObjectPtr) ||
		!TargetObjectPtr || !TargetObjectPtr->IsValid())
	{
		OutError = MakeError(
			TEXT("invalid_asset_factory_target"),
			TEXT("Asset Factory TaskPlan step requires target object."),
			FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils::AssetFactoryBuildStepFieldPath(TEXT("target")));
		return false;
	}

	FString AssetPath;
	if (!FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils::AssetFactoryTryReadRequiredString(
		*TargetObjectPtr,
		TEXT("asset_path"),
		FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils::AssetFactoryBuildStepFieldPath(TEXT("target.asset_path")),
		TEXT("invalid_asset_factory_target"),
		TEXT("Asset Factory TaskPlan target requires asset_path."),
		AssetPath,
		OutError))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* WriteObjectPtr = nullptr;
	if (!StepObject->TryGetObjectField(TEXT("write"), WriteObjectPtr) ||
		!WriteObjectPtr || !WriteObjectPtr->IsValid())
	{
		OutError = MakeError(
			TEXT("invalid_asset_factory_write"),
			TEXT("asset_factory TaskPlan step requires write object."),
			FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils::AssetFactoryBuildStepFieldPath(TEXT("write")));
		return false;
	}

	FString Strategy;
	if (!(*WriteObjectPtr)->TryGetStringField(TEXT("strategy"), Strategy) ||
		Strategy != SupportedStrategy)
	{
		OutError = MakeError(
			TEXT("unsupported_asset_factory_strategy"),
			TEXT("Asset Factory Task Runtime currently supports asset_create strategy only."),
			FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils::AssetFactoryBuildStepFieldPath(TEXT("write.strategy")));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* OpsArray = nullptr;
	if (!(*WriteObjectPtr)->TryGetArrayField(TEXT("ops"), OpsArray) || !OpsArray)
	{
		OutError = MakeError(
			TEXT("invalid_asset_factory_ops"),
			TEXT("asset_factory TaskPlan step requires write.ops array."),
			FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils::AssetFactoryBuildStepFieldPath(TEXT("write.ops")));
		return false;
	}

	if (OpsArray->Num() != 1)
	{
		OutError = MakeError(
			TEXT("unsupported_asset_factory_op_count"),
			TEXT("Asset Factory Task Runtime currently supports exactly one create_asset op."),
			FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils::AssetFactoryBuildStepFieldPath(TEXT("write.ops")));
		return false;
	}

	const TSharedPtr<FJsonObject> OpObject =
		(*OpsArray)[0].IsValid()
			? (*OpsArray)[0]->AsObject()
			: nullptr;
	if (!OpObject.IsValid())
	{
		OutError = MakeError(
			TEXT("invalid_asset_factory_create_asset_op"),
			TEXT("Asset Factory create_asset op must be an object."),
			FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils::AssetFactoryBuildOpFieldPath(TEXT("")));
		return false;
	}

	FString OpName;
	if (!OpObject->TryGetStringField(TEXT("op"), OpName) || OpName != SupportedOp)
	{
		OutError = MakeError(
			TEXT("unsupported_asset_factory_op"),
			TEXT("Asset Factory Task Runtime currently supports create_asset only."),
			FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils::AssetFactoryBuildOpFieldPath(TEXT("op")));
		return false;
	}

	FString AssetType;
	if (!FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils::AssetFactoryTryReadRequiredString(
		OpObject,
		TEXT("asset_type"),
		FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils::AssetFactoryBuildOpFieldPath(TEXT("asset_type")),
		TEXT("invalid_asset_factory_create_asset_op"),
		TEXT("Asset Factory create_asset op requires asset_type."),
		AssetType,
		OutError))
	{
		return false;
	}

	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("asset_path"), AssetPath);
	Payload->SetStringField(TEXT("asset_type"), AssetType);
	Payload->SetBoolField(TEXT("dry_run"), bDryRun);

	if (!FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils::AssetFactoryTryCopyOptionalStringField(OpObject, TEXT("parent_class"), FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils::AssetFactoryBuildOpFieldPath(TEXT("parent_class")), Payload, OutError) ||
		!FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils::AssetFactoryTryCopyOptionalStringField(OpObject, TEXT("value_type"), FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils::AssetFactoryBuildOpFieldPath(TEXT("value_type")), Payload, OutError) ||
		!FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils::AssetFactoryTryCopyOptionalStringField(OpObject, TEXT("row_struct"), FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils::AssetFactoryBuildOpFieldPath(TEXT("row_struct")), Payload, OutError) ||
		!FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils::AssetFactoryTryCopyOptionalStringField(OpObject, TEXT("data_asset_class"), FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils::AssetFactoryBuildOpFieldPath(TEXT("data_asset_class")), Payload, OutError) ||
		!FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils::AssetFactoryTryCopyOptionalStringField(OpObject, TEXT("collision"), FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils::AssetFactoryBuildOpFieldPath(TEXT("collision")), Payload, OutError))
	{
		return false;
	}

	if (!FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils::AssetFactoryTryCopyOptionalArrayField(OpObject, TEXT("fields"), FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils::AssetFactoryBuildOpFieldPath(TEXT("fields")), Payload, OutError))
	{
		return false;
	}

	OutPayload = Payload;
	return true;
}

bool FBlueprintHelperAssetFactoryTaskPlanAdapter::TryBuildPayload(
	const TSharedPtr<FJsonObject>& TaskPlan,
	const TSharedPtr<FJsonObject>& StepObject,
	bool bDryRun,
	TSharedPtr<FJsonObject>& OutPayload,
	FBlueprintHelperToolError& OutError) const
{
	return TryBuildPayloadFromTaskPlanStep(TaskPlan, StepObject, bDryRun, OutPayload, OutError);
}

FBlueprintHelperToolError FBlueprintHelperAssetFactoryTaskPlanAdapter::MakeError(
	const FString& Code,
	const FString& Message,
	const FString& Field)
{
	return FBlueprintHelperAssetFactoryTaskPlanAdapterLocalUtils::MakeAssetFactoryTaskPlanError(Code, Message, Field);
}
