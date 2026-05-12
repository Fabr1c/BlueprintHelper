#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Blueprint.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperBlockIdService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphSnapshotService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonPathService.h"
#include "Modules/ModuleManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"
#include "Systems/ToolClusters/AssetFactory/BlueprintHelperAssetFactoryService.h"
#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassSettingsService.h"
#include "Systems/ToolClusters/BlueprintComponent/BlueprintHelperComponentService.h"
#include "Shared/Services/BlueprintHelperAgentImportService.h"
#include "Shared/Services/BlueprintHelperBlueprintStructureService.h"
#include "Systems/ToolClusters/BlueprintVariables/BlueprintHelperBlueprintVariableService.h"
#include "Systems/ToolClusters/CleanupOwnership/BlueprintHelperCleanupBlueprintHelperBlockService.h"
#include "Systems/ToolClusters/CleanupOwnership/BlueprintHelperConvertBlockToUserOwnedService.h"
#include "Systems/ToolClusters/CleanupOwnership/BlueprintHelperRollbackCleanupTransactionService.h"
#include "Systems/ToolClusters/ObjectProperty/BlueprintHelperPropertyReflectionService.h"
#include "Systems/ToolClusters/DataTable/BlueprintHelperDataTableService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperAppendBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.h"
#include "Systems/Debug/BlueprintHelperAssetBrowseService.h"
#include "Systems/Debug/BlueprintHelperCompileAssetService.h"
#include "Systems/Debug/BlueprintHelperCompileService.h"
#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetService.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/AssetFactory/BlueprintHelperAssetFactoryTaskPlanAdapter.h"
#include "Systems/Transactions/BlueprintHelperTransactionJournalService.h"
#include "Engine/UserDefinedStruct.h"
#include "Runtime/Launch/Resources/Version.h"
#include "UObject/Interface.h"
#include "UObject/SoftObjectPath.h"
#include "WidgetBlueprint.h"
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
#include "StructUtils/UserDefinedStruct.h"
#endif

class FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils
{
public:
	static FString AssetFactoryTestObjectPath(const FString& AssetPath)
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

	static bool AssetFactoryTestAssetExists(const FString& AssetPath)
	{
		FAssetRegistryModule& AssetRegistry =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		return AssetRegistry.Get().GetAssetByObjectPath(FSoftObjectPath(AssetFactoryTestObjectPath(AssetPath))).IsValid();
	}

	static bool AssetFactoryHasPropertyWithFriendlyName(const UUserDefinedStruct* Struct, const FString& FriendlyName)
	{
		if (!Struct)
		{
			return false;
		}

		for (TFieldIterator<const FProperty> It(Struct); It; ++It)
		{
			if (Struct->GetAuthoredNameForField(*It) == FriendlyName)
			{
				return true;
			}
		}

		return false;
	}

	static TArray<FBlueprintHelperAssetFactoryFieldSpec> MakeDamageAmmoFields()
	{
		TArray<FBlueprintHelperAssetFactoryFieldSpec> Fields;
		Fields.Add(FBlueprintHelperAssetFactoryFieldSpec(TEXT("Damage"), TEXT("int")));
		Fields.Add(FBlueprintHelperAssetFactoryFieldSpec(TEXT("Ammo"), TEXT("int")));
		return Fields;
	}

	static TSharedPtr<FJsonObject> MakeAssetFactoryCreateAssetStep()
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
		Op->SetStringField(TEXT("row_struct"), TEXT("/Game/BlueprintHelperSafety/ST_Row.ST_Row"));
		Op->SetStringField(TEXT("data_asset_class"), TEXT("/Script/Engine.DataAsset"));
		Op->SetStringField(TEXT("collision"), TEXT("reuse_if_exists"));

		TSharedPtr<FJsonObject> DamageField = MakeShared<FJsonObject>();
		DamageField->SetStringField(TEXT("name"), TEXT("Damage"));
		DamageField->SetStringField(TEXT("type"), TEXT("int"));

		TSharedPtr<FJsonObject> AmmoField = MakeShared<FJsonObject>();
		AmmoField->SetStringField(TEXT("name"), TEXT("Ammo"));
		AmmoField->SetStringField(TEXT("type"), TEXT("int"));

		TArray<TSharedPtr<FJsonValue>> Fields;
		Fields.Add(MakeShared<FJsonValueObject>(DamageField.ToSharedRef()));
		Fields.Add(MakeShared<FJsonValueObject>(AmmoField.ToSharedRef()));
		Op->SetArrayField(TEXT("fields"), Fields);

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

		TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), FBlueprintHelperAssetFactoryTaskPlanAdapter::SupportedStrategy);
		Write->SetArrayField(TEXT("ops"), Ops);
		Step->SetObjectField(TEXT("write"), Write);

		return Step;
	}

	static TSharedPtr<FJsonObject> MakeAssetFactoryCreateAssetStep(
		const FString& StepId,
		const FString& AssetPath,
		const FString& AssetType,
		const FString& ParentClass,
		const FString& ValueType,
		const FString& RowStruct,
		const FString& DataAssetClass,
		const TArray<FString>& DependsOn)
	{
		TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), FBlueprintHelperAssetFactoryTaskPlanAdapter::SupportedOp);
		Op->SetStringField(TEXT("asset_type"), AssetType);
		Op->SetStringField(TEXT("collision"), TEXT("reuse_if_exists"));
		if (!ParentClass.IsEmpty())
		{
			Op->SetStringField(TEXT("parent_class"), ParentClass);
		}
		if (!ValueType.IsEmpty())
		{
			Op->SetStringField(TEXT("value_type"), ValueType);
		}
		if (!RowStruct.IsEmpty())
		{
			Op->SetStringField(TEXT("row_struct"), RowStruct);
		}
		if (!DataAssetClass.IsEmpty())
		{
			Op->SetStringField(TEXT("data_asset_class"), DataAssetClass);
		}

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

		TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), FBlueprintHelperAssetFactoryTaskPlanAdapter::SupportedStrategy);
		Write->SetArrayField(TEXT("ops"), Ops);

		TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);

		TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
		Step->SetStringField(TEXT("step_id"), StepId);
		Step->SetStringField(TEXT("capability"), FBlueprintHelperAssetFactoryTaskPlanAdapter::SupportedCapability);
		Step->SetObjectField(TEXT("target"), Target);
		Step->SetObjectField(TEXT("write"), Write);
		if (DependsOn.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> DependsOnValues;
			for (const FString& DependsOnStepId : DependsOn)
			{
				DependsOnValues.Add(MakeShared<FJsonValueString>(DependsOnStepId));
			}
			Step->SetArrayField(TEXT("depends_on"), DependsOnValues);
		}

		return Step;
	}

	static TSharedPtr<FJsonObject> MakeAssetFactoryTaskPlanWithSteps(
		const FString& TaskName,
		const FString& TaskType,
		const TArray<FString>& TargetAssetPaths,
		const TArray<TSharedPtr<FJsonObject>>& StepObjects)
	{
		TArray<TSharedPtr<FJsonValue>> Steps;
		for (const TSharedPtr<FJsonObject>& StepObject : StepObjects)
		{
			Steps.Add(MakeShared<FJsonValueObject>(StepObject.ToSharedRef()));
		}

		TArray<TSharedPtr<FJsonValue>> TargetAssets;
		for (const FString& TargetAssetPath : TargetAssetPaths)
		{
			TargetAssets.Add(MakeShared<FJsonValueString>(TargetAssetPath));
		}

		TSharedPtr<FJsonObject> ExecutionPolicy = MakeShared<FJsonObject>();
		ExecutionPolicy->SetStringField(TEXT("dry_run_mode"), TEXT("full"));
		ExecutionPolicy->SetBoolField(TEXT("should_compile"), false);
		ExecutionPolicy->SetBoolField(TEXT("should_save"), false);

		TSharedPtr<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
		TaskPlan->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.TaskPlan.v1"));
		TaskPlan->SetStringField(TEXT("task_name"), TaskName);
		TaskPlan->SetStringField(TEXT("task_type"), TaskType);
		TaskPlan->SetArrayField(TEXT("target_assets"), TargetAssets);
		TaskPlan->SetObjectField(TEXT("execution_policy"), ExecutionPolicy);
		TaskPlan->SetArrayField(TEXT("steps"), Steps);
		return TaskPlan;
	}

	static TSharedPtr<FJsonObject> MakeAssetFactoryCreateAssetTaskPlan(
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

};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanAssetFactoryAdapterBuildsCreateAssetPayloadTest,
	"BlueprintHelper.TaskPlan.AssetFactoryAdapter.BuildsCreateAssetPayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanAssetFactoryAdapterBuildsCreateAssetPayloadTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
	const TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::MakeAssetFactoryCreateAssetStep();

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
	FString RowStruct;
	FString DataAssetClass;
	FString Collision;
	bool bDryRun = false;
	TestTrue(TEXT("payload carries asset_path"), Payload->TryGetStringField(TEXT("asset_path"), AssetPath));
	TestTrue(TEXT("payload carries asset_type"), Payload->TryGetStringField(TEXT("asset_type"), AssetType));
	TestTrue(TEXT("payload carries value_type"), Payload->TryGetStringField(TEXT("value_type"), ValueType));
	TestTrue(TEXT("payload carries row_struct"), Payload->TryGetStringField(TEXT("row_struct"), RowStruct));
	TestTrue(TEXT("payload carries data_asset_class"), Payload->TryGetStringField(TEXT("data_asset_class"), DataAssetClass));
	TestTrue(TEXT("payload carries collision"), Payload->TryGetStringField(TEXT("collision"), Collision));
	TestTrue(TEXT("payload carries dry_run"), Payload->TryGetBoolField(TEXT("dry_run"), bDryRun));
	const TArray<TSharedPtr<FJsonValue>>* Fields = nullptr;
	TestTrue(TEXT("payload carries fields"), Payload->TryGetArrayField(TEXT("fields"), Fields));

	TestEqual(TEXT("asset_path comes from step target"), AssetPath, FString(TEXT("/Game/Input/IA_Interact")));
	TestEqual(TEXT("asset_type comes from create op"), AssetType, FString(TEXT("input_action")));
	TestEqual(TEXT("value_type is preserved"), ValueType, FString(TEXT("bool")));
	TestEqual(TEXT("row_struct is preserved"), RowStruct, FString(TEXT("/Game/BlueprintHelperSafety/ST_Row.ST_Row")));
	TestEqual(TEXT("data_asset_class is preserved"), DataAssetClass, FString(TEXT("/Script/Engine.DataAsset")));
	TestEqual(TEXT("existing collision field name is preserved"), Collision, FString(TEXT("reuse_if_exists")));
	TestTrue(TEXT("preview dry_run is preserved"), bDryRun);
	TestTrue(TEXT("fields array has two entries"), Fields && Fields->Num() == 2);
	if (Fields && Fields->Num() == 2)
	{
		FString DamageName;
		FString AmmoType;
		TestTrue(TEXT("first field is an object"), (*Fields)[0].IsValid() && (*Fields)[0]->AsObject().IsValid());
		TestTrue(TEXT("second field is an object"), (*Fields)[1].IsValid() && (*Fields)[1]->AsObject().IsValid());
		TestTrue(TEXT("first field name is preserved"), (*Fields)[0]->AsObject()->TryGetStringField(TEXT("name"), DamageName));
		TestTrue(TEXT("second field type is preserved"), (*Fields)[1]->AsObject()->TryGetStringField(TEXT("type"), AmmoType));
		TestEqual(TEXT("first field name value"), DamageName, FString(TEXT("Damage")));
		TestEqual(TEXT("second field type value"), AmmoType, FString(TEXT("int")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetFactoryServiceNormalizesDataTableAndWidgetAliasesTest,
	"BlueprintHelper.AssetFactory.NormalizesDataTableAndWidgetAliases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAssetFactoryServiceNormalizesDataTableAndWidgetAliasesTest::RunTest(const FString& Parameters)
{
	EBlueprintHelperAssetType AssetType = EBlueprintHelperAssetType::Unknown;
	FString ParentClass;

	TestTrue(TEXT("data_table alias normalizes"), FBlueprintHelperAssetFactoryService::TryNormalizeAssetTypeAndParent(TEXT("data_table"), ParentClass, AssetType));
	TestEqual(TEXT("data_table maps to DataTable"), AssetType, EBlueprintHelperAssetType::DataTable);

	AssetType = EBlueprintHelperAssetType::Unknown;
	TestTrue(TEXT("datatable alias normalizes"), FBlueprintHelperAssetFactoryService::TryNormalizeAssetTypeAndParent(TEXT("datatable"), ParentClass, AssetType));
	TestEqual(TEXT("datatable maps to DataTable"), AssetType, EBlueprintHelperAssetType::DataTable);

	AssetType = EBlueprintHelperAssetType::Unknown;
	TestTrue(TEXT("widget alias normalizes"), FBlueprintHelperAssetFactoryService::TryNormalizeAssetTypeAndParent(TEXT("widget"), ParentClass, AssetType));
	TestEqual(TEXT("widget maps to WidgetBlueprint"), AssetType, EBlueprintHelperAssetType::WidgetBlueprint);

	AssetType = EBlueprintHelperAssetType::Unknown;
	TestTrue(TEXT("widget_blueprint alias normalizes"), FBlueprintHelperAssetFactoryService::TryNormalizeAssetTypeAndParent(TEXT("widget_blueprint"), ParentClass, AssetType));
	TestEqual(TEXT("widget_blueprint maps to WidgetBlueprint"), AssetType, EBlueprintHelperAssetType::WidgetBlueprint);

	AssetType = EBlueprintHelperAssetType::Unknown;
	TestTrue(TEXT("widgetblueprint alias normalizes"), FBlueprintHelperAssetFactoryService::TryNormalizeAssetTypeAndParent(TEXT("widgetblueprint"), ParentClass, AssetType));
	TestEqual(TEXT("widgetblueprint maps to WidgetBlueprint"), AssetType, EBlueprintHelperAssetType::WidgetBlueprint);

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

	TestFalse(TEXT("test asset does not exist before dry-run"), FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryTestAssetExists(AssetPath));

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
	TestFalse(TEXT("dry-run does not create an asset registry entry"), FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryTestAssetExists(AssetPath));

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

	TestFalse(TEXT("test asset does not exist before create"), FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryTestAssetExists(AssetPath));

	const FBlueprintHelperAssetFactoryService Service;
	const FBlueprintHelperAssetFactoryData Data = Service.CreateAsset(
		AssetPath,
		EBlueprintHelperAssetType::BlueprintClass,
		TEXT("Actor"),
		TEXT(""),
		EBlueprintHelperAssetCollisionPolicy::FailIfExists,
		false);

	TestTrue(TEXT("blueprint class create reports created"), Data.Asset.bCreated);
	TestTrue(TEXT("created blueprint asset is registered at requested object path"), FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryTestAssetExists(AssetPath));

	UObject* CreatedAsset = StaticLoadObject(UBlueprint::StaticClass(), nullptr, *FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryTestObjectPath(AssetPath));
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
	FBlueprintHelperAssetFactoryServiceCreatesBlueprintInterfaceAssetTest,
	"BlueprintHelper.AssetFactory.CreatesBlueprintInterfaceAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAssetFactoryServiceCreatesBlueprintInterfaceAssetTest::RunTest(const FString& Parameters)
{
	const FString AssetPath = FString::Printf(
		TEXT("/Game/BlueprintHelperSafety/BPI_Create_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));

	TestFalse(TEXT("test asset does not exist before create"), FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryTestAssetExists(AssetPath));

	const FBlueprintHelperAssetFactoryService Service;
	const FBlueprintHelperAssetFactoryData Data = Service.CreateAsset(
		AssetPath,
		EBlueprintHelperAssetType::BlueprintInterface,
		TEXT(""),
		TEXT(""),
		EBlueprintHelperAssetCollisionPolicy::FailIfExists,
		false);

	TestTrue(TEXT("blueprint interface create reports created"), Data.Asset.bCreated);
	TestTrue(TEXT("blueprint interface asset is registered at requested object path"), FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryTestAssetExists(AssetPath));

	UObject* CreatedAsset = StaticLoadObject(UBlueprint::StaticClass(), nullptr, *FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryTestObjectPath(AssetPath));
	UBlueprint* CreatedBlueprint = Cast<UBlueprint>(CreatedAsset);
	TestNotNull(TEXT("created interface asset loads as UBlueprint"), CreatedBlueprint);
	if (CreatedBlueprint)
	{
		TestEqual(TEXT("created blueprint uses interface blueprint type"), CreatedBlueprint->BlueprintType, BPTYPE_Interface);
		TestTrue(TEXT("created blueprint interface uses UInterface parent"), CreatedBlueprint->ParentClass == UInterface::StaticClass());
		TestFalse(TEXT("created blueprint interface is not an Actor blueprint"), CreatedBlueprint->ParentClass == AActor::StaticClass());
		ObjectTools::DeleteSingleObject(CreatedBlueprint, false);
	}

	return CreatedBlueprint != nullptr && Data.Asset.bCreated;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetFactoryServiceCreatesUserDefinedStructWithFieldsTest,
	"BlueprintHelper.AssetFactory.CreatesUserDefinedStructWithFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAssetFactoryServiceCreatesUserDefinedStructWithFieldsTest::RunTest(const FString& Parameters)
{
	const FString AssetPath = FString::Printf(
		TEXT("/Game/BlueprintHelperSafety/ST_DamageAmmo_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));

	const FBlueprintHelperAssetFactoryService Service;
	const FBlueprintHelperAssetFactoryData Data = Service.CreateAsset(
		AssetPath,
		EBlueprintHelperAssetType::Structure,
		TEXT(""),
		TEXT(""),
		TEXT(""),
		TEXT(""),
		FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::MakeDamageAmmoFields(),
		EBlueprintHelperAssetCollisionPolicy::FailIfExists,
		false);

	TestTrue(TEXT("structure create reports created"), Data.Asset.bCreated);
	TestTrue(TEXT("structure asset is registered"), FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryTestAssetExists(AssetPath));

	UObject* CreatedAsset = StaticLoadObject(UUserDefinedStruct::StaticClass(), nullptr, *FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryTestObjectPath(AssetPath));
	UUserDefinedStruct* CreatedStruct = Cast<UUserDefinedStruct>(CreatedAsset);
	TestNotNull(TEXT("created asset loads as UUserDefinedStruct"), CreatedStruct);
	if (CreatedStruct)
	{
		TestTrue(TEXT("Damage field exists"), FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryHasPropertyWithFriendlyName(CreatedStruct, TEXT("Damage")));
		TestTrue(TEXT("Ammo field exists"), FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryHasPropertyWithFriendlyName(CreatedStruct, TEXT("Ammo")));
		ObjectTools::DeleteSingleObject(CreatedStruct, false);
	}

	return CreatedStruct != nullptr && Data.Asset.bCreated;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetFactoryServiceCreatesDataTableWithRowStructTest,
	"BlueprintHelper.AssetFactory.CreatesDataTableWithRowStruct",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAssetFactoryServiceCreatesDataTableWithRowStructTest::RunTest(const FString& Parameters)
{
	const FString GuidText = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString StructPath = FString::Printf(TEXT("/Game/BlueprintHelperSafety/ST_TableRow_%s"), *GuidText);
	const FString TablePath = FString::Printf(TEXT("/Game/BlueprintHelperSafety/DT_Table_%s"), *GuidText);

	const FBlueprintHelperAssetFactoryService Service;
	const FBlueprintHelperAssetFactoryData StructData = Service.CreateAsset(
		StructPath,
		EBlueprintHelperAssetType::Structure,
		TEXT(""),
		TEXT(""),
		TEXT(""),
		TEXT(""),
		FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::MakeDamageAmmoFields(),
		EBlueprintHelperAssetCollisionPolicy::FailIfExists,
		false);

	UUserDefinedStruct* RowStruct = Cast<UUserDefinedStruct>(
		StaticLoadObject(UUserDefinedStruct::StaticClass(), nullptr, *FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryTestObjectPath(StructPath)));
	TestTrue(TEXT("row struct fixture was created"), StructData.Asset.bCreated);
	TestNotNull(TEXT("row struct fixture loads"), RowStruct);
	if (!RowStruct)
	{
		return false;
	}

	const FBlueprintHelperAssetFactoryData TableData = Service.CreateAsset(
		TablePath,
		EBlueprintHelperAssetType::DataTable,
		TEXT(""),
		TEXT(""),
		RowStruct->GetPathName(),
		TEXT(""),
		TArray<FBlueprintHelperAssetFactoryFieldSpec>(),
		EBlueprintHelperAssetCollisionPolicy::FailIfExists,
		false);

	TestTrue(TEXT("data table create reports created"), TableData.Asset.bCreated);
	TestTrue(TEXT("data table asset is registered"), FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryTestAssetExists(TablePath));
	TestEqual(TEXT("factory records row_struct"), TableData.Factory.RowStruct, RowStruct->GetPathName());

	UObject* CreatedAsset = StaticLoadObject(UDataTable::StaticClass(), nullptr, *FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryTestObjectPath(TablePath));
	UDataTable* CreatedTable = Cast<UDataTable>(CreatedAsset);
	TestNotNull(TEXT("created asset loads as UDataTable"), CreatedTable);
	if (CreatedTable)
	{
		TestTrue(TEXT("data table references requested RowStruct"), CreatedTable->GetRowStruct() == RowStruct);
		ObjectTools::DeleteSingleObject(CreatedTable, false);
	}
	ObjectTools::DeleteSingleObject(RowStruct, false);

	return CreatedTable != nullptr && TableData.Asset.bCreated;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetFactoryServiceCreatesDataAssetFromBlueprintClassPathTest,
	"BlueprintHelper.AssetFactory.CreatesDataAssetFromBlueprintDataAssetClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAssetFactoryServiceCreatesDataAssetFromBlueprintClassPathTest::RunTest(const FString& Parameters)
{
	const FString TestSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString ClassAssetPath = FString::Printf(
		TEXT("/Game/BlueprintHelperSafety/BP_DataAssetClass_%s"),
		*TestSuffix);
	const FString DataAssetPath = FString::Printf(
		TEXT("/Game/BlueprintHelperSafety/DA_FromBlueprintClass_%s"),
		*TestSuffix);

	const FBlueprintHelperAssetFactoryService Service;
	const FBlueprintHelperAssetFactoryData ClassData = Service.CreateAsset(
		ClassAssetPath,
		EBlueprintHelperAssetType::BlueprintClass,
		TEXT("PrimaryDataAsset"),
		TEXT(""),
		TEXT(""),
		TEXT(""),
		TArray<FBlueprintHelperAssetFactoryFieldSpec>(),
		EBlueprintHelperAssetCollisionPolicy::FailIfExists,
		false);

	UBlueprint* DataAssetBlueprint = Cast<UBlueprint>(StaticLoadObject(
		UBlueprint::StaticClass(),
		nullptr,
		*FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryTestObjectPath(ClassAssetPath)));
	TestTrue(TEXT("primary data asset blueprint class fixture was created"), ClassData.Asset.bCreated);
	TestNotNull(TEXT("primary data asset blueprint class fixture loads"), DataAssetBlueprint);
	if (!DataAssetBlueprint)
	{
		return false;
	}

	FKismetEditorUtilities::CompileBlueprint(DataAssetBlueprint);
	UClass* GeneratedDataAssetClass = DataAssetBlueprint->GeneratedClass.Get();
	TestNotNull(TEXT("data asset blueprint generated class exists"), GeneratedDataAssetClass);

	const FBlueprintHelperAssetFactoryData Data = Service.CreateAsset(
		DataAssetPath,
		EBlueprintHelperAssetType::DataAsset,
		TEXT(""),
		TEXT(""),
		TEXT(""),
		ClassAssetPath,
		TArray<FBlueprintHelperAssetFactoryFieldSpec>(),
		EBlueprintHelperAssetCollisionPolicy::FailIfExists,
		false);

	UObject* CreatedAsset = StaticLoadObject(
		UDataAsset::StaticClass(),
		nullptr,
		*FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryTestObjectPath(DataAssetPath));
	UDataAsset* CreatedDataAsset = Cast<UDataAsset>(CreatedAsset);
	TestTrue(TEXT("data asset create reports created from blueprint class asset path"), Data.Asset.bCreated);
	TestNotNull(TEXT("created asset loads as UDataAsset"), CreatedDataAsset);
	const bool bUsesGeneratedClass = CreatedDataAsset && GeneratedDataAssetClass && CreatedDataAsset->IsA(GeneratedDataAssetClass);
	if (CreatedDataAsset && GeneratedDataAssetClass)
	{
		TestTrue(TEXT("created data asset uses the blueprint generated class"), bUsesGeneratedClass);
	}

	if (CreatedDataAsset)
	{
		ObjectTools::DeleteSingleObject(CreatedDataAsset, false);
	}
	ObjectTools::DeleteSingleObject(DataAssetBlueprint, false);

	return Data.Asset.bCreated &&
		CreatedDataAsset != nullptr &&
		bUsesGeneratedClass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetFactoryServiceCreatesPrimaryDataAssetSubclassTest,
	"BlueprintHelper.AssetFactory.CreatesPrimaryDataAssetSubclass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAssetFactoryServiceCreatesPrimaryDataAssetSubclassTest::RunTest(const FString& Parameters)
{
	const FString AssetPath = FString::Printf(
		TEXT("/Game/BlueprintHelperSafety/DA_PrimaryCreate_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));

	const FBlueprintHelperAssetFactoryService Service;
	const FBlueprintHelperAssetFactoryData Data = Service.CreateAsset(
		AssetPath,
		EBlueprintHelperAssetType::DataAsset,
		TEXT(""),
		TEXT(""),
		TEXT(""),
		TEXT("/Script/Engine.PrimaryAssetLabel"),
		TArray<FBlueprintHelperAssetFactoryFieldSpec>(),
		EBlueprintHelperAssetCollisionPolicy::FailIfExists,
		false);

	TestTrue(TEXT("primary data asset subclass create reports created"), Data.Asset.bCreated);
	TestTrue(TEXT("primary data asset subclass is registered"), FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryTestAssetExists(AssetPath));
	TestEqual(TEXT("factory records requested data_asset_class"), Data.Factory.DataAssetClass, FString(TEXT("/Script/Engine.PrimaryAssetLabel")));

	UObject* CreatedAsset = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryTestObjectPath(AssetPath));
	UPrimaryDataAsset* CreatedPrimaryDataAsset = Cast<UPrimaryDataAsset>(CreatedAsset);
	TestNotNull(TEXT("created asset loads as UPrimaryDataAsset subclass"), CreatedPrimaryDataAsset);
	if (CreatedPrimaryDataAsset)
	{
		ObjectTools::DeleteSingleObject(CreatedPrimaryDataAsset, false);
	}

	return CreatedPrimaryDataAsset != nullptr && Data.Asset.bCreated;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetFactoryServiceReuseAcceptsPrimaryDataAssetSubclassTest,
	"BlueprintHelper.AssetFactory.ReuseAcceptsPrimaryDataAssetSubclass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAssetFactoryServiceReuseAcceptsPrimaryDataAssetSubclassTest::RunTest(const FString& Parameters)
{
	const FString AssetPath = FString::Printf(
		TEXT("/Game/BlueprintHelperSafety/DA_PrimaryReuse_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));

	const FBlueprintHelperAssetFactoryService Service;
	const FBlueprintHelperAssetFactoryData CreateData = Service.CreateAsset(
		AssetPath,
		EBlueprintHelperAssetType::DataAsset,
		TEXT(""),
		TEXT(""),
		TEXT(""),
		TEXT("/Script/Engine.PrimaryAssetLabel"),
		TArray<FBlueprintHelperAssetFactoryFieldSpec>(),
		EBlueprintHelperAssetCollisionPolicy::FailIfExists,
		false);

	UObject* CreatedAsset = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryTestObjectPath(AssetPath));
	UPrimaryDataAsset* CreatedPrimaryDataAsset = Cast<UPrimaryDataAsset>(CreatedAsset);
	TestTrue(TEXT("primary data asset fixture was created"), CreateData.Asset.bCreated);
	TestNotNull(TEXT("primary data asset fixture loads"), CreatedPrimaryDataAsset);
	if (!CreatedPrimaryDataAsset)
	{
		return false;
	}

	const FBlueprintHelperAssetFactoryData ReuseData = Service.CreateAsset(
		AssetPath,
		EBlueprintHelperAssetType::DataAsset,
		TEXT(""),
		TEXT(""),
		TEXT(""),
		TEXT("/Script/Engine.PrimaryAssetLabel"),
		TArray<FBlueprintHelperAssetFactoryFieldSpec>(),
		EBlueprintHelperAssetCollisionPolicy::ReuseIfExists,
		false);

	TestTrue(TEXT("reuse sees the existing primary data asset"), ReuseData.Asset.bAlreadyExisted);
	TestFalse(TEXT("reuse does not create a duplicate asset"), ReuseData.Asset.bCreated);
	TestTrue(TEXT("reuse accepts DataAsset subclass compatibility"), ReuseData.Collision.bHandled);
	TestEqual(TEXT("reuse records existing asset path"), ReuseData.Collision.ExistingAssetPath, AssetPath);

	ObjectTools::DeleteSingleObject(CreatedPrimaryDataAsset, false);

	return ReuseData.Asset.bAlreadyExisted && ReuseData.Collision.bHandled;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetFactoryServiceRejectsAbstractDataAssetClassTest,
	"BlueprintHelper.AssetFactory.RejectsAbstractDataAssetClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAssetFactoryServiceRejectsAbstractDataAssetClassTest::RunTest(const FString& Parameters)
{
	const FString AssetPath = FString::Printf(
		TEXT("/Game/BlueprintHelperSafety/DA_AbstractPrimary_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));

	const FBlueprintHelperAssetFactoryService Service;
	const FBlueprintHelperAssetFactoryData Data = Service.CreateAsset(
		AssetPath,
		EBlueprintHelperAssetType::DataAsset,
		TEXT(""),
		TEXT(""),
		TEXT(""),
		TEXT("/Script/Engine.PrimaryDataAsset"),
		TArray<FBlueprintHelperAssetFactoryFieldSpec>(),
		EBlueprintHelperAssetCollisionPolicy::FailIfExists,
		false);

	TestFalse(TEXT("abstract PrimaryDataAsset class is not instantiated"), Data.Asset.bCreated);
	TestFalse(TEXT("abstract data asset request does not register an asset"), FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryTestAssetExists(AssetPath));

	return !Data.Asset.bCreated && !FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryTestAssetExists(AssetPath);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAssetFactoryServiceCreatesWidgetBlueprintTest,
	"BlueprintHelper.AssetFactory.CreatesWidgetBlueprint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAssetFactoryServiceCreatesWidgetBlueprintTest::RunTest(const FString& Parameters)
{
	const FString AssetPath = FString::Printf(
		TEXT("/Game/BlueprintHelperSafety/WBP_Create_%s"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));

	const FBlueprintHelperAssetFactoryService Service;
	const FBlueprintHelperAssetFactoryData Data = Service.CreateAsset(
		AssetPath,
		EBlueprintHelperAssetType::WidgetBlueprint,
		TEXT(""),
		TEXT(""),
		TEXT(""),
		TEXT(""),
		TArray<FBlueprintHelperAssetFactoryFieldSpec>(),
		EBlueprintHelperAssetCollisionPolicy::FailIfExists,
		false);

	TestTrue(TEXT("widget blueprint create reports created"), Data.Asset.bCreated);
	TestTrue(TEXT("widget blueprint asset is registered"), FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryTestAssetExists(AssetPath));

	UObject* CreatedAsset = StaticLoadObject(UWidgetBlueprint::StaticClass(), nullptr, *FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryTestObjectPath(AssetPath));
	UWidgetBlueprint* CreatedWidgetBlueprint = Cast<UWidgetBlueprint>(CreatedAsset);
	TestNotNull(TEXT("created asset loads as UWidgetBlueprint"), CreatedWidgetBlueprint);
	if (CreatedWidgetBlueprint)
	{
		TestTrue(TEXT("widget blueprint defaults to UUserWidget parent"), CreatedWidgetBlueprint->ParentClass == UUserWidget::StaticClass());
		ObjectTools::DeleteSingleObject(CreatedWidgetBlueprint, false);
	}

	return CreatedWidgetBlueprint != nullptr && Data.Asset.bCreated;
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

	FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::FAssetFactoryTaskRuntimeTestServices Services;
	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetObjectField(TEXT("task_plan"), FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::MakeAssetFactoryCreateAssetTaskPlan(AssetPath, TEXT("Actor")));

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
	TestFalse(TEXT("Actor alias dry-run does not create an asset"), FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryTestAssetExists(AssetPath));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskRuntimeAssetFactoryPartialFailureBlocksDependentStepTest,
	"BlueprintHelper.TaskRuntime.AssetFactory.PartialFailureBlocksDependentStep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskRuntimeAssetFactoryPartialFailureBlocksDependentStepTest::RunTest(const FString& Parameters)
{
	const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString FailedDataAssetPath = FString::Printf(
		TEXT("/Game/BlueprintHelperSafety/DA_AbstractPartial_%s"),
		*Suffix);
	const FString BlockedInputActionPath = FString::Printf(
		TEXT("/Game/BlueprintHelperSafety/IA_Blocked_%s"),
		*Suffix);
	const FString IndependentInputActionPath = FString::Printf(
		TEXT("/Game/BlueprintHelperSafety/IA_Independent_%s"),
		*Suffix);
	const FString FailedStepId = TEXT("step_fail_abstract_data_asset");
	const FString BlockedStepId = TEXT("step_blocked_input_action");
	const FString IndependentStepId = TEXT("step_independent_input_action");

	TArray<FString> BlockedDependsOn;
	BlockedDependsOn.Add(FailedStepId);

	const TSharedPtr<FJsonObject> FailedStep = FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::MakeAssetFactoryCreateAssetStep(
		FailedStepId,
		FailedDataAssetPath,
		TEXT("data_asset"),
		TEXT(""),
		TEXT(""),
		TEXT(""),
		TEXT("/Script/Engine.PrimaryDataAsset"),
		TArray<FString>());
	const TSharedPtr<FJsonObject> BlockedStep = FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::MakeAssetFactoryCreateAssetStep(
		BlockedStepId,
		BlockedInputActionPath,
		TEXT("input_action"),
		TEXT(""),
		TEXT("bool"),
		TEXT(""),
		TEXT(""),
		BlockedDependsOn);
	const TSharedPtr<FJsonObject> IndependentStep = FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::MakeAssetFactoryCreateAssetStep(
		IndependentStepId,
		IndependentInputActionPath,
		TEXT("input_action"),
		TEXT(""),
		TEXT("bool"),
		TEXT(""),
		TEXT(""),
		TArray<FString>());

	TArray<TSharedPtr<FJsonObject>> StepObjects;
	StepObjects.Add(FailedStep);
	StepObjects.Add(BlockedStep);
	StepObjects.Add(IndependentStep);

	TArray<FString> TargetAssets;
	TargetAssets.Add(FailedDataAssetPath);
	TargetAssets.Add(BlockedInputActionPath);
	TargetAssets.Add(IndependentInputActionPath);

	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetObjectField(TEXT("task_plan"), FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::MakeAssetFactoryTaskPlanWithSteps(
		TEXT("ControlledPartialFailureFixture"),
		TEXT("create_blueprint_feature"),
		TargetAssets,
		StepObjects));

	FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::FAssetFactoryTaskRuntimeTestServices Services;
	const FBlueprintHelperToolResultBase ExecuteResult = Services.TaskRuntimeService.ExecuteTaskPlan(Payload);
	TestFalse(TEXT("controlled failure returns a failed task result"), ExecuteResult.bOk);
	TestNotNull(TEXT("failed task result still exposes runtime data"), ExecuteResult.Data.Get());

	FString TaskRunId;
	TestTrue(TEXT("runtime data includes task_run_id"), ExecuteResult.Data.IsValid() && ExecuteResult.Data->TryGetStringField(TEXT("task_run_id"), TaskRunId));

	const TArray<TSharedPtr<FJsonValue>>* RuntimeSteps = nullptr;
	TestTrue(TEXT("runtime result records executed steps only"), ExecuteResult.Data.IsValid() && ExecuteResult.Data->TryGetArrayField(TEXT("steps"), RuntimeSteps));
	TestEqual(TEXT("failed and independent steps executed; blocked dependent step did not execute"), RuntimeSteps ? RuntimeSteps->Num() : 0, 2);

	const FBlueprintHelperToolResultBase JournalResult = Services.TaskRuntimeService.GetTaskRunJournal(TaskRunId);
	TestTrue(TEXT("TaskRunJournal can be loaded for controlled partial failure"), JournalResult.bOk);
	TestNotNull(TEXT("TaskRunJournal data exists"), JournalResult.Data.Get());

	FString JournalStatus;
	TestTrue(TEXT("journal has status"), JournalResult.Data.IsValid() && JournalResult.Data->TryGetStringField(TEXT("status"), JournalStatus));
	TestEqual(TEXT("journal status is partial_failure"), JournalStatus, FString(TEXT("partial_failure")));

	const TArray<TSharedPtr<FJsonValue>>* JournalSteps = nullptr;
	TestTrue(TEXT("journal exposes all planned steps"), JournalResult.Data.IsValid() && JournalResult.Data->TryGetArrayField(TEXT("steps"), JournalSteps));
	TestEqual(TEXT("journal keeps failed, blocked, and independent steps"), JournalSteps ? JournalSteps->Num() : 0, 3);
	if (!JournalSteps || JournalSteps->Num() != 3)
	{
		return false;
	}

	auto FindJournalStep = [JournalSteps](const FString& StepId) -> TSharedPtr<FJsonObject>
	{
		for (const TSharedPtr<FJsonValue>& StepValue : *JournalSteps)
		{
			const TSharedPtr<FJsonObject> StepObject = StepValue.IsValid() ? StepValue->AsObject() : nullptr;
			FString CurrentStepId;
			if (StepObject.IsValid() && StepObject->TryGetStringField(TEXT("step_id"), CurrentStepId) && CurrentStepId == StepId)
			{
				return StepObject;
			}
		}
		return nullptr;
	};

	const TSharedPtr<FJsonObject> FailedJournalStep = FindJournalStep(FailedStepId);
	const TSharedPtr<FJsonObject> BlockedJournalStep = FindJournalStep(BlockedStepId);
	const TSharedPtr<FJsonObject> IndependentJournalStep = FindJournalStep(IndependentStepId);
	TestNotNull(TEXT("failed journal step exists"), FailedJournalStep.Get());
	TestNotNull(TEXT("blocked journal step exists"), BlockedJournalStep.Get());
	TestNotNull(TEXT("independent journal step exists"), IndependentJournalStep.Get());
	if (!FailedJournalStep.IsValid() || !BlockedJournalStep.IsValid() || !IndependentJournalStep.IsValid())
	{
		return false;
	}

	FString FailedStatus;
	FString BlockedStatus;
	FString IndependentStatus;
	TestTrue(TEXT("failed step has status"), FailedJournalStep->TryGetStringField(TEXT("status"), FailedStatus));
	TestTrue(TEXT("blocked step has status"), BlockedJournalStep->TryGetStringField(TEXT("status"), BlockedStatus));
	TestTrue(TEXT("independent step has status"), IndependentJournalStep->TryGetStringField(TEXT("status"), IndependentStatus));
	TestEqual(TEXT("first step failed"), FailedStatus, FString(TEXT("failed")));
	TestEqual(TEXT("dependent step is blocked"), BlockedStatus, FString(TEXT("blocked")));
	TestEqual(TEXT("independent step completed"), IndependentStatus, FString(TEXT("completed")));

	const TArray<TSharedPtr<FJsonValue>>* BlockedByStepIds = nullptr;
	FString BlockedReason;
	TestTrue(TEXT("blocked step records blocked_by_step_ids"), BlockedJournalStep->TryGetArrayField(TEXT("blocked_by_step_ids"), BlockedByStepIds));
	TestEqual(TEXT("blocked step has one blocker"), BlockedByStepIds ? BlockedByStepIds->Num() : 0, 1);
	TestTrue(TEXT("blocked step records blocked_reason"), BlockedJournalStep->TryGetStringField(TEXT("blocked_reason"), BlockedReason));
	TestEqual(TEXT("blocked reason is dependency_failed"), BlockedReason, FString(TEXT("dependency_failed")));
	if (BlockedByStepIds && BlockedByStepIds->Num() == 1)
	{
		TestEqual(TEXT("blocked step points at the failed dependency"), (*BlockedByStepIds)[0]->AsString(), FailedStepId);
	}

	TestFalse(TEXT("abstract DataAsset failure did not create an asset"), FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryTestAssetExists(FailedDataAssetPath));
	TestFalse(TEXT("blocked dependent asset was not created"), FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryTestAssetExists(BlockedInputActionPath));
	TestTrue(TEXT("independent asset was created"), FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryTestAssetExists(IndependentInputActionPath));

	UObject* IndependentAsset = StaticLoadObject(UObject::StaticClass(), nullptr, *FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::AssetFactoryTestObjectPath(IndependentInputActionPath));
	if (IndependentAsset)
	{
		ObjectTools::DeleteSingleObject(IndependentAsset, false);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperTaskPlanAssetFactoryAdapterRejectsOperationFieldTest,
	"BlueprintHelper.TaskPlan.AssetFactoryAdapter.RejectsOperationField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperTaskPlanAssetFactoryAdapterRejectsOperationFieldTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
	const TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::MakeAssetFactoryCreateAssetStep();
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
	const TSharedPtr<FJsonObject> Step = FBlueprintHelperTaskPlanAssetFactoryAdapterTestsLocalUtils::MakeAssetFactoryCreateAssetStep();

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
