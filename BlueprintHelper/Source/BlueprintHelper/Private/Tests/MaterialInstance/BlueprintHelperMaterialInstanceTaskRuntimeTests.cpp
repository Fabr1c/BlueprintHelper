#if WITH_DEV_AUTOMATION_TESTS

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"
#include "Runtime/TaskRuntime/Clusters/MaterialInstance/BlueprintHelperMaterialInstanceTaskRuntimeCluster.h"
#include "Systems/ToolClusters/MaterialInstance/BlueprintHelperMaterialInstanceMutationAdapter.h"
#include "Systems/ToolClusters/MaterialInstance/BlueprintHelperMaterialInstanceResolver.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

class FBlueprintHelperMaterialInstanceTaskRuntimeTestUtils
{
public:
	static FString MakeAssetName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static FString MakePackagePath(const FString& Prefix)
	{
		return FString::Printf(
			TEXT("/Game/BlueprintHelperMaterialInstanceTaskRuntimeTests/%s"),
			*MakeAssetName(Prefix));
	}

	static FString GetLeafName(const FString& PackagePath)
	{
		int32 SlashIndex = INDEX_NONE;
		if (PackagePath.FindLastChar(TEXT('/'), SlashIndex))
		{
			return PackagePath.Mid(SlashIndex + 1);
		}
		return PackagePath;
	}

	static UMaterial* MakeScalarParentMaterial(UPackage*& OutPackage)
	{
		const FString PackagePath = MakePackagePath(TEXT("M_BH_MI_TaskRuntimeParent"));
		OutPackage = CreatePackage(*PackagePath);
		if (!OutPackage)
		{
			return nullptr;
		}

		UMaterial* Material = NewObject<UMaterial>(
			OutPackage,
			UMaterial::StaticClass(),
			*GetLeafName(PackagePath),
			RF_Public | RF_Standalone | RF_Transactional);
		if (!Material)
		{
			return nullptr;
		}

		UMaterialExpressionScalarParameter* Scalar = Cast<UMaterialExpressionScalarParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(
				Material,
				UMaterialExpressionScalarParameter::StaticClass()));
		if (!Scalar)
		{
			return nullptr;
		}

		Scalar->ParameterName = TEXT("BH_TaskRuntimeScalar");
		Scalar->DefaultValue = 1.25f;

		Material->UpdateCachedExpressionData();
		Material->PostEditChange();
		OutPackage->SetDirtyFlag(false);
		return Material;
	}

	static TSharedRef<FJsonObject> MakeOp(const FString& Op)
	{
		TSharedRef<FJsonObject> OpObject = MakeShared<FJsonObject>();
		OpObject->SetStringField(TEXT("op"), Op);
		return OpObject;
	}

	static TSharedRef<FJsonObject> MakeScalarOp(const FString& Op, double Value)
	{
		TSharedRef<FJsonObject> OpObject = MakeOp(Op);
		OpObject->SetStringField(TEXT("parameter_name"), TEXT("BH_TaskRuntimeScalar"));
		OpObject->SetStringField(TEXT("parameter_type"), TEXT("scalar"));
		OpObject->SetNumberField(TEXT("value"), Value);
		return OpObject;
	}

	static TSharedRef<FJsonObject> MakeScalarReadOp()
	{
		TSharedRef<FJsonObject> OpObject = MakeOp(TEXT("read_effective_value"));
		OpObject->SetStringField(TEXT("parameter_name"), TEXT("BH_TaskRuntimeScalar"));
		OpObject->SetStringField(TEXT("parameter_type"), TEXT("scalar"));
		return OpObject;
	}

	static TSharedRef<FJsonObject> MakeScalarClearOp()
	{
		TSharedRef<FJsonObject> OpObject = MakeOp(TEXT("clear_override"));
		OpObject->SetStringField(TEXT("parameter_name"), TEXT("BH_TaskRuntimeScalar"));
		OpObject->SetStringField(TEXT("parameter_type"), TEXT("scalar"));
		return OpObject;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMaterialInstanceTaskRuntimeCreateSetClearScalarTest,
	"BlueprintHelper.MaterialInstance.TaskRuntime.CreateSetClearScalar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMaterialInstanceTaskRuntimeCreateSetClearScalarTest::RunTest(const FString& Parameters)
{
	UPackage* MaterialPackage = nullptr;
	UMaterial* ParentMaterial =
		FBlueprintHelperMaterialInstanceTaskRuntimeTestUtils::MakeScalarParentMaterial(MaterialPackage);
	TestNotNull(TEXT("parent material fixture is created"), ParentMaterial);
	if (!ParentMaterial)
	{
		return false;
	}

	const FString InstanceAssetPath =
		FBlueprintHelperMaterialInstanceTaskRuntimeTestUtils::MakePackagePath(TEXT("MI_BH_TaskRuntime"));
	TSharedRef<FJsonObject> CreateOp =
		FBlueprintHelperMaterialInstanceTaskRuntimeTestUtils::MakeOp(TEXT("create_material_instance"));
	CreateOp->SetStringField(TEXT("parent_material"), ParentMaterial->GetPathName());

	TArray<TSharedPtr<FJsonValue>> Ops;
	Ops.Add(MakeShared<FJsonValueObject>(CreateOp));
	Ops.Add(MakeShared<FJsonValueObject>(
		FBlueprintHelperMaterialInstanceTaskRuntimeTestUtils::MakeScalarOp(TEXT("set_scalar_override"), 3.5)));
	Ops.Add(MakeShared<FJsonValueObject>(
		FBlueprintHelperMaterialInstanceTaskRuntimeTestUtils::MakeScalarReadOp()));
	Ops.Add(MakeShared<FJsonValueObject>(
		FBlueprintHelperMaterialInstanceTaskRuntimeTestUtils::MakeScalarClearOp()));
	Ops.Add(MakeShared<FJsonValueObject>(
		FBlueprintHelperMaterialInstanceTaskRuntimeTestUtils::MakeScalarReadOp()));

	TSharedRef<FJsonObject> WriteObject = MakeShared<FJsonObject>();
	WriteObject->SetStringField(TEXT("strategy"), TEXT("material_instance_edit"));
	WriteObject->SetArrayField(TEXT("ops"), Ops);

	TSharedRef<FJsonObject> TargetObject = MakeShared<FJsonObject>();
	TargetObject->SetStringField(TEXT("asset_path"), InstanceAssetPath);

	TSharedRef<FJsonObject> StepObject = MakeShared<FJsonObject>();
	StepObject->SetStringField(TEXT("step_id"), TEXT("material_instance_task_runtime_scalar"));
	StepObject->SetStringField(TEXT("capability"), TEXT("material_instance"));
	StepObject->SetObjectField(TEXT("target"), TargetObject);
	StepObject->SetObjectField(TEXT("write"), WriteObject);

	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolError LowerError;
	TestTrue(
		TEXT("material_instance TaskPlan step lowers through task runtime service"),
		FBlueprintHelperTaskRuntimeService::TryLowerTaskPlanStep(
			MakeShared<FJsonObject>(),
			StepObject,
			false,
			LoweredStep,
			LowerError));
	if (!LowerError.Message.IsEmpty())
	{
		AddError(FString::Printf(TEXT("lowering error: %s"), *LowerError.Message));
	}
	TestEqual(TEXT("lowered capability is material_instance"), LoweredStep.Capability, FString(TEXT("material_instance")));
	TestEqual(TEXT("lowered runtime operation is material_instance"), LoweredStep.RuntimeOperation, FString(TEXT("material_instance")));
	TestEqual(TEXT("lowered adapter operation is material_instance_edit"), LoweredStep.AdapterOperation, FString(TEXT("material_instance_edit")));
	TestTrue(
		TEXT("material_instance cluster accepts lowered step"),
		FBlueprintHelperMaterialInstanceTaskRuntimeCluster::CanExecuteStep(LoweredStep));

	const FBlueprintHelperToolResultBase Result =
		FBlueprintHelperMaterialInstanceTaskRuntimeCluster().ExecuteStep(LoweredStep);
	TestTrue(TEXT("material_instance mutation executes"), Result.bOk);
	TestTrue(TEXT("material_instance mutation modifies the asset"), Result.bModified);
	TestTrue(TEXT("result data is present"), Result.Data.IsValid());
	if (!Result.bOk || !Result.Data.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* OperationResults = nullptr;
	TestTrue(
		TEXT("operation results are returned"),
		Result.Data->TryGetArrayField(TEXT("operations"), OperationResults) &&
		OperationResults &&
		OperationResults->Num() == 5);

	const FBlueprintHelperMaterialInstanceAssetResolveResult AssetResult =
		FBlueprintHelperMaterialInstanceResolver::ResolveAsset(InstanceAssetPath);
	TestTrue(TEXT("created material instance resolves"), AssetResult.bSuccess);
	TestNotNull(TEXT("created material instance object exists"), AssetResult.Instance);
	if (!AssetResult.bSuccess || !AssetResult.Instance)
	{
		return false;
	}

	const FBlueprintHelperMaterialInstanceParameterResolveResult ScalarResult =
		FBlueprintHelperMaterialInstanceResolver::ResolveParameter(
			AssetResult.Instance,
			TEXT("BH_TaskRuntimeScalar"),
			EBlueprintHelperMaterialInstanceParameterType::Scalar);
	TestTrue(TEXT("scalar parameter resolves after clear"), ScalarResult.bSuccess);
	TestFalse(TEXT("scalar override is cleared"), ScalarResult.Parameter.bHasOverride);
	TestEqual(
		TEXT("scalar source is inherited after clear"),
		BlueprintHelperMaterialInstanceParameterSourceToString(ScalarResult.Parameter.Source),
		FString(TEXT("inherited")));
	TestEqual(TEXT("scalar effective value returns to parent default"), ScalarResult.Parameter.EffectiveValue.Scalar, 1.25f);

	MaterialPackage->SetDirtyFlag(false);
	if (AssetResult.Instance->GetPackage())
	{
		AssetResult.Instance->GetPackage()->SetDirtyFlag(false);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperMaterialInstanceTaskRuntimePreviewCreateThenOverrideTest,
	"BlueprintHelper.MaterialInstance.TaskRuntime.PreviewCreateThenOverride",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperMaterialInstanceTaskRuntimePreviewCreateThenOverrideTest::RunTest(const FString& Parameters)
{
	UPackage* MaterialPackage = nullptr;
	UMaterial* ParentMaterial =
		FBlueprintHelperMaterialInstanceTaskRuntimeTestUtils::MakeScalarParentMaterial(MaterialPackage);
	TestNotNull(TEXT("parent material fixture is created"), ParentMaterial);
	if (!ParentMaterial)
	{
		return false;
	}

	const FString InstanceAssetPath =
		FBlueprintHelperMaterialInstanceTaskRuntimeTestUtils::MakePackagePath(TEXT("MI_BH_TaskRuntimePreview"));
	TSharedRef<FJsonObject> CreateOp =
		FBlueprintHelperMaterialInstanceTaskRuntimeTestUtils::MakeOp(TEXT("create_material_instance"));
	CreateOp->SetStringField(TEXT("parent_material"), ParentMaterial->GetPathName());

	TArray<TSharedPtr<FJsonValue>> Ops;
	Ops.Add(MakeShared<FJsonValueObject>(CreateOp));
	Ops.Add(MakeShared<FJsonValueObject>(
		FBlueprintHelperMaterialInstanceTaskRuntimeTestUtils::MakeScalarOp(TEXT("set_scalar_override"), 2.5)));
	Ops.Add(MakeShared<FJsonValueObject>(
		FBlueprintHelperMaterialInstanceTaskRuntimeTestUtils::MakeScalarReadOp()));
	Ops.Add(MakeShared<FJsonValueObject>(
		FBlueprintHelperMaterialInstanceTaskRuntimeTestUtils::MakeScalarClearOp()));
	Ops.Add(MakeShared<FJsonValueObject>(
		FBlueprintHelperMaterialInstanceTaskRuntimeTestUtils::MakeScalarReadOp()));

	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("asset_path"), InstanceAssetPath);
	Payload->SetBoolField(TEXT("dry_run"), true);
	Payload->SetArrayField(TEXT("ops"), Ops);

	const FBlueprintHelperToolResultBase Result =
		FBlueprintHelperMaterialInstanceMutationAdapter::ExecutePayload(Payload);
	TestTrue(TEXT("dry-run create plus override sequence succeeds"), Result.bOk);
	TestFalse(TEXT("dry-run sequence does not modify assets"), Result.bModified);
	TestTrue(TEXT("dry-run result data is present"), Result.Data.IsValid());
	if (!Result.bOk || !Result.Data.IsValid())
	{
		if (Result.Error.IsSet() && Result.Error->Message.Len() > 0)
		{
			AddError(FString::Printf(TEXT("dry-run error: %s"), *Result.Error->Message));
		}
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* OperationResults = nullptr;
	TestTrue(
		TEXT("dry-run operation results are returned"),
		Result.Data->TryGetArrayField(TEXT("operations"), OperationResults) &&
		OperationResults &&
		OperationResults->Num() == 5);
	if (OperationResults && OperationResults->Num() == 5)
	{
		const TSharedPtr<FJsonObject> ReadAfterSet = (*OperationResults)[2]->AsObject();
		const TSharedPtr<FJsonObject> ReadAfterClear = (*OperationResults)[4]->AsObject();
		const TSharedPtr<FJsonObject>* ParameterAfterSet = nullptr;
		const TSharedPtr<FJsonObject>* ParameterAfterClear = nullptr;
		TestTrue(
			TEXT("read after set has parameter"),
			ReadAfterSet.IsValid() &&
			ReadAfterSet->TryGetObjectField(TEXT("parameter"), ParameterAfterSet) &&
			ParameterAfterSet &&
			ParameterAfterSet->IsValid());
		TestTrue(
			TEXT("read after clear has parameter"),
			ReadAfterClear.IsValid() &&
			ReadAfterClear->TryGetObjectField(TEXT("parameter"), ParameterAfterClear) &&
			ParameterAfterClear &&
			ParameterAfterClear->IsValid());
		if (ParameterAfterSet && ParameterAfterSet->IsValid())
		{
			TestEqual(
				TEXT("preview read after set sees override source"),
				(*ParameterAfterSet)->GetStringField(TEXT("source")),
				FString(TEXT("override")));
		}
		if (ParameterAfterClear && ParameterAfterClear->IsValid())
		{
			TestEqual(
				TEXT("preview read after clear sees inherited source"),
				(*ParameterAfterClear)->GetStringField(TEXT("source")),
				FString(TEXT("inherited")));
		}
	}

	const FBlueprintHelperMaterialInstanceAssetResolveResult AssetResult =
		FBlueprintHelperMaterialInstanceResolver::ResolveAsset(InstanceAssetPath);
	TestFalse(TEXT("dry-run did not create the target asset"), AssetResult.bSuccess);

	if (MaterialPackage)
	{
		MaterialPackage->SetDirtyFlag(false);
	}
	return true;
}

#endif
