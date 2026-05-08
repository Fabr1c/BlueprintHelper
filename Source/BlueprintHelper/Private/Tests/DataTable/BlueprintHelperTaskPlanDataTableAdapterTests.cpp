#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/DataTable.h"
#include "Misc/AutomationTest.h"
#include "Systems/ToolClusters/DataTable/BlueprintHelperDataTableService.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/DataTable/BlueprintHelperDataTableTaskPlanAdapter.h"
#include "UObject/NoExportTypes.h"
#include "UObject/Package.h"

namespace
{
	TSharedPtr<FJsonObject> MakeDataTableTaskPlanStep(const FString& OpName)
	{
		TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
		Step->SetStringField(TEXT("step_id"), TEXT("step_data_table"));
		Step->SetStringField(TEXT("capability"), FBlueprintHelperDataTableTaskPlanAdapter::CapabilityDataTable);

		TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), TEXT("/Game/Data/DT_Weapons"));
		Step->SetObjectField(TEXT("target"), Target);

		TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), OpName);

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

		TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), FBlueprintHelperDataTableTaskPlanAdapter::StrategyRowEdit);
		Write->SetArrayField(TEXT("ops"), Ops);
		Step->SetObjectField(TEXT("write"), Write);

		return Step;
	}

	TSharedPtr<FJsonObject> GetFirstDataTableOp(const TSharedPtr<FJsonObject>& Step)
	{
		const TSharedPtr<FJsonObject>* Write = nullptr;
		if (!Step->TryGetObjectField(TEXT("write"), Write) || !Write || !Write->IsValid())
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

	TSharedPtr<FJsonObject> MakeWeaponFields()
	{
		TSharedPtr<FJsonObject> Fields = MakeShared<FJsonObject>();
		Fields->SetStringField(TEXT("Damage"), TEXT("12"));
		Fields->SetStringField(TEXT("Cooldown"), TEXT("0.25"));
		return Fields;
	}

	FString MakeDataTableTestObjectName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	UPackage* MakeDataTableTestPackage(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperDataTableTests/%s"),
			*MakeDataTableTestObjectName(Prefix)));
		Package->SetDirtyFlag(false);
		return Package;
	}

	UDataTable* MakeVectorDataTable(
		UPackage* Package,
		const FName TableName,
		const FName RowName,
		const FVector& InitialValue)
	{
		UDataTable* DataTable = NewObject<UDataTable>(Package, TableName, RF_Public | RF_Standalone | RF_Transactional);
		TMap<FName, const uint8*> RawRows;
		RawRows.Add(RowName, reinterpret_cast<const uint8*>(&InitialValue));
		DataTable->CreateTableFromRawData(RawRows, TBaseStructure<FVector>::Get());
		Package->SetDirtyFlag(false);
		return DataTable;
	}

	TMap<FString, FString> MakeVectorFields(const double X, const double Y, const double Z)
	{
		TMap<FString, FString> Fields;
		Fields.Add(TEXT("X"), FString::SanitizeFloat(X));
		Fields.Add(TEXT("Y"), FString::SanitizeFloat(Y));
		Fields.Add(TEXT("Z"), FString::SanitizeFloat(Z));
		return Fields;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanDataTableAdapterAddRowTest,
	"BlueprintHelper.TaskPlan.DataTableAdapter.AddRow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanDataTableAdapterAddRowTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Step = MakeDataTableTaskPlanStep(FBlueprintHelperDataTableTaskPlanAdapter::OpAddRow);
	TSharedPtr<FJsonObject> Op = GetFirstDataTableOp(Step);
	TestNotNull(TEXT("data_table op exists"), Op.Get());

	Op->SetStringField(TEXT("row_name"), TEXT("Pistol"));
	Op->SetObjectField(TEXT("fields"), MakeWeaponFields());

	FBlueprintHelperDataTableTaskPlanPayload BuiltPayload;
	FBlueprintHelperToolError Error;
	const bool bBuilt = FBlueprintHelperDataTableTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
		Step,
		true,
		BuiltPayload,
		Error);

	TestTrue(TEXT("add_row lowers successfully"), bBuilt);
	TestEqual(TEXT("capability preserved"), BuiltPayload.Capability, FString(FBlueprintHelperDataTableTaskPlanAdapter::CapabilityDataTable));
	TestEqual(TEXT("runtime operation reports data_table capability"), BuiltPayload.RuntimeOperation, FString(FBlueprintHelperDataTableTaskPlanAdapter::RuntimeOperationDataTable));
	TestEqual(TEXT("adapter operation is add_datatable_row"), BuiltPayload.AdapterOperation, FString(FBlueprintHelperDataTableTaskPlanAdapter::AdapterOperationAddRow));
	TestTrue(TEXT("DataTable adapter supports service dry-run"), BuiltPayload.bAdapterDryRunSupported);
	TestNotNull(TEXT("payload exists"), BuiltPayload.Payload.Get());

	FString AssetPath;
	FString RowName;
	bool bDryRun = false;
	const TSharedPtr<FJsonObject>* PayloadFields = nullptr;
	TestTrue(TEXT("payload carries asset_path"), BuiltPayload.Payload->TryGetStringField(TEXT("asset_path"), AssetPath));
	TestTrue(TEXT("payload carries row_name"), BuiltPayload.Payload->TryGetStringField(TEXT("row_name"), RowName));
	TestTrue(TEXT("payload carries fields"), BuiltPayload.Payload->TryGetObjectField(TEXT("fields"), PayloadFields));
	TestTrue(TEXT("payload carries dry_run"), BuiltPayload.Payload->TryGetBoolField(TEXT("dry_run"), bDryRun));

	FString Damage;
	TestTrue(TEXT("fields preserve current string value format"), (*PayloadFields)->TryGetStringField(TEXT("Damage"), Damage));
	TestEqual(TEXT("asset_path matches target"), AssetPath, FString(TEXT("/Game/Data/DT_Weapons")));
	TestEqual(TEXT("row_name preserved"), RowName, FString(TEXT("Pistol")));
	TestEqual(TEXT("Damage field preserved"), Damage, FString(TEXT("12")));
	TestTrue(TEXT("preview dry_run is recorded"), bDryRun);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanDataTableAdapterUpdateRowTest,
	"BlueprintHelper.TaskPlan.DataTableAdapter.UpdateRow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanDataTableAdapterUpdateRowTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Step = MakeDataTableTaskPlanStep(FBlueprintHelperDataTableTaskPlanAdapter::OpUpdateRow);
	TSharedPtr<FJsonObject> Op = GetFirstDataTableOp(Step);
	TestNotNull(TEXT("data_table op exists"), Op.Get());

	Op->SetStringField(TEXT("row_name"), TEXT("Pistol"));
	Op->SetObjectField(TEXT("fields"), MakeWeaponFields());

	FBlueprintHelperDataTableTaskPlanPayload BuiltPayload;
	FBlueprintHelperToolError Error;
	const bool bBuilt = FBlueprintHelperDataTableTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
		Step,
		false,
		BuiltPayload,
		Error);

	TestTrue(TEXT("update_row lowers successfully"), bBuilt);
	TestEqual(TEXT("adapter operation is update_datatable_row"), BuiltPayload.AdapterOperation, FString(FBlueprintHelperDataTableTaskPlanAdapter::AdapterOperationUpdateRow));
	TestTrue(TEXT("DataTable adapter supports service dry-run"), BuiltPayload.bAdapterDryRunSupported);

	FString AssetPath;
	FString RowName;
	bool bDryRun = true;
	const TSharedPtr<FJsonObject>* PayloadFields = nullptr;
	TestTrue(TEXT("payload carries asset_path"), BuiltPayload.Payload->TryGetStringField(TEXT("asset_path"), AssetPath));
	TestTrue(TEXT("payload carries row_name"), BuiltPayload.Payload->TryGetStringField(TEXT("row_name"), RowName));
	TestTrue(TEXT("payload carries fields"), BuiltPayload.Payload->TryGetObjectField(TEXT("fields"), PayloadFields));
	TestTrue(TEXT("payload carries dry_run"), BuiltPayload.Payload->TryGetBoolField(TEXT("dry_run"), bDryRun));
	TestEqual(TEXT("asset_path matches target"), AssetPath, FString(TEXT("/Game/Data/DT_Weapons")));
	TestEqual(TEXT("row_name preserved"), RowName, FString(TEXT("Pistol")));
	TestEqual(TEXT("fields copied without row parsing"), PayloadFields && PayloadFields->IsValid() ? (*PayloadFields)->Values.Num() : 0, 2);
	TestFalse(TEXT("execute dry_run is recorded as false"), bDryRun);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanDataTableAdapterDeleteRowTest,
	"BlueprintHelper.TaskPlan.DataTableAdapter.DeleteRow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanDataTableAdapterDeleteRowTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Step = MakeDataTableTaskPlanStep(FBlueprintHelperDataTableTaskPlanAdapter::OpDeleteRow);
	TSharedPtr<FJsonObject> Op = GetFirstDataTableOp(Step);
	TestNotNull(TEXT("data_table op exists"), Op.Get());
	Op->SetStringField(TEXT("row_name"), TEXT("Pistol"));

	FBlueprintHelperDataTableTaskPlanPayload BuiltPayload;
	FBlueprintHelperToolError Error;
	const bool bBuilt = FBlueprintHelperDataTableTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
		Step,
		true,
		BuiltPayload,
		Error);

	TestTrue(TEXT("delete_row lowers successfully"), bBuilt);
	TestEqual(TEXT("adapter operation is delete_datatable_row"), BuiltPayload.AdapterOperation, FString(FBlueprintHelperDataTableTaskPlanAdapter::AdapterOperationDeleteRow));
	TestTrue(TEXT("DataTable adapter supports service dry-run"), BuiltPayload.bAdapterDryRunSupported);

	FString AssetPath;
	FString RowName;
	bool bDryRun = false;
	TestTrue(TEXT("payload carries asset_path"), BuiltPayload.Payload->TryGetStringField(TEXT("asset_path"), AssetPath));
	TestTrue(TEXT("payload carries row_name"), BuiltPayload.Payload->TryGetStringField(TEXT("row_name"), RowName));
	TestTrue(TEXT("payload carries dry_run"), BuiltPayload.Payload->TryGetBoolField(TEXT("dry_run"), bDryRun));
	TestEqual(TEXT("asset_path matches target"), AssetPath, FString(TEXT("/Game/Data/DT_Weapons")));
	TestEqual(TEXT("row_name preserved"), RowName, FString(TEXT("Pistol")));
	TestTrue(TEXT("preview dry_run is recorded"), bDryRun);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperDataTableServiceAddRowDryRunDoesNotCreateRowTest,
	"BlueprintHelper.TaskPlan.DataTableAdapter.ServiceDryRun.AddRowDoesNotCreateRow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperDataTableServiceAddRowDryRunDoesNotCreateRowTest::RunTest(const FString& Parameters)
{
	const FName ExistingRowName(TEXT("ExistingRow"));
	const FName NewRowName(TEXT("DryRunRow"));
	const FVector InitialValue(1.0, 2.0, 3.0);

	UPackage* Package = MakeDataTableTestPackage(TEXT("AddRowDryRun"));
	UDataTable* DataTable = MakeVectorDataTable(
		Package,
		*MakeDataTableTestObjectName(TEXT("DT_VectorRows")),
		ExistingRowName,
		InitialValue);
	TestNotNull(TEXT("test DataTable is created"), DataTable);

	FBlueprintHelperDataTableService Service;
	const FBlueprintHelperDataTableMutationResult Result = Service.AddDataTableRow(
		DataTable->GetPathName(),
		NewRowName.ToString(),
		MakeVectorFields(10.0, 20.0, 30.0),
		true);

	TestTrue(TEXT("add row dry-run validates successfully"), Result.bSuccess);
	TestTrue(TEXT("mutation result marks dry-run"), Result.bDryRun);
	TestEqual(TEXT("affected row is reported"), Result.AffectedRow, NewRowName);
	TestFalse(TEXT("dry-run add does not create row"), DataTable->FindRowUnchecked(NewRowName) != nullptr);
	TestTrue(TEXT("existing row remains present"), DataTable->FindRowUnchecked(ExistingRowName) != nullptr);
	TestEqual(TEXT("row count is unchanged"), DataTable->GetRowMap().Num(), 1);
	TestFalse(TEXT("dry-run add does not dirty the package"), Package->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperDataTableServiceUpdateRowDryRunDoesNotModifyRowTest,
	"BlueprintHelper.TaskPlan.DataTableAdapter.ServiceDryRun.UpdateRowDoesNotModifyRow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperDataTableServiceUpdateRowDryRunDoesNotModifyRowTest::RunTest(const FString& Parameters)
{
	const FName RowName(TEXT("ExistingRow"));
	const FVector InitialValue(1.0, 2.0, 3.0);

	UPackage* Package = MakeDataTableTestPackage(TEXT("UpdateRowDryRun"));
	UDataTable* DataTable = MakeVectorDataTable(
		Package,
		*MakeDataTableTestObjectName(TEXT("DT_VectorRows")),
		RowName,
		InitialValue);
	TestNotNull(TEXT("test DataTable is created"), DataTable);

	FBlueprintHelperDataTableService Service;
	const FBlueprintHelperDataTableMutationResult Result = Service.UpdateDataTableRow(
		DataTable->GetPathName(),
		RowName.ToString(),
		MakeVectorFields(10.0, 20.0, 30.0),
		true);

	TestTrue(TEXT("update row dry-run validates successfully"), Result.bSuccess);
	TestTrue(TEXT("mutation result marks dry-run"), Result.bDryRun);

	const FVector* RowAfter = reinterpret_cast<const FVector*>(DataTable->FindRowUnchecked(RowName));
	TestNotNull(TEXT("row still exists"), RowAfter);
	if (RowAfter)
	{
		TestEqual(TEXT("X remains unchanged"), RowAfter->X, InitialValue.X);
		TestEqual(TEXT("Y remains unchanged"), RowAfter->Y, InitialValue.Y);
		TestEqual(TEXT("Z remains unchanged"), RowAfter->Z, InitialValue.Z);
	}
	TestFalse(TEXT("dry-run update does not dirty the package"), Package->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperDataTableServiceDeleteRowDryRunDoesNotRemoveRowTest,
	"BlueprintHelper.TaskPlan.DataTableAdapter.ServiceDryRun.DeleteRowDoesNotRemoveRow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperDataTableServiceDeleteRowDryRunDoesNotRemoveRowTest::RunTest(const FString& Parameters)
{
	const FName RowName(TEXT("ExistingRow"));
	const FVector InitialValue(1.0, 2.0, 3.0);

	UPackage* Package = MakeDataTableTestPackage(TEXT("DeleteRowDryRun"));
	UDataTable* DataTable = MakeVectorDataTable(
		Package,
		*MakeDataTableTestObjectName(TEXT("DT_VectorRows")),
		RowName,
		InitialValue);
	TestNotNull(TEXT("test DataTable is created"), DataTable);

	FBlueprintHelperDataTableService Service;
	const FBlueprintHelperDataTableMutationResult Result = Service.DeleteDataTableRow(
		DataTable->GetPathName(),
		RowName.ToString(),
		true);

	TestTrue(TEXT("delete row dry-run validates successfully"), Result.bSuccess);
	TestTrue(TEXT("mutation result marks dry-run"), Result.bDryRun);
	TestEqual(TEXT("affected row is reported"), Result.AffectedRow, RowName);

	const FVector* RowAfter = reinterpret_cast<const FVector*>(DataTable->FindRowUnchecked(RowName));
	TestNotNull(TEXT("dry-run delete keeps row"), RowAfter);
	if (RowAfter)
	{
		TestEqual(TEXT("X remains unchanged"), RowAfter->X, InitialValue.X);
		TestEqual(TEXT("Y remains unchanged"), RowAfter->Y, InitialValue.Y);
		TestEqual(TEXT("Z remains unchanged"), RowAfter->Z, InitialValue.Z);
	}
	TestEqual(TEXT("row count is unchanged"), DataTable->GetRowMap().Num(), 1);
	TestFalse(TEXT("dry-run delete does not dirty the package"), Package->IsDirty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanDataTableAdapterRejectsOperationFieldTest,
	"BlueprintHelper.TaskPlan.DataTableAdapter.RejectsOperationField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanDataTableAdapterRejectsOperationFieldTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Step = MakeDataTableTaskPlanStep(FBlueprintHelperDataTableTaskPlanAdapter::OpAddRow);
	Step->SetStringField(TEXT("operation"), TEXT("add_datatable_row"));

	FBlueprintHelperDataTableTaskPlanPayload BuiltPayload;
	FBlueprintHelperToolError Error;
	const bool bBuilt = FBlueprintHelperDataTableTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
		Step,
		true,
		BuiltPayload,
		Error);

	TestFalse(TEXT("data_table IR rejects adapter operation field"), bBuilt);
	TestEqual(TEXT("operation field error code"), Error.Code, FString(TEXT("unsupported_data_table_operation_field")));
	TestEqual(TEXT("operation field error path"), Error.Field, FString(TEXT("task_plan.steps[0].operation")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanDataTableAdapterRejectsUpdateValuesFieldTest,
	"BlueprintHelper.TaskPlan.DataTableAdapter.RejectsUpdateValuesField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanDataTableAdapterRejectsUpdateValuesFieldTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Step = MakeDataTableTaskPlanStep(FBlueprintHelperDataTableTaskPlanAdapter::OpUpdateRow);
	TSharedPtr<FJsonObject> Op = GetFirstDataTableOp(Step);
	TestNotNull(TEXT("data_table op exists"), Op.Get());
	Op->SetStringField(TEXT("row_name"), TEXT("Pistol"));
	Op->SetObjectField(TEXT("values"), MakeWeaponFields());

	FBlueprintHelperDataTableTaskPlanPayload BuiltPayload;
	FBlueprintHelperToolError Error;
	const bool bBuilt = FBlueprintHelperDataTableTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
		Step,
		false,
		BuiltPayload,
		Error);

	TestFalse(TEXT("update_row requires current fields object instead of values"), bBuilt);
	TestEqual(TEXT("missing fields error code"), Error.Code, FString(TEXT("invalid_data_table_update_row_op")));
	TestEqual(TEXT("missing fields error path"), Error.Field, FString(TEXT("task_plan.steps[0].write.ops[0].fields")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanDataTableAdapterRejectsBatchOpsTest,
	"BlueprintHelper.TaskPlan.DataTableAdapter.RejectsBatchOps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanDataTableAdapterRejectsBatchOpsTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Step = MakeDataTableTaskPlanStep(FBlueprintHelperDataTableTaskPlanAdapter::OpUpdateRow);

	const TSharedPtr<FJsonObject>* Write = nullptr;
	TestTrue(TEXT("test step has write object"), Step->TryGetObjectField(TEXT("write"), Write));

	TArray<TSharedPtr<FJsonValue>> Ops;
	TSharedPtr<FJsonObject> FirstOp = MakeShared<FJsonObject>();
	FirstOp->SetStringField(TEXT("op"), FBlueprintHelperDataTableTaskPlanAdapter::OpUpdateRow);
	FirstOp->SetStringField(TEXT("row_name"), TEXT("Pistol"));
	FirstOp->SetObjectField(TEXT("fields"), MakeWeaponFields());
	Ops.Add(MakeShared<FJsonValueObject>(FirstOp.ToSharedRef()));

	TSharedPtr<FJsonObject> SecondOp = MakeShared<FJsonObject>();
	SecondOp->SetStringField(TEXT("op"), FBlueprintHelperDataTableTaskPlanAdapter::OpUpdateRow);
	SecondOp->SetStringField(TEXT("row_name"), TEXT("Rifle"));
	SecondOp->SetObjectField(TEXT("fields"), MakeWeaponFields());
	Ops.Add(MakeShared<FJsonValueObject>(SecondOp.ToSharedRef()));
	(*Write)->SetArrayField(TEXT("ops"), Ops);

	FBlueprintHelperDataTableTaskPlanPayload BuiltPayload;
	FBlueprintHelperToolError Error;
	const bool bBuilt = FBlueprintHelperDataTableTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
		Step,
		false,
		BuiltPayload,
		Error);

	TestFalse(TEXT("batch data_table ops are not lowered until service support exists"), bBuilt);
	TestEqual(TEXT("batch op error code"), Error.Code, FString(TEXT("unsupported_data_table_batch_ops")));
	TestEqual(TEXT("batch op error path"), Error.Field, FString(TEXT("task_plan.steps[0].write.ops")));

	return true;
}

#endif
