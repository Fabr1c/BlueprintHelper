#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "GraphSupport/BlueprintHelperBlockIdService.h"
#include "GraphSupport/BlueprintHelperGraphResolver.h"
#include "GraphSupport/BlueprintHelperGraphSnapshotService.h"
#include "GraphSupport/BlueprintHelperOwnershipService.h"
#include "Logic/BlueprintHelperLogicJsonPathService.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Services/AssetFactory/BlueprintHelperAssetFactoryService.h"
#include "Services/BlueprintClassSettings/BlueprintHelperClassSettingsService.h"
#include "Services/BlueprintComponent/BlueprintHelperComponentService.h"
#include "Services/BlueprintHelperAgentImportService.h"
#include "Services/BlueprintHelperBlueprintStructureService.h"
#include "Services/BlueprintVariables/BlueprintHelperBlueprintVariableService.h"
#include "Services/DataTable/BlueprintHelperDataTableService.h"
#include "Services/GraphWrite/BlueprintHelperAppendBlueprintGraphService.h"
#include "Services/GraphWrite/BlueprintHelperMergeBlueprintGraphService.h"
#include "Services/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h"
#include "Services/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.h"
#include "Services/RuntimeDiagnostics/BlueprintHelperAssetBrowseService.h"
#include "Services/RuntimeDiagnostics/BlueprintHelperCompileAssetService.h"
#include "Services/RuntimeDiagnostics/BlueprintHelperCompileService.h"
#include "Services/UMGWidget/BlueprintHelperWidgetService.h"
#include "TaskRuntime/BlueprintHelperTaskRuntimeService.h"
#include "TaskRuntime/TaskPlanAdapters/BlueprintComponent/BlueprintHelperComponentTaskPlanAdapter.h"
#include "Transactions/Transactions/BlueprintHelperTransactionJournalService.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace
{
	struct FComponentTaskRuntimeTestServices
	{
		FBlueprintHelperGraphResolver Resolver;
		FBlueprintHelperCompileService CompileService;
		FBlueprintHelperAssetBrowseService AssetBrowseService;
		FBlueprintHelperAgentImportService AgentImportService;
		FBlueprintHelperBlockIdService BlockIdService;
		FBlueprintHelperOwnershipService OwnershipService;
		FBlueprintHelperTransactionJournalService JournalService;
		FBlueprintHelperGraphSnapshotService SnapshotService;
		FBlueprintHelperLogicJsonPathService PathService;
		FBlueprintHelperAppendBlueprintGraphService AppendGraphService;
		FBlueprintHelperReplaceBlueprintGraphService ReplaceGraphService;
		FBlueprintHelperPatchBlueprintGraphService PatchGraphService;
		FBlueprintHelperMergeBlueprintGraphService MergeGraphService;
		FBlueprintHelperBlueprintStructureService StructureService;
		FBlueprintHelperBlueprintVariableService VariableService;
		FBlueprintHelperAssetFactoryService AssetFactoryService;
		FBlueprintHelperComponentService ComponentService;
		FBlueprintHelperClassSettingsService ClassSettingsService;
		FBlueprintHelperWidgetService WidgetService;
		FBlueprintHelperDataTableService DataTableService;
		FBlueprintHelperCompileAssetService CompileAssetService;
		FBlueprintHelperTaskRuntimeService TaskRuntimeService;

		FComponentTaskRuntimeTestServices()
			: CompileService(Resolver)
			, AgentImportService(Resolver, CompileService, AssetBrowseService)
			, AppendGraphService(Resolver, AgentImportService, BlockIdService, OwnershipService, JournalService)
			, ReplaceGraphService(Resolver, AgentImportService, BlockIdService, OwnershipService, JournalService, SnapshotService)
			, PatchGraphService(Resolver, PathService, JournalService)
			, MergeGraphService(Resolver, PathService, JournalService)
			, StructureService(Resolver)
			, VariableService(Resolver, StructureService)
			, ComponentService(Resolver)
			, ClassSettingsService(Resolver)
			, CompileAssetService(CompileService)
			, TaskRuntimeService(
				AppendGraphService,
				ReplaceGraphService,
				PatchGraphService,
				MergeGraphService,
				VariableService,
				StructureService,
				AssetFactoryService,
				ComponentService,
				ClassSettingsService,
				WidgetService,
				DataTableService,
				CompileAssetService,
				AssetBrowseService)
		{
		}
	};

	FString MakeComponentServiceTestObjectName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	UPackage* MakeComponentServiceTestPackage(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperComponent/%s"),
			*MakeComponentServiceTestObjectName(Prefix)));
		Package->SetDirtyFlag(false);
		return Package;
	}

	UBlueprint* MakeComponentServiceActorBlueprint(const FString& Prefix)
	{
		UPackage* Package = MakeComponentServiceTestPackage(Prefix);
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			*MakeComponentServiceTestObjectName(TEXT("BP_ComponentService")),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperComponentToolResultBaseTests"));
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

	USCS_Node* FindComponentServiceTestNode(UBlueprint* Blueprint, const FString& ComponentName)
	{
		if (!Blueprint || !Blueprint->SimpleConstructionScript)
		{
			return nullptr;
		}

		for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (Node && Node->GetVariableName().ToString() == ComponentName)
			{
				return Node;
			}
		}
		return nullptr;
	}

	FBoolProperty* FindWritableBoolProperty(UObject* Object, FString& OutPropertyName)
	{
		if (!Object)
		{
			return nullptr;
		}

		for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
		{
			FBoolProperty* BoolProperty = CastField<FBoolProperty>(*It);
			if (BoolProperty && FBlueprintHelperEditablePropertyPolicy::AllowsWrite(BoolProperty))
			{
				OutPropertyName = BoolProperty->GetName();
				return BoolProperty;
			}
		}
		return nullptr;
	}

	bool ReadBoolPropertyValue(UObject* Object, FBoolProperty* Property)
	{
		return Object && Property
			? Property->GetPropertyValue(Property->ContainerPtrToValuePtr<void>(Object))
			: false;
	}

	void WriteBoolPropertyValue(UObject* Object, FBoolProperty* Property, bool bValue)
	{
		if (Object && Property)
		{
			Property->SetPropertyValue(Property->ContainerPtrToValuePtr<void>(Object), bValue);
		}
	}

	void AssertComponentDryRunResult(
		FAutomationTestBase& Test,
		const FBlueprintHelperToolResultBase& Result,
		const FString& ExpectedOperation)
	{
		Test.TestTrue(TEXT("component dry-run succeeds"), Result.bOk);
		Test.TestEqual(TEXT("component dry-run status"), Result.Status, EBlueprintHelperToolStatus::DryRun);
		Test.TestEqual(TEXT("component dry-run operation"), Result.Operation, ExpectedOperation);
		Test.TestFalse(TEXT("component dry-run does not mark modified"), Result.bModified);
		Test.TestTrue(TEXT("component dry-run returns validation"), Result.Validation.IsSet());
		if (Result.Validation.IsSet())
		{
			Test.TestFalse(TEXT("component dry-run does not request compile"), Result.Validation->bShouldCompile);
			Test.TestFalse(TEXT("component dry-run does not request save"), Result.Validation->bShouldSave);
		}
	}

	void AssertComponentToolResultBaseEnvelope(
		FAutomationTestBase& Test,
		const FBlueprintHelperToolResultBase& Result,
		const FString& ExpectedOperation)
	{
		Test.TestEqual(TEXT("schema is ToolResultBase schema"),
			Result.Schema, FString(BlueprintHelperProtocol::ToolResultSchema));
		Test.TestEqual(TEXT("operation is preserved"), Result.Operation, ExpectedOperation);
		Test.TestEqual(TEXT("failed status is represented by ToolResultBase"),
			Result.Status, EBlueprintHelperToolStatus::Failed);
		Test.TestFalse(TEXT("failed component result is not modified"), Result.bModified);
		Test.TestTrue(TEXT("error is carried by ToolResultBase"), Result.Error.IsSet());
		Test.TestNotNull(TEXT("component data is still present under data"), Result.Data.Get());

		const TSharedRef<FJsonObject> Json = Result.ToJson();
		FString Schema;
		FString Operation;
		FString Status;
		Test.TestTrue(TEXT("json carries schema"), Json->TryGetStringField(TEXT("schema"), Schema));
		Test.TestTrue(TEXT("json carries operation"), Json->TryGetStringField(TEXT("operation"), Operation));
		Test.TestTrue(TEXT("json carries status"), Json->TryGetStringField(TEXT("status"), Status));
		Test.TestEqual(TEXT("json schema value"), Schema, FString(BlueprintHelperProtocol::ToolResultSchema));
		Test.TestEqual(TEXT("json operation value"), Operation, ExpectedOperation);
		Test.TestEqual(TEXT("json status value"), Status, FString(TEXT("failed")));
		Test.TestTrue(TEXT("json carries target"), Json->HasTypedField<EJson::Object>(TEXT("target")));
		Test.TestTrue(TEXT("json carries data"), Json->HasTypedField<EJson::Object>(TEXT("data")));
		Test.TestTrue(TEXT("json carries error"), Json->HasTypedField<EJson::Object>(TEXT("error")));
	}

	TSharedPtr<FJsonObject> MakeComponentTaskPlanStep(
		const FString& StepId,
		const FString& AssetPath,
		const FString& OpName,
		const FString& ComponentName)
	{
		TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
		Step->SetStringField(TEXT("step_id"), StepId);
		Step->SetStringField(TEXT("capability"), FBlueprintHelperComponentTaskPlanAdapter::CapabilityBlueprintComponent);

		TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);
		Step->SetObjectField(TEXT("target"), Target);

		TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), OpName);
		Op->SetStringField(TEXT("component_name"), ComponentName);
		if (OpName == FBlueprintHelperComponentTaskPlanAdapter::OpAddComponent)
		{
			Op->SetStringField(TEXT("component_class"), TEXT("StaticMeshComponent"));
			Op->SetStringField(TEXT("name_collision_policy"), TEXT("reuse_if_exists"));
		}
		else if (OpName == FBlueprintHelperComponentTaskPlanAdapter::OpSetComponentProperties)
		{
			TSharedPtr<FJsonObject> Setting = MakeShared<FJsonObject>();
			Setting->SetStringField(TEXT("property_path"), TEXT("Mobility"));
			Setting->SetStringField(TEXT("value"), TEXT("Movable"));

			TArray<TSharedPtr<FJsonValue>> Settings;
			Settings.Add(MakeShared<FJsonValueObject>(Setting.ToSharedRef()));
			Op->SetArrayField(TEXT("settings"), Settings);
		}

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

		TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), FBlueprintHelperComponentTaskPlanAdapter::StrategyComponentTree);
		Write->SetArrayField(TEXT("ops"), Ops);
		Step->SetObjectField(TEXT("write"), Write);
		return Step;
	}

	TSharedPtr<FJsonObject> MakeComponentAddThenConfigureTaskPlan(
		const FString& AssetPath,
		const FString& ComponentName)
	{
		TSharedPtr<FJsonObject> AddStep = MakeComponentTaskPlanStep(
			TEXT("step_001"),
			AssetPath,
			FBlueprintHelperComponentTaskPlanAdapter::OpAddComponent,
			ComponentName);
		TSharedPtr<FJsonObject> ConfigureStep = MakeComponentTaskPlanStep(
			TEXT("step_002"),
			AssetPath,
			FBlueprintHelperComponentTaskPlanAdapter::OpSetComponentProperties,
			ComponentName);

		TArray<TSharedPtr<FJsonValue>> DependsOn;
		DependsOn.Add(MakeShared<FJsonValueString>(TEXT("step_001")));
		ConfigureStep->SetArrayField(TEXT("depends_on"), DependsOn);

		TArray<TSharedPtr<FJsonValue>> TargetAssets;
		TargetAssets.Add(MakeShared<FJsonValueString>(AssetPath));

		TArray<TSharedPtr<FJsonValue>> Steps;
		Steps.Add(MakeShared<FJsonValueObject>(AddStep.ToSharedRef()));
		Steps.Add(MakeShared<FJsonValueObject>(ConfigureStep.ToSharedRef()));

		TSharedPtr<FJsonObject> ExecutionPolicy = MakeShared<FJsonObject>();
		ExecutionPolicy->SetStringField(TEXT("dry_run_mode"), TEXT("full"));
		ExecutionPolicy->SetBoolField(TEXT("should_compile"), false);
		ExecutionPolicy->SetBoolField(TEXT("should_save"), false);

		TSharedPtr<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
		TaskPlan->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.TaskPlan.v1"));
		TaskPlan->SetStringField(TEXT("task_name"), TEXT("ComponentPreview"));
		TaskPlan->SetStringField(TEXT("task_type"), TEXT("edit_blueprint_components"));
		TaskPlan->SetArrayField(TEXT("target_assets"), TargetAssets);
		TaskPlan->SetObjectField(TEXT("execution_policy"), ExecutionPolicy);
		TaskPlan->SetArrayField(TEXT("steps"), Steps);
		return TaskPlan;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperComponentToolResultBaseReadEnvelopeTest,
	"BlueprintHelper.Component.ToolResultBase.ReadEnvelope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperComponentToolResultBaseReadEnvelopeTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperComponentService ComponentService(Resolver);

	FBlueprintHelperReadComponentsRequest Request;
	Request.AssetPath = TEXT("/Game/BlueprintHelper/DoesNotExist/BP_Missing");

	const FBlueprintHelperToolResultBase Result = ComponentService.ReadComponents(Request);
	AssertComponentToolResultBaseEnvelope(*this, Result, TEXT("read_components"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperComponentToolResultBaseWriteEnvelopeTest,
	"BlueprintHelper.Component.ToolResultBase.WriteEnvelopes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperComponentToolResultBaseWriteEnvelopeTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperComponentService ComponentService(Resolver);
	const FString MissingAssetPath = TEXT("/Game/BlueprintHelper/DoesNotExist/BP_Missing");

	FBlueprintHelperAddComponentRequest AddRequest;
	AddRequest.AssetPath = MissingAssetPath;
	AddRequest.ComponentName = TEXT("TestComponent");
	AddRequest.ComponentClass = TEXT("StaticMeshComponent");
	AssertComponentToolResultBaseEnvelope(*this,
		ComponentService.AddComponent(AddRequest),
		TEXT("add_component"));

	FBlueprintHelperSetComponentPropertiesRequest SetRequest;
	SetRequest.AssetPath = MissingAssetPath;
	SetRequest.ComponentName = TEXT("TestComponent");
	SetRequest.Mode = EBlueprintHelperComponentPropertyMode::Single;
	FBlueprintHelperComponentPropertySetting Setting;
	Setting.PropertyPath = TEXT("Mobility");
	Setting.Value = MakeShared<FJsonValueString>(TEXT("Movable"));
	SetRequest.Settings.Add(MoveTemp(Setting));
	AssertComponentToolResultBaseEnvelope(*this,
		ComponentService.SetComponentProperty(SetRequest),
		TEXT("set_component_property"));

	FBlueprintHelperRemoveComponentRequest RemoveRequest;
	RemoveRequest.AssetPath = MissingAssetPath;
	RemoveRequest.ComponentName = TEXT("TestComponent");
	AssertComponentToolResultBaseEnvelope(*this,
		ComponentService.RemoveComponent(RemoveRequest),
		TEXT("remove_component"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperComponentAddDryRunDoesNotCreateTest,
	"BlueprintHelper.Component.DryRun.AddComponentDoesNotCreate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperComponentAddDryRunDoesNotCreateTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeComponentServiceActorBlueprint(TEXT("AddDryRun"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	TestNotNull(TEXT("test Blueprint has SimpleConstructionScript"), Blueprint ? Blueprint->SimpleConstructionScript.Get() : nullptr);
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		return false;
	}

	const int32 BeforeNodeCount = Blueprint->SimpleConstructionScript->GetAllNodes().Num();
	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperComponentService ComponentService(Resolver);

	FBlueprintHelperAddComponentRequest Request;
	Request.AssetPath = Blueprint->GetPathName();
	Request.ComponentName = TEXT("DryRunActorComponent");
	Request.ComponentClass = TEXT("SceneComponent");
	Request.bDryRun = true;

	const FBlueprintHelperToolResultBase Result = ComponentService.AddComponent(Request);

	AssertComponentDryRunResult(*this, Result, TEXT("add_component"));
	TestNull(TEXT("dry-run add does not create component node"),
		FindComponentServiceTestNode(Blueprint, TEXT("DryRunActorComponent")));
	TestEqual(TEXT("dry-run add leaves SCS node count unchanged"),
		Blueprint->SimpleConstructionScript->GetAllNodes().Num(),
		BeforeNodeCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperComponentSetPropertiesDryRunDoesNotWriteTest,
	"BlueprintHelper.Component.DryRun.SetComponentPropertiesDoesNotWrite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperComponentSetPropertiesDryRunDoesNotWriteTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeComponentServiceActorBlueprint(TEXT("SetPropertiesDryRun"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperComponentService ComponentService(Resolver);

	FBlueprintHelperAddComponentRequest AddRequest;
	AddRequest.AssetPath = Blueprint->GetPathName();
	AddRequest.ComponentName = TEXT("DryRunEditableComponent");
	AddRequest.ComponentClass = TEXT("SceneComponent");
	const FBlueprintHelperToolResultBase AddResult = ComponentService.AddComponent(AddRequest);
	TestTrue(TEXT("component is added before property dry-run"), AddResult.bOk);
	TestEqual(TEXT("component add applies change"), AddResult.Status, EBlueprintHelperToolStatus::Applied);

	USCS_Node* Node = FindComponentServiceTestNode(Blueprint, TEXT("DryRunEditableComponent"));
	TestNotNull(TEXT("component node exists before property dry-run"), Node);
	TestNotNull(TEXT("component template exists before property dry-run"), Node ? Node->ComponentTemplate.Get() : nullptr);
	if (!Node || !Node->ComponentTemplate)
	{
		return false;
	}

	FString PropertyName;
	FBoolProperty* BoolProperty = FindWritableBoolProperty(Node->ComponentTemplate, PropertyName);
	TestNotNull(TEXT("component template has a writable bool property"), BoolProperty);
	if (!BoolProperty)
	{
		return false;
	}

	WriteBoolPropertyValue(Node->ComponentTemplate, BoolProperty, true);
	TestTrue(TEXT("test bool property starts true"),
		ReadBoolPropertyValue(Node->ComponentTemplate, BoolProperty));

	FBlueprintHelperSetComponentPropertiesRequest SetRequest;
	SetRequest.AssetPath = Blueprint->GetPathName();
	SetRequest.ComponentName = TEXT("DryRunEditableComponent");
	SetRequest.bDryRun = true;

	FBlueprintHelperComponentPropertySetting Setting;
	Setting.PropertyPath = PropertyName;
	Setting.Value = MakeShared<FJsonValueBoolean>(false);
	SetRequest.Settings.Add(MoveTemp(Setting));

	const FBlueprintHelperToolResultBase SetResult = ComponentService.SetComponentProperties(SetRequest);

	AssertComponentDryRunResult(*this, SetResult, TEXT("set_component_properties"));
	TestTrue(TEXT("dry-run property write validates property but leaves value unchanged"),
		ReadBoolPropertyValue(Node->ComponentTemplate, BoolProperty));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperComponentRemoveDryRunDoesNotDeleteTest,
	"BlueprintHelper.Component.DryRun.RemoveComponentDoesNotDelete",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperComponentRemoveDryRunDoesNotDeleteTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeComponentServiceActorBlueprint(TEXT("RemoveDryRun"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintHelperGraphResolver Resolver;
	FBlueprintHelperComponentService ComponentService(Resolver);

	FBlueprintHelperAddComponentRequest AddRequest;
	AddRequest.AssetPath = Blueprint->GetPathName();
	AddRequest.ComponentName = TEXT("DryRunRemoveComponent");
	AddRequest.ComponentClass = TEXT("SceneComponent");
	const FBlueprintHelperToolResultBase AddResult = ComponentService.AddComponent(AddRequest);
	TestTrue(TEXT("component is added before remove dry-run"), AddResult.bOk);
	TestEqual(TEXT("component add applies change"), AddResult.Status, EBlueprintHelperToolStatus::Applied);
	TestNotNull(TEXT("component node exists before remove dry-run"),
		FindComponentServiceTestNode(Blueprint, TEXT("DryRunRemoveComponent")));

	FBlueprintHelperRemoveComponentRequest RemoveRequest;
	RemoveRequest.AssetPath = Blueprint->GetPathName();
	RemoveRequest.ComponentName = TEXT("DryRunRemoveComponent");
	RemoveRequest.bDryRun = true;

	const FBlueprintHelperToolResultBase RemoveResult = ComponentService.RemoveComponent(RemoveRequest);

	AssertComponentDryRunResult(*this, RemoveResult, TEXT("remove_component"));
	TestNotNull(TEXT("dry-run remove leaves component node in SCS"),
		FindComponentServiceTestNode(Blueprint, TEXT("DryRunRemoveComponent")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperComponentTaskRuntimeDryRunPlannedConfigureTest,
	"BlueprintHelper.Component.TaskRuntimeDryRun.PlannedAddThenConfigure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperComponentTaskRuntimeDryRunPlannedConfigureTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeComponentServiceActorBlueprint(TEXT("TaskRuntimeDryRun"));
	TestNotNull(TEXT("target Blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	const FString ComponentName = TEXT("DryRunPlannedMesh");
	TestNull(TEXT("component does not exist before preview"),
		FindComponentServiceTestNode(Blueprint, ComponentName));

	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetObjectField(TEXT("task_plan"), MakeComponentAddThenConfigureTaskPlan(
		Blueprint->GetPathName(),
		ComponentName));

	FComponentTaskRuntimeTestServices Services;
	const FBlueprintHelperToolResultBase Result = Services.TaskRuntimeService.PreviewTaskPlan(Payload);

	TestTrue(TEXT("component add+configure preview succeeds"), Result.bOk);
	TestEqual(TEXT("preview returns dry-run status"), Result.Status, EBlueprintHelperToolStatus::DryRun);
	TestFalse(TEXT("preview does not modify assets"), Result.bModified);
	TestNull(TEXT("dry-run does not create the planned component"),
		FindComponentServiceTestNode(Blueprint, ComponentName));
	TestTrue(TEXT("preview carries runtime data"), Result.Data.IsValid());
	if (!Result.Data.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* RuntimeSteps = nullptr;
	TestTrue(TEXT("preview runtime data contains steps"),
		Result.Data->TryGetArrayField(TEXT("steps"), RuntimeSteps));
	TestEqual(TEXT("both component steps are represented"),
		RuntimeSteps ? RuntimeSteps->Num() : 0,
		2);
	if (!RuntimeSteps || RuntimeSteps->Num() != 2)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> ConfigureStep = (*RuntimeSteps)[1]->AsObject();
	TestTrue(TEXT("configure step exists"), ConfigureStep.IsValid());
	if (!ConfigureStep.IsValid())
	{
		return false;
	}

	FString StepStatus;
	FString AdapterOperation;
	TestTrue(TEXT("configure step has status"),
		ConfigureStep->TryGetStringField(TEXT("status"), StepStatus));
	TestTrue(TEXT("configure step keeps adapter operation"),
		ConfigureStep->TryGetStringField(TEXT("adapter_operation"), AdapterOperation));
	TestEqual(TEXT("configure step is dry-run"),
		StepStatus,
		FString(TEXT("dry_run")));
	TestEqual(TEXT("configure step adapter is set_component_properties"),
		AdapterOperation,
		FString(FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationSetComponentProperties));

	return true;
}

#endif
