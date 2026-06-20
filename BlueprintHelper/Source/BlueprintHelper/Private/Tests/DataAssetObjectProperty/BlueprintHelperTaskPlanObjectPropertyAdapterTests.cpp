#if WITH_DEV_AUTOMATION_TESTS

#include "Runtime/TaskRuntime/TaskPlanAdapters/DataAssetObjectProperty/BlueprintHelperObjectPropertyTaskPlanAdapter.h"

#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Character.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Systems/ToolClusters/ObjectProperty/BlueprintHelperPropertyReflectionService.h"
#include "UObject/Package.h"

class FBlueprintHelperTaskPlanObjectPropertyAdapterTestsLocalUtils
{
public:
	static TSharedPtr<FJsonObject> MakeObjectPropertyStep(const FString& OpName)
	{
		TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
		Step->SetStringField(TEXT("step_id"), TEXT("step_object_property"));
		Step->SetStringField(TEXT("capability"), FBlueprintHelperObjectPropertyTaskPlanAdapter::CapabilityObjectProperty);

		TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), TEXT("/Game/Data/DA_CombatTuning"));
		Step->SetObjectField(TEXT("target"), Target);

		TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), OpName);

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

		TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), FBlueprintHelperObjectPropertyTaskPlanAdapter::StrategyPropertyEdit);
		Write->SetArrayField(TEXT("ops"), Ops);
		Step->SetObjectField(TEXT("write"), Write);

		return Step;
	}

	static TSharedPtr<FJsonObject> GetFirstObjectPropertyOp(const TSharedPtr<FJsonObject>& Step)
	{
		const TSharedPtr<FJsonObject>* Write = nullptr;
		if (!Step.IsValid() || !Step->TryGetObjectField(TEXT("write"), Write) || !Write || !Write->IsValid())
		{
			return nullptr;
		}

		const TArray<TSharedPtr<FJsonValue>>* Ops = nullptr;
		if (!(*Write)->TryGetArrayField(TEXT("ops"), Ops) || !Ops || Ops->Num() == 0)
		{
			return nullptr;
		}

		return (*Ops)[0].IsValid() ? (*Ops)[0]->AsObject() : nullptr;
	}

	static FString MakeObjectPropertyTestObjectName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static UPackage* MakeObjectPropertyTestPackage(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperObjectPropertyTests/%s"),
			*MakeObjectPropertyTestObjectName(Prefix)));
		Package->SetDirtyFlag(false);
		return Package;
	}

	static UBlueprint* MakeCharacterBlueprint(const FString& Prefix)
	{
		UPackage* Package = MakeObjectPropertyTestPackage(Prefix);
		if (!Package)
		{
			return nullptr;
		}

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ACharacter::StaticClass(),
			Package,
			*MakeObjectPropertyTestObjectName(TEXT("BP_ObjectPropertyCharacter")),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperTaskPlanObjectPropertyAdapterTests"));
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperObjectPropertyAdapterSetSinglePropertyTest,
	"BlueprintHelper.TaskPlan.ObjectPropertyAdapter.SetSingleProperty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperObjectPropertyAdapterSetSinglePropertyTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanObjectPropertyAdapterTestsLocalUtils::MakeObjectPropertyStep(FBlueprintHelperObjectPropertyTaskPlanAdapter::OpSetObjectProperty);
	TSharedPtr<FJsonObject> Op = FBlueprintHelperTaskPlanObjectPropertyAdapterTestsLocalUtils::GetFirstObjectPropertyOp(Step);
	TestNotNull(TEXT("object_property op exists"), Op.Get());

	Op->SetStringField(TEXT("property_path"), TEXT("CombatRules.DamageScale"));
	Op->SetNumberField(TEXT("value"), 1.5);

	FBlueprintHelperObjectPropertyTaskPlanPayload BuiltPayload;
	FBlueprintHelperToolError Error;
	const bool bBuilt = FBlueprintHelperObjectPropertyTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
		Step,
		true,
		BuiltPayload,
		Error);

	TestTrue(TEXT("set_object_property lowers successfully"), bBuilt);
	TestEqual(TEXT("capability preserved"), BuiltPayload.Capability, FString(FBlueprintHelperObjectPropertyTaskPlanAdapter::CapabilityObjectProperty));
	TestEqual(TEXT("runtime operation is object_property"), BuiltPayload.RuntimeOperation, FString(FBlueprintHelperObjectPropertyTaskPlanAdapter::RuntimeOperationObjectProperty));
	TestEqual(TEXT("adapter operation is set_object_property"), BuiltPayload.AdapterOperation, FString(FBlueprintHelperObjectPropertyTaskPlanAdapter::AdapterOperationSetObjectProperty));
	TestTrue(TEXT("object_property adapter supports service dry-run"), BuiltPayload.bAdapterDryRunSupported);
	TestNotNull(TEXT("payload exists"), BuiltPayload.Payload.Get());

	FString AssetPath;
	FString PropertyPath;
	bool bDryRun = false;
	TSharedPtr<FJsonValue> Value;
	TestTrue(TEXT("payload carries asset_path"), BuiltPayload.Payload->TryGetStringField(TEXT("asset_path"), AssetPath));
	TestTrue(TEXT("payload carries property_path"), BuiltPayload.Payload->TryGetStringField(TEXT("property_path"), PropertyPath));
	Value = FBlueprintHelperVersionCompat::FindJsonValue(BuiltPayload.Payload, TEXT("value"));
	TestTrue(TEXT("payload carries value"), Value.IsValid());
	TestTrue(TEXT("payload carries dry_run"), BuiltPayload.Payload->TryGetBoolField(TEXT("dry_run"), bDryRun));

	TestEqual(TEXT("asset_path matches target"), AssetPath, FString(TEXT("/Game/Data/DA_CombatTuning")));
	TestEqual(TEXT("property_path preserved"), PropertyPath, FString(TEXT("CombatRules.DamageScale")));
	TestTrue(TEXT("numeric value is preserved for service validation"), Value.IsValid() && Value->Type == EJson::Number);
	TestTrue(TEXT("preview dry_run is recorded"), bDryRun);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperObjectPropertyAdapterSetBatchPropertiesTest,
	"BlueprintHelper.TaskPlan.ObjectPropertyAdapter.SetBatchProperties",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperObjectPropertyAdapterSetBatchPropertiesTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanObjectPropertyAdapterTestsLocalUtils::MakeObjectPropertyStep(FBlueprintHelperObjectPropertyTaskPlanAdapter::OpSetObjectProperties);
	TSharedPtr<FJsonObject> Op = FBlueprintHelperTaskPlanObjectPropertyAdapterTestsLocalUtils::GetFirstObjectPropertyOp(Step);
	TestNotNull(TEXT("object_property op exists"), Op.Get());

	TArray<TSharedPtr<FJsonValue>> Settings;
	TSharedPtr<FJsonObject> FirstSetting = MakeShared<FJsonObject>();
	FirstSetting->SetStringField(TEXT("property_path"), TEXT("DamageScale"));
	FirstSetting->SetNumberField(TEXT("value"), 2.0);
	Settings.Add(MakeShared<FJsonValueObject>(FirstSetting.ToSharedRef()));

	TSharedPtr<FJsonObject> SecondSetting = MakeShared<FJsonObject>();
	SecondSetting->SetStringField(TEXT("property_path"), TEXT("bEnabled"));
	SecondSetting->SetBoolField(TEXT("value"), true);
	Settings.Add(MakeShared<FJsonValueObject>(SecondSetting.ToSharedRef()));
	Op->SetArrayField(TEXT("settings"), Settings);

	FBlueprintHelperObjectPropertyTaskPlanPayload BuiltPayload;
	FBlueprintHelperToolError Error;
	const bool bBuilt = FBlueprintHelperObjectPropertyTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
		Step,
		false,
		BuiltPayload,
		Error);

	TestTrue(TEXT("set_object_properties lowers successfully"), bBuilt);
	TestEqual(TEXT("adapter operation is set_object_properties"), BuiltPayload.AdapterOperation, FString(FBlueprintHelperObjectPropertyTaskPlanAdapter::AdapterOperationSetObjectProperties));
	TestTrue(TEXT("object_property adapter supports service dry-run"), BuiltPayload.bAdapterDryRunSupported);

	FString AssetPath;
	bool bDryRun = true;
	const TArray<TSharedPtr<FJsonValue>>* PayloadSettings = nullptr;
	TestTrue(TEXT("payload carries asset_path"), BuiltPayload.Payload->TryGetStringField(TEXT("asset_path"), AssetPath));
	TestTrue(TEXT("payload carries settings"), BuiltPayload.Payload->TryGetArrayField(TEXT("settings"), PayloadSettings));
	TestTrue(TEXT("payload carries dry_run"), BuiltPayload.Payload->TryGetBoolField(TEXT("dry_run"), bDryRun));
	TestEqual(TEXT("asset_path matches target"), AssetPath, FString(TEXT("/Game/Data/DA_CombatTuning")));
	TestEqual(TEXT("settings copied"), PayloadSettings ? PayloadSettings->Num() : 0, 2);
	TestFalse(TEXT("execute dry_run is recorded as false"), bDryRun);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperObjectPropertyAdapterRejectsOperationFieldTest,
	"BlueprintHelper.TaskPlan.ObjectPropertyAdapter.RejectsOperationField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperObjectPropertyAdapterRejectsOperationFieldTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanObjectPropertyAdapterTestsLocalUtils::MakeObjectPropertyStep(FBlueprintHelperObjectPropertyTaskPlanAdapter::OpSetObjectProperty);
	Step->SetStringField(TEXT("operation"), TEXT("set_object_property"));

	FBlueprintHelperObjectPropertyTaskPlanPayload BuiltPayload;
	FBlueprintHelperToolError Error;
	const bool bBuilt = FBlueprintHelperObjectPropertyTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
		Step,
		true,
		BuiltPayload,
		Error);

	TestFalse(TEXT("object_property IR rejects adapter operation field"), bBuilt);
	TestEqual(TEXT("operation field error code"), Error.Code, FString(TEXT("unsupported_object_property_operation_field")));
	TestEqual(TEXT("operation field error path"), Error.Field, FString(TEXT("task_plan.steps[0].operation")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperObjectPropertyAdapterRejectsReadOpTest,
	"BlueprintHelper.TaskPlan.ObjectPropertyAdapter.RejectsReadOp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperObjectPropertyAdapterRejectsReadOpTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanObjectPropertyAdapterTestsLocalUtils::MakeObjectPropertyStep(TEXT("read_object_property"));

	FBlueprintHelperObjectPropertyTaskPlanPayload BuiltPayload;
	FBlueprintHelperToolError Error;
	const bool bBuilt = FBlueprintHelperObjectPropertyTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
		Step,
		true,
		BuiltPayload,
		Error);

	TestFalse(TEXT("read_object_property is not part of write lowering"), bBuilt);
	TestEqual(TEXT("read op error code"), Error.Code, FString(TEXT("unsupported_object_property_read_op")));
	TestEqual(TEXT("read op error path"), Error.Field, FString(TEXT("task_plan.steps[0].write.ops[0].op")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperObjectPropertyAdapterRejectsInvalidBatchSettingsTest,
	"BlueprintHelper.TaskPlan.ObjectPropertyAdapter.RejectsInvalidBatchSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperObjectPropertyAdapterRejectsInvalidBatchSettingsTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanObjectPropertyAdapterTestsLocalUtils::MakeObjectPropertyStep(FBlueprintHelperObjectPropertyTaskPlanAdapter::OpSetObjectProperties);

	FBlueprintHelperObjectPropertyTaskPlanPayload BuiltPayload;
	FBlueprintHelperToolError Error;
	const bool bBuilt = FBlueprintHelperObjectPropertyTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
		Step,
		true,
		BuiltPayload,
		Error);

	TestFalse(TEXT("batch op requires settings"), bBuilt);
	TestEqual(TEXT("missing settings error code"), Error.Code, FString(TEXT("invalid_object_property_settings")));
	TestEqual(TEXT("missing settings error path"), Error.Field, FString(TEXT("task_plan.steps[0].write.ops[0].settings")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperObjectPropertyServiceDryRunDoesNotMutatePropertyTest,
	"BlueprintHelper.TaskPlan.ObjectPropertyAdapter.ServiceDryRun.DoesNotMutateProperty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperObjectPropertyServiceDryRunDoesNotMutatePropertyTest::RunTest(const FString& Parameters)
{
	UPackage* Package = FBlueprintHelperTaskPlanObjectPropertyAdapterTestsLocalUtils::MakeObjectPropertyTestPackage(TEXT("DryRun"));
	UTextBlock* TextBlock = NewObject<UTextBlock>(
		Package,
		*FBlueprintHelperTaskPlanObjectPropertyAdapterTestsLocalUtils::MakeObjectPropertyTestObjectName(TEXT("TextBlock")),
		RF_Public | RF_Standalone | RF_Transactional);
	TestNotNull(TEXT("test object is created"), TextBlock);

	TextBlock->SetRenderOpacity(1.0f);
	Package->SetDirtyFlag(false);

	FBlueprintHelperPropertyReflectionService Service;
	const FBlueprintHelperSetPropertyResult Result = Service.SetObjectProperty(
		TextBlock->GetPathName(),
		TEXT("RenderOpacity"),
		TEXT("0.25"),
		true);

	TestTrue(TEXT("dry-run validates successfully"), Result.bSuccess);
	TestTrue(TEXT("result records dry-run"), Result.bDryRun);
	TestEqual(TEXT("property path is reported"), Result.PropertyName, FString(TEXT("RenderOpacity")));
	TestEqual(TEXT("dry-run does not change render opacity"), TextBlock->GetRenderOpacity(), 1.0f);
	TestFalse(TEXT("dry-run does not dirty package"), Package->IsDirty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperObjectPropertyServiceDryRunRejectsInvalidValueTest,
	"BlueprintHelper.TaskPlan.ObjectPropertyAdapter.ServiceDryRun.RejectsInvalidValue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperObjectPropertyServiceDryRunRejectsInvalidValueTest::RunTest(const FString& Parameters)
{
	UPackage* Package = FBlueprintHelperTaskPlanObjectPropertyAdapterTestsLocalUtils::MakeObjectPropertyTestPackage(TEXT("InvalidDryRun"));
	UTextBlock* TextBlock = NewObject<UTextBlock>(
		Package,
		*FBlueprintHelperTaskPlanObjectPropertyAdapterTestsLocalUtils::MakeObjectPropertyTestObjectName(TEXT("TextBlock")),
		RF_Public | RF_Standalone | RF_Transactional);
	TestNotNull(TEXT("test object is created"), TextBlock);

	TextBlock->SetRenderOpacity(1.0f);
	Package->SetDirtyFlag(false);

	FBlueprintHelperPropertyReflectionService Service;
	const FBlueprintHelperSetPropertyResult Result = Service.SetObjectProperty(
		TextBlock->GetPathName(),
		TEXT("RenderOpacity"),
		TEXT("not_a_float"),
		true);

	TestFalse(TEXT("dry-run rejects invalid import text"), Result.bSuccess);
	TestTrue(TEXT("failed result still records dry-run"), Result.bDryRun);
	TestEqual(TEXT("invalid dry-run does not change render opacity"), TextBlock->GetRenderOpacity(), 1.0f);
	TestFalse(TEXT("invalid dry-run does not dirty package"), Package->IsDirty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperObjectPropertyServiceReadsBlueprintCdoNestedNativeComponentPropertyTest,
	"BlueprintHelper.TaskPlan.ObjectPropertyAdapter.ServiceRead.BlueprintCdoNestedNativeComponentProperty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperObjectPropertyServiceReadsBlueprintCdoNestedNativeComponentPropertyTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperTaskPlanObjectPropertyAdapterTestsLocalUtils::MakeCharacterBlueprint(TEXT("BlueprintCdoRead"));
	TestNotNull(TEXT("character blueprint is created"), Blueprint);
	TestNotNull(TEXT("generated class exists"), Blueprint ? Blueprint->GeneratedClass.Get() : nullptr);

	FBlueprintHelperPropertyReflectionService Service;
	const FBlueprintHelperObjectPropertiesResult Result = Service.GetObjectProperties(
		Blueprint->GetPathName(),
		TEXT("CharacterMovement.MaxWalkSpeed"));

	TestTrue(TEXT("targeted property read succeeds"), Result.bSuccess);
	TestEqual(TEXT("targeted property count"), Result.Properties.Num(), 1);
	if (Result.Properties.Num() == 1)
	{
		TestEqual(TEXT("property path is preserved"), Result.Properties[0].Name, FString(TEXT("CharacterMovement.MaxWalkSpeed")));
		TestTrue(TEXT("property type is exported"), !Result.Properties[0].TypeName.IsEmpty());
		TestTrue(TEXT("property value is exported"), !Result.Properties[0].Value.IsEmpty());
	}

	return true;
}

#endif
