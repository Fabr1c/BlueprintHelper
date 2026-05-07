#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "GraphSupport/BlueprintHelperBlockIdService.h"
#include "GraphSupport/BlueprintHelperGraphResolver.h"
#include "GraphSupport/BlueprintHelperGraphSnapshotService.h"
#include "GraphSupport/BlueprintHelperOwnershipService.h"
#include "Logic/BlueprintHelperLogicJsonPathService.h"
#include "Modules/ModuleManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"
#include "Services/AssetFactory/BlueprintHelperAssetFactoryService.h"
#include "Services/BlueprintClassSettings/BlueprintHelperClassSettingsService.h"
#include "Services/BlueprintComponent/BlueprintHelperComponentService.h"
#include "Services/BlueprintHelperAgentImportService.h"
#include "Services/BlueprintHelperBlueprintStructureService.h"
#include "Services/BlueprintVariables/BlueprintHelperBlueprintVariableService.h"
#include "Services/CleanupOwnership/BlueprintHelperCleanupBlueprintHelperBlockService.h"
#include "Services/CleanupOwnership/BlueprintHelperConvertBlockToUserOwnedService.h"
#include "Services/CleanupOwnership/BlueprintHelperRollbackCleanupTransactionService.h"
#include "Services/DataAssetObjectProperty/BlueprintHelperPropertyReflectionService.h"
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
#include "TaskRuntime/TaskPlanAdapters/AssetFactory/BlueprintHelperAssetFactoryTaskPlanAdapter.h"
#include "Transactions/Transactions/BlueprintHelperTransactionJournalService.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	FString AssetFactoryTestObjectPath(const FString& AssetPath)
	{
		if (AssetPath.Contains(TEXT(".")))
		{
			return AssetPath;
		}

		const FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);
		return FString::Printf(TEXT("%s.%s"), *AssetPath, *AssetName);
	}

	struct FAssetFactoryTaskRuntimeTestServices
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
		FBlueprintHelperPropertyReflectionService PropertyReflectionService;
		FBlueprintHelperCleanupBlueprintHelperBlockService CleanupBlockService;
		FBlueprintHelperRollbackCleanupTransactionService RollbackCleanupService;
		FBlueprintHelperConvertBlockToUserOwnedService ConvertBlockService;
		FBlueprintHelperCompileAssetService CompileAssetService;
		FBlueprintHelperTaskRuntimeService TaskRuntimeService;

		FAssetFactoryTaskRuntimeTestServices()
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
			, CleanupBlockService(Resolver, JournalService)
			, RollbackCleanupService(Resolver, JournalService)
			, ConvertBlockService(Resolver, OwnershipService, JournalService)
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
				PropertyReflectionService,
				CleanupBlockService,
				RollbackCleanupService,
				ConvertBlockService,
				CompileAssetService,
				AssetBrowseService)
		{
		}
	};

	bool AssetFactoryTestAssetExists(const FString& AssetPath)
	{
		FAssetRegistryModule& AssetRegistry =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		return AssetRegistry.Get().GetAssetByObjectPath(FSoftObjectPath(AssetFactoryTestObjectPath(AssetPath))).IsValid();
	}

	TSharedPtr<FJsonObject> MakeAssetFactoryCreateAssetStep()
	{
		TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
		Step->SetStringField(TEXT("step_id"), TEXT("step_asset_factory"));
		Step->SetStringField(TEXT("capability"), FBlueprintHelperAssetFactoryTaskPlanAdapter::SupportedCapability);

		TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), TEXT("/Game/Input/IA_Interact"));
		Step->SetObjectField(TEXT("target"), Target);

		TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), FBlueprintHelperAssetFactoryTaskPlanAdapter::SupportedOp);
		Op->SetStringField(TEXT("asset_type"), TEXT("input_action"));
		Op->SetStringField(TEXT("value_type"), TEXT("bool"));
		Op->SetStringField(TEXT("collision"), TEXT("reuse_if_exists"));

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

		TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), FBlueprintHelperAssetFactoryTaskPlanAdapter::SupportedStrategy);
		Write->SetArrayField(TEXT("ops"), Ops);
		Step->SetObjectField(TEXT("write"), Write);

		return Step;
	}

	TSharedPtr<FJsonObject> MakeAssetFactoryCreateAssetTaskPlan(
		const FString& AssetPath,
		const FString& AssetType,
		const FString& ParentClass = FString())
	{
		TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), FBlueprintHelperAssetFactoryTaskPlanAdapter::SupportedOp);
		Op->SetStringField(TEXT("asset_type"), AssetType);
		Op->SetStringField(TEXT("collision"), TEXT("reuse_if_exists"));
		if (!ParentClass.IsEmpty())
		{
			Op->SetStringField(TEXT("parent_class"), ParentClass);
		}

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

		TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), FBlueprintHelperAssetFactoryTaskPlanAdapter::SupportedStrategy);
		Write->SetArrayField(TEXT("ops"), Ops);

		TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);

		TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
		Step->SetStringField(TEXT("step_id"), TEXT("step_001"));
		Step->SetStringField(TEXT("capability"), FBlueprintHelperAssetFactoryTaskPlanAdapter::SupportedCapability);
		Step->SetObjectField(TEXT("target"), Target);
		Step->SetObjectField(TEXT("write"), Write);

		TArray<TSharedPtr<FJsonValue>> Steps;
		Steps.Add(MakeShared<FJsonValueObject>(Step.ToSharedRef()));

		TArray<TSharedPtr<FJsonValue>> TargetAssets;
		TargetAssets.Add(MakeShared<FJsonValueString>(AssetPath));

		TSharedPtr<FJsonObject> ExecutionPolicy = MakeShared<FJsonObject>();
		ExecutionPolicy->SetStringField(TEXT("dry_run_mode"), TEXT("full"));
		ExecutionPolicy->SetBoolField(TEXT("should_compile"), false);
		ExecutionPolicy->SetBoolField(TEXT("should_save"), false);

		TSharedPtr<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
		TaskPlan->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.TaskPlan.v1"));
		TaskPlan->SetStringField(TEXT("task_name"), TEXT("ActorAliasFixture"));
		TaskPlan->SetStringField(TEXT("task_type"), TEXT("create_asset"));
		TaskPlan->SetArrayField(TEXT("target_assets"), TargetAssets);
		TaskPlan->SetObjectField(TEXT("execution_policy"), ExecutionPolicy);
		TaskPlan->SetArrayField(TEXT("steps"), Steps);
		return TaskPlan;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanAssetFactoryAdapterBuildsCreateAssetPayloadTest,
	"BlueprintHelper.TaskPlan.AssetFactoryAdapter.BuildsCreateAssetPayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanAssetFactoryAdapterBuildsCreateAssetPayloadTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
	const TSharedPtr<FJsonObject> Step = MakeAssetFactoryCreateAssetStep();

	TSharedPtr<FJsonObject> Payload;
	FBlueprintHelperToolError Error;
	const bool bBuilt = FBlueprintHelperAssetFactoryTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
		TaskPlan,
		Step,
		true,
		Payload,
		Error);

	TestTrue(TEXT("asset_factory create_asset step lowers"), bBuilt);
	TestNotNull(TEXT("lowered payload exists"), Payload.Get());
	if (!Payload.IsValid())
	{
		return false;
	}

	FString AssetPath;
	FString AssetType;
	FString ValueType;
	FString Collision;
	bool bDryRun = false;
	TestTrue(TEXT("payload carries asset_path"), Payload->TryGetStringField(TEXT("asset_path"), AssetPath));
	TestTrue(TEXT("payload carries asset_type"), Payload->TryGetStringField(TEXT("asset_type"), AssetType));
	TestTrue(TEXT("payload carries value_type"), Payload->TryGetStringField(TEXT("value_type"), ValueType));
	TestTrue(TEXT("payload carries collision"), Payload->TryGetStringField(TEXT("collision"), Collision));
	TestTrue(TEXT("payload carries dry_run"), Payload->TryGetBoolField(TEXT("dry_run"), bDryRun));

	TestEqual(TEXT("asset_path comes from step target"), AssetPath, FString(TEXT("/Game/Input/IA_Interact")));
	TestEqual(TEXT("asset_type comes from create op"), AssetType, FString(TEXT("input_action")));
	TestEqual(TEXT("value_type is preserved"), ValueType, FString(TEXT("bool")));
	TestEqual(TEXT("existing collision field name is preserved"), Collision, FString(TEXT("reuse_if_exists")));
	TestTrue(TEXT("preview dry_run is preserved"), bDryRun);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetFactoryServiceDryRunDoesNotCreateAssetTest,
	"BlueprintHelper.Safety.AssetFactory.DryRunDoesNotCreateAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAssetFactoryServiceDryRunDoesNotCreateAssetTest::RunTest(const FString& Parameters)
{
	const FString AssetPath = FString::Printf(
		TEXT("/Game/BlueprintHelperSafety/IA_DryRun_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));

	TestFalse(TEXT("test asset does not exist before dry-run"), AssetFactoryTestAssetExists(AssetPath));

	const FBlueprintHelperAssetFactoryService Service;
	const FBlueprintHelperAssetFactoryData Data = Service.CreateAsset(
		AssetPath,
		EBlueprintHelperAssetType::InputAction,
		TEXT(""),
		TEXT("bool"),
		EBlueprintHelperAssetCollisionPolicy::FailIfExists,
		true);

	TestFalse(TEXT("dry-run does not report a real created asset"), Data.Asset.bCreated);
	TestFalse(TEXT("dry-run target was not already present"), Data.Asset.bAlreadyExisted);
	TestFalse(TEXT("dry-run does not create an asset registry entry"), AssetFactoryTestAssetExists(AssetPath));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetFactoryServiceCreatesActorBlueprintAssetTest,
	"BlueprintHelper.AssetFactory.CreatesActorBlueprintAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAssetFactoryServiceCreatesActorBlueprintAssetTest::RunTest(const FString& Parameters)
{
	const FString AssetPath = FString::Printf(
		TEXT("/Game/BlueprintHelperSafety/BP_ActorCreate_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));

	TestFalse(TEXT("test asset does not exist before create"), AssetFactoryTestAssetExists(AssetPath));

	const FBlueprintHelperAssetFactoryService Service;
	const FBlueprintHelperAssetFactoryData Data = Service.CreateAsset(
		AssetPath,
		EBlueprintHelperAssetType::BlueprintClass,
		TEXT("Actor"),
		TEXT(""),
		EBlueprintHelperAssetCollisionPolicy::FailIfExists,
		false);

	TestTrue(TEXT("blueprint class create reports created"), Data.Asset.bCreated);
	TestTrue(TEXT("created blueprint asset is registered at requested object path"), AssetFactoryTestAssetExists(AssetPath));

	UObject* CreatedAsset = StaticLoadObject(UBlueprint::StaticClass(), nullptr, *AssetFactoryTestObjectPath(AssetPath));
	UBlueprint* CreatedBlueprint = Cast<UBlueprint>(CreatedAsset);
	TestNotNull(TEXT("created asset loads as UBlueprint"), CreatedBlueprint);
	if (CreatedBlueprint)
	{
		TestTrue(TEXT("created blueprint uses Actor parent"), CreatedBlueprint->ParentClass == AActor::StaticClass());
		ObjectTools::DeleteSingleObject(CreatedBlueprint, false);
	}

	return CreatedBlueprint != nullptr && Data.Asset.bCreated;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeAssetFactoryAcceptsActorAliasPreviewTest,
	"BlueprintHelper.TaskRuntime.AssetFactory.AcceptsActorAliasPreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskRuntimeAssetFactoryAcceptsActorAliasPreviewTest::RunTest(const FString& Parameters)
{
	const FString AssetPath = FString::Printf(
		TEXT("/Game/BlueprintHelperSafety/BP_ActorAliasDryRun_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));

	FAssetFactoryTaskRuntimeTestServices Services;
	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetObjectField(TEXT("task_plan"), MakeAssetFactoryCreateAssetTaskPlan(AssetPath, TEXT("Actor")));

	const FBlueprintHelperToolResultBase Result = Services.TaskRuntimeService.PreviewTaskPlan(Payload);
	TestTrue(TEXT("Actor alias preview succeeds"), Result.bOk);
	TestEqual(TEXT("Actor alias preview is dry-run"), Result.Status, EBlueprintHelperToolStatus::DryRun);
	TestNotNull(TEXT("Actor alias preview has runtime data"), Result.Data.Get());

	const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
	TestTrue(TEXT("runtime data contains steps"), Result.Data.IsValid() && Result.Data->TryGetArrayField(TEXT("steps"), Steps));
	TestTrue(TEXT("runtime data has one step"), Steps && Steps->Num() == 1);
	if (!Steps || Steps->Num() == 0)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> Step = (*Steps)[0]->AsObject();
	const TSharedPtr<FJsonObject>* StepResult = nullptr;
	TestTrue(TEXT("step contains ToolResultBase"), Step.IsValid() && Step->TryGetObjectField(TEXT("result"), StepResult));
	const TSharedPtr<FJsonObject>* StepData = nullptr;
	TestTrue(TEXT("step result contains AssetFactory data"), StepResult && StepResult->IsValid() && (*StepResult)->TryGetObjectField(TEXT("data"), StepData));
	const TSharedPtr<FJsonObject>* Factory = nullptr;
	TestTrue(TEXT("AssetFactory data contains factory object"), StepData && StepData->IsValid() && (*StepData)->TryGetObjectField(TEXT("factory"), Factory));

	FString AssetType;
	FString ParentClass;
	TestTrue(TEXT("factory asset_type is readable"), Factory && Factory->IsValid() && (*Factory)->TryGetStringField(TEXT("asset_type"), AssetType));
	TestTrue(TEXT("factory parent_class is readable"), Factory && Factory->IsValid() && (*Factory)->TryGetStringField(TEXT("parent_class"), ParentClass));
	TestEqual(TEXT("Actor alias normalizes to blueprint_class"), AssetType, FString(TEXT("blueprint_class")));
	TestEqual(TEXT("Actor alias supplies Actor parent class"), ParentClass, FString(TEXT("Actor")));
	TestFalse(TEXT("Actor alias dry-run does not create an asset"), AssetFactoryTestAssetExists(AssetPath));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanAssetFactoryAdapterRejectsOperationFieldTest,
	"BlueprintHelper.TaskPlan.AssetFactoryAdapter.RejectsOperationField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanAssetFactoryAdapterRejectsOperationFieldTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
	const TSharedPtr<FJsonObject> Step = MakeAssetFactoryCreateAssetStep();
	Step->SetStringField(TEXT("operation"), TEXT("create_asset"));

	TSharedPtr<FJsonObject> Payload;
	FBlueprintHelperToolError Error;
	const bool bBuilt = FBlueprintHelperAssetFactoryTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
		TaskPlan,
		Step,
		false,
		Payload,
		Error);

	TestFalse(TEXT("asset_factory IR rejects adapter operation compatibility field"), bBuilt);
	TestEqual(TEXT("operation field error code"), Error.Code, FString(TEXT("unsupported_asset_factory_operation_field")));
	TestEqual(TEXT("operation field error stage"), Error.Stage, EBlueprintHelperToolStage::ParseInput);
	TestEqual(TEXT("operation field error path"), Error.Field, FString(TEXT("task_plan.steps[0].operation")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanAssetFactoryAdapterRejectsMissingAssetTypeTest,
	"BlueprintHelper.TaskPlan.AssetFactoryAdapter.RejectsMissingAssetType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanAssetFactoryAdapterRejectsMissingAssetTypeTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
	const TSharedPtr<FJsonObject> Step = MakeAssetFactoryCreateAssetStep();

	const TSharedPtr<FJsonObject>* Write = nullptr;
	TestTrue(TEXT("test step has write object"), Step->TryGetObjectField(TEXT("write"), Write));

	const TArray<TSharedPtr<FJsonValue>>* Ops = nullptr;
	TestTrue(TEXT("test step has ops array"), (*Write)->TryGetArrayField(TEXT("ops"), Ops));

	const TSharedPtr<FJsonObject> Op = (*Ops)[0]->AsObject();
	Op->RemoveField(TEXT("asset_type"));

	TSharedPtr<FJsonObject> Payload;
	FBlueprintHelperToolError Error;
	const bool bBuilt = FBlueprintHelperAssetFactoryTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
		TaskPlan,
		Step,
		false,
		Payload,
		Error);

	TestFalse(TEXT("asset_type is required"), bBuilt);
	TestEqual(TEXT("missing asset_type error code"), Error.Code, FString(TEXT("invalid_asset_factory_create_asset_op")));
	TestEqual(TEXT("missing asset_type error path"), Error.Field, FString(TEXT("task_plan.steps[0].write.ops[0].asset_type")));

	return true;
}

#endif
