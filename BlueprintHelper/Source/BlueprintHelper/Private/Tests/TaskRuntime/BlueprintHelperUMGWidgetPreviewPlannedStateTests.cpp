#if WITH_DEV_AUTOMATION_TESTS

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/UMGWidget/BlueprintHelperWidgetTaskPlanAdapter.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/ExpandableArea.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Shared/BlueprintHelperWidgetVersionCompat.h"
#include "Shared/Services/BlueprintHelperBlueprintStructureService.h"
#include "Systems/Debug/BlueprintHelperAssetBrowseService.h"
#include "Systems/Debug/BlueprintHelperCompileAssetService.h"
#include "Systems/Debug/BlueprintHelperCompileService.h"
#include "Systems/ToolClusters/AssetFactory/BlueprintHelperAssetFactoryService.h"
#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassSettingsService.h"
#include "Systems/ToolClusters/BlueprintComponent/BlueprintHelperComponentService.h"
#include "Systems/ToolClusters/BlueprintVariables/BlueprintHelperBlueprintVariableService.h"
#include "Systems/ToolClusters/DataTable/BlueprintHelperDataTableService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperAppendBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperGraphWriteServiceRegistry.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperBlockIdService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphSnapshotService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonPathService.h"
#include "Systems/ToolClusters/ObjectProperty/BlueprintHelperPropertyReflectionService.h"
#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetService.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"

class FBlueprintHelperUMGWidgetPreviewPlannedStateTestsLocalUtils
{
public:
	struct FRuntimeServices
	{
		FBlueprintHelperGraphResolver Resolver;
		FBlueprintHelperCompileService CompileService;
		FBlueprintHelperAssetBrowseService AssetBrowseService;
		FBlueprintHelperBlockIdService BlockIdService;
		FBlueprintHelperOwnershipService OwnershipService;
		FBlueprintHelperGraphSnapshotService SnapshotService;
		FBlueprintHelperLogicJsonPathService PathService;
		FBlueprintHelperAppendBlueprintGraphService AppendGraphService;
		FBlueprintHelperReplaceBlueprintGraphService ReplaceGraphService;
		FBlueprintHelperPatchBlueprintGraphService PatchGraphService;
		FBlueprintHelperMergeBlueprintGraphService MergeGraphService;
		FBlueprintHelperGraphWriteServiceRegistry GraphWriteRegistry;
		FBlueprintHelperBlueprintStructureService StructureService;
		FBlueprintHelperBlueprintVariableService VariableService;
		FBlueprintHelperAssetFactoryService AssetFactoryService;
		FBlueprintHelperComponentService ComponentService;
		FBlueprintHelperClassSettingsService ClassSettingsService;
		FBlueprintHelperWidgetService WidgetService;
		FBlueprintHelperDataTableService DataTableService;
		FBlueprintHelperPropertyReflectionService PropertyReflectionService;
		FBlueprintHelperCompileAssetService CompileAssetService;
		FBlueprintHelperTaskRuntimeService RuntimeService;

		FRuntimeServices()
			: CompileService(Resolver)
			, AppendGraphService(Resolver, BlockIdService, OwnershipService)
			, ReplaceGraphService(Resolver, BlockIdService, OwnershipService, SnapshotService)
			, PatchGraphService(Resolver, PathService)
			, MergeGraphService(Resolver, PathService)
			, StructureService(Resolver)
			, VariableService(Resolver, StructureService)
			, ComponentService(Resolver)
			, ClassSettingsService(Resolver)
			, CompileAssetService(CompileService)
			, RuntimeService(
				GraphWriteRegistry,
				VariableService,
				StructureService,
				AssetFactoryService,
				ComponentService,
				ClassSettingsService,
				WidgetService,
				DataTableService,
				PropertyReflectionService,
				CompileAssetService,
				AssetBrowseService)
		{
		}
	};

	static UWidgetBlueprint* MakeEmptyWidgetBlueprint()
	{
		const FString PackageName = FString::Printf(
			TEXT("/Game/BlueprintHelperSafety/WBP_UMGPreviewPlanned_%s"),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UPackage* Package = CreatePackage(*PackageName);
		Package->SetDirtyFlag(false);

		UWidgetBlueprint* WidgetBlueprint = NewObject<UWidgetBlueprint>(
			Package,
			*FPackageName::GetLongPackageAssetName(PackageName),
			RF_Public | RF_Standalone | RF_Transactional);
		WidgetBlueprint->ParentClass = UUserWidget::StaticClass();
		WidgetBlueprint->WidgetTree = NewObject<UWidgetTree>(
			WidgetBlueprint,
			TEXT("WidgetTree"),
			RF_Transactional);
		Package->SetDirtyFlag(false);
		return WidgetBlueprint;
	}

	static TSharedRef<FJsonObject> MakeWidgetStep(
		const FString& StepId,
		const FString& AssetPath,
		const TSharedRef<FJsonObject>& Op)
	{
		TSharedRef<FJsonObject> Step = MakeShared<FJsonObject>();
		Step->SetStringField(TEXT("step_id"), StepId);
		Step->SetStringField(TEXT("capability"), FBlueprintHelperWidgetTaskPlan::Capability::UMGWidget);

		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);
		Step->SetObjectField(TEXT("target"), Target);

		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op));

		TSharedRef<FJsonObject> Write = MakeShared<FJsonObject>();
		Write->SetStringField(TEXT("strategy"), FBlueprintHelperWidgetTaskPlan::Strategy::WidgetTreeEdit);
		Write->SetArrayField(TEXT("ops"), Ops);
		Step->SetObjectField(TEXT("write"), Write);

		TSharedRef<FJsonObject> Constraints = MakeShared<FJsonObject>();
		Constraints->SetBoolField(TEXT("allow_remove_referenced_widgets"), false);
		Step->SetObjectField(TEXT("constraints"), Constraints);
		return Step;
	}

	static TSharedRef<FJsonObject> MakeAddOp(
		const FString& WidgetName,
		const FString& WidgetClass,
		const FString& ParentName = FString(),
		TOptional<int32> VirtualIndex = TOptional<int32>())
	{
		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), FBlueprintHelperWidgetTaskPlan::Op::AddWidget);
		Op->SetStringField(TEXT("widget_name"), WidgetName);
		Op->SetStringField(TEXT("widget_class"), WidgetClass);
		if (!ParentName.IsEmpty())
		{
			Op->SetStringField(TEXT("parent_name"), ParentName);
			Op->SetStringField(TEXT("expected_parent_name"), ParentName);
		}
		if (VirtualIndex.IsSet())
		{
			Op->SetNumberField(TEXT("virtual_index"), VirtualIndex.GetValue());
		}
		return Op;
	}

	static TSharedRef<FJsonObject> MakeMoveOp()
	{
		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), FBlueprintHelperWidgetTaskPlan::Op::MoveWidget);
		Op->SetStringField(TEXT("widget_name"), TEXT("TitleText"));
		Op->SetStringField(TEXT("new_parent_name"), TEXT("RootCanvas"));
		Op->SetNumberField(TEXT("virtual_index"), 0);
		Op->SetStringField(TEXT("expected_parent_name"), TEXT("RootCanvas"));
		Op->SetNumberField(TEXT("expected_virtual_index"), 1);
		return Op;
	}

	static TSharedRef<FJsonObject> MakeMoveToNamedSlotOp()
	{
		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), FBlueprintHelperWidgetTaskPlan::Op::MoveWidget);
		Op->SetStringField(TEXT("widget_name"), TEXT("BodyText"));
		Op->SetStringField(TEXT("new_parent_name"), TEXT("DialogShell"));
		Op->SetStringField(TEXT("slot_name"), TEXT("Body"));
		Op->SetNumberField(TEXT("virtual_index"), 0);
		Op->SetStringField(TEXT("expected_parent_name"), TEXT("RootCanvas"));
		Op->SetNumberField(TEXT("expected_virtual_index"), 1);
		return Op;
	}

	static TSharedRef<FJsonObject> MakeNamedSlotOp()
	{
		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), FBlueprintHelperWidgetTaskPlan::Op::SetNamedSlotContent);
		Op->SetStringField(TEXT("host_widget_name"), TEXT("DialogShell"));
		Op->SetStringField(TEXT("slot_name"), TEXT("Body"));
		Op->SetStringField(TEXT("widget_name"), TEXT("BodyText"));
		Op->SetStringField(TEXT("widget_class"), TEXT("TextBlock"));
		Op->SetNumberField(TEXT("virtual_index"), 0);
		Op->SetBoolField(TEXT("replace_existing"), true);
		return Op;
	}

	static TSharedRef<FJsonObject> MakeNamedSlotOp(
		const FString& WidgetName,
		const FString& WidgetClass,
		const FString& ExpectedContentWidgetName = FString())
	{
		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), FBlueprintHelperWidgetTaskPlan::Op::SetNamedSlotContent);
		Op->SetStringField(TEXT("host_widget_name"), TEXT("DialogShell"));
		Op->SetStringField(TEXT("slot_name"), TEXT("Body"));
		Op->SetStringField(TEXT("widget_name"), WidgetName);
		Op->SetStringField(TEXT("widget_class"), WidgetClass);
		Op->SetNumberField(TEXT("virtual_index"), 0);
		Op->SetBoolField(TEXT("replace_existing"), true);
		if (!ExpectedContentWidgetName.IsEmpty())
		{
			Op->SetStringField(TEXT("expected_content_widget_name"), ExpectedContentWidgetName);
		}
		return Op;
	}

	static TSharedRef<FJsonObject> MakeRenameOp()
	{
		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), FBlueprintHelperWidgetTaskPlan::Op::RenameWidget);
		Op->SetStringField(TEXT("widget_name"), TEXT("TitleText"));
		Op->SetStringField(TEXT("new_widget_name"), TEXT("HeaderText"));
		return Op;
	}

	static TSharedRef<FJsonObject> MakeDuplicateSubtreeOp()
	{
		TSharedRef<FJsonObject> NameMapping = MakeShared<FJsonObject>();
		NameMapping->SetStringField(TEXT("HeaderText"), TEXT("HeaderCopy"));

		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), FBlueprintHelperWidgetTaskPlan::Op::DuplicateWidgetSubtree);
		Op->SetStringField(TEXT("source_widget_name"), TEXT("HeaderText"));
		Op->SetStringField(TEXT("target_parent_name"), TEXT("RootCanvas"));
		Op->SetNumberField(TEXT("virtual_index"), 1);
		Op->SetObjectField(TEXT("name_mapping"), NameMapping);
		return Op;
	}

	static TSharedRef<FJsonObject> MakeWrapOp()
	{
		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), FBlueprintHelperWidgetTaskPlan::Op::WrapWidget);
		Op->SetStringField(TEXT("widget_name"), TEXT("HeaderCopy"));
		Op->SetStringField(TEXT("wrapper_class"), TEXT("CanvasPanel"));
		Op->SetStringField(TEXT("wrapper_name"), TEXT("WrapperPanel"));
		return Op;
	}

	static TSharedRef<FJsonObject> MakeReplaceWidgetClassOp()
	{
		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), FBlueprintHelperWidgetTaskPlan::Op::ReplaceWidgetClass);
		Op->SetStringField(TEXT("widget_name"), TEXT("WrapperPanel"));
		Op->SetStringField(TEXT("new_widget_class"), TEXT("Border"));
		Op->SetBoolField(TEXT("preserve_children"), true);
		return Op;
	}

	static TSharedRef<FJsonObject> MakeRemoveOp()
	{
		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), FBlueprintHelperWidgetTaskPlan::Op::RemoveWidget);
		Op->SetStringField(TEXT("widget_name"), TEXT("HeaderText"));
		return Op;
	}

	static TSharedRef<FJsonObject> MakeRemoveRootOp()
	{
		TSharedRef<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), FBlueprintHelperWidgetTaskPlan::Op::RemoveRootWidget);
		Op->SetStringField(TEXT("root_widget_name"), TEXT("RootCanvas"));
		Op->SetStringField(TEXT("replacement_policy"), TEXT("promote_single_child"));
		return Op;
	}

	static TSharedRef<FJsonObject> MakeTaskPlanPayload(const FString& AssetPath)
	{
		TArray<TSharedPtr<FJsonValue>> Steps;
		Steps.Add(MakeShared<FJsonValueObject>(MakeWidgetStep(
			TEXT("step_create_root"),
			AssetPath,
			MakeAddOp(TEXT("RootCanvas"), TEXT("CanvasPanel")))));
		Steps.Add(MakeShared<FJsonValueObject>(MakeWidgetStep(
			TEXT("step_add_host"),
			AssetPath,
			MakeAddOp(TEXT("DialogShell"), TEXT("ExpandableArea"), TEXT("RootCanvas"), 0))));
		Steps.Add(MakeShared<FJsonValueObject>(MakeWidgetStep(
			TEXT("step_add_title"),
			AssetPath,
			MakeAddOp(TEXT("TitleText"), TEXT("TextBlock"), TEXT("RootCanvas"), 1))));
		Steps.Add(MakeShared<FJsonValueObject>(MakeWidgetStep(
			TEXT("step_move_title"),
			AssetPath,
			MakeMoveOp())));
		Steps.Add(MakeShared<FJsonValueObject>(MakeWidgetStep(
			TEXT("step_set_slot"),
			AssetPath,
			MakeNamedSlotOp())));

		TArray<TSharedPtr<FJsonValue>> TargetAssets;
		TargetAssets.Add(MakeShared<FJsonValueString>(AssetPath));

		TSharedRef<FJsonObject> ExecutionPolicy = MakeShared<FJsonObject>();
		ExecutionPolicy->SetStringField(TEXT("dry_run_mode"), TEXT("full"));
		ExecutionPolicy->SetBoolField(TEXT("should_compile"), false);
		ExecutionPolicy->SetBoolField(TEXT("should_save"), false);

		TSharedRef<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
		TaskPlan->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.TaskPlan.v1"));
		TaskPlan->SetStringField(TEXT("task_type"), TEXT("edit_umg_widget"));
		TaskPlan->SetStringField(TEXT("task_name"), TEXT("UMGWidgetPreviewPlannedState"));
		TaskPlan->SetArrayField(TEXT("target_assets"), TargetAssets);
		TaskPlan->SetObjectField(TEXT("execution_policy"), ExecutionPolicy);
		TaskPlan->SetArrayField(TEXT("steps"), Steps);

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetObjectField(TEXT("task_plan"), TaskPlan);
		return Payload;
	}

	static TSharedRef<FJsonObject> MakeP2StructuralOpsPayload(const FString& AssetPath)
	{
		TArray<TSharedPtr<FJsonValue>> Steps;
		Steps.Add(MakeShared<FJsonValueObject>(MakeWidgetStep(
			TEXT("step_create_root"),
			AssetPath,
			MakeAddOp(TEXT("RootCanvas"), TEXT("CanvasPanel")))));
		Steps.Add(MakeShared<FJsonValueObject>(MakeWidgetStep(
			TEXT("step_add_title"),
			AssetPath,
			MakeAddOp(TEXT("TitleText"), TEXT("TextBlock"), TEXT("RootCanvas"), 0))));
		Steps.Add(MakeShared<FJsonValueObject>(MakeWidgetStep(
			TEXT("step_rename_title"),
			AssetPath,
			MakeRenameOp())));
		Steps.Add(MakeShared<FJsonValueObject>(MakeWidgetStep(
			TEXT("step_duplicate_header"),
			AssetPath,
			MakeDuplicateSubtreeOp())));
		Steps.Add(MakeShared<FJsonValueObject>(MakeWidgetStep(
			TEXT("step_wrap_header_copy"),
			AssetPath,
			MakeWrapOp())));
		Steps.Add(MakeShared<FJsonValueObject>(MakeWidgetStep(
			TEXT("step_replace_wrapper_class"),
			AssetPath,
			MakeReplaceWidgetClassOp())));
		Steps.Add(MakeShared<FJsonValueObject>(MakeWidgetStep(
			TEXT("step_remove_original_header"),
			AssetPath,
			MakeRemoveOp())));
		Steps.Add(MakeShared<FJsonValueObject>(MakeWidgetStep(
			TEXT("step_promote_single_child_root"),
			AssetPath,
			MakeRemoveRootOp())));

		TArray<TSharedPtr<FJsonValue>> TargetAssets;
		TargetAssets.Add(MakeShared<FJsonValueString>(AssetPath));

		TSharedRef<FJsonObject> ExecutionPolicy = MakeShared<FJsonObject>();
		ExecutionPolicy->SetStringField(TEXT("dry_run_mode"), TEXT("full"));
		ExecutionPolicy->SetBoolField(TEXT("should_compile"), false);
		ExecutionPolicy->SetBoolField(TEXT("should_save"), false);

		TSharedRef<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
		TaskPlan->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.TaskPlan.v1"));
		TaskPlan->SetStringField(TEXT("task_type"), TEXT("edit_umg_widget"));
		TaskPlan->SetStringField(TEXT("task_name"), TEXT("UMGWidgetPreviewP2StructuralOps"));
		TaskPlan->SetArrayField(TEXT("target_assets"), TargetAssets);
		TaskPlan->SetObjectField(TEXT("execution_policy"), ExecutionPolicy);
		TaskPlan->SetArrayField(TEXT("steps"), Steps);

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetObjectField(TEXT("task_plan"), TaskPlan);
		return Payload;
	}

	static TSharedRef<FJsonObject> MakeImplicitRootParentMismatchPayload(const FString& AssetPath)
	{
		TArray<TSharedPtr<FJsonValue>> Steps;
		Steps.Add(MakeShared<FJsonValueObject>(MakeWidgetStep(
			TEXT("step_create_non_panel_root"),
			AssetPath,
			MakeAddOp(TEXT("RootText"), TEXT("TextBlock")))));
		Steps.Add(MakeShared<FJsonValueObject>(MakeWidgetStep(
			TEXT("step_add_implicit_child"),
			AssetPath,
			MakeAddOp(TEXT("ChildText"), TEXT("TextBlock")))));

		TArray<TSharedPtr<FJsonValue>> TargetAssets;
		TargetAssets.Add(MakeShared<FJsonValueString>(AssetPath));

		TSharedRef<FJsonObject> ExecutionPolicy = MakeShared<FJsonObject>();
		ExecutionPolicy->SetStringField(TEXT("dry_run_mode"), TEXT("full"));
		ExecutionPolicy->SetBoolField(TEXT("should_compile"), false);
		ExecutionPolicy->SetBoolField(TEXT("should_save"), false);

		TSharedRef<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
		TaskPlan->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.TaskPlan.v1"));
		TaskPlan->SetStringField(TEXT("task_type"), TEXT("edit_umg_widget"));
		TaskPlan->SetStringField(TEXT("task_name"), TEXT("UMGWidgetPreviewImplicitRootParentMismatch"));
		TaskPlan->SetArrayField(TEXT("target_assets"), TargetAssets);
		TaskPlan->SetObjectField(TEXT("execution_policy"), ExecutionPolicy);
		TaskPlan->SetArrayField(TEXT("steps"), Steps);

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetObjectField(TEXT("task_plan"), TaskPlan);
		return Payload;
	}

	static TSharedRef<FJsonObject> MakeMoveToNamedSlotPayload(const FString& AssetPath)
	{
		TArray<TSharedPtr<FJsonValue>> Steps;
		Steps.Add(MakeShared<FJsonValueObject>(MakeWidgetStep(
			TEXT("step_create_root"),
			AssetPath,
			MakeAddOp(TEXT("RootCanvas"), TEXT("CanvasPanel")))));
		Steps.Add(MakeShared<FJsonValueObject>(MakeWidgetStep(
			TEXT("step_add_host"),
			AssetPath,
			MakeAddOp(TEXT("DialogShell"), TEXT("ExpandableArea"), TEXT("RootCanvas"), 0))));
		Steps.Add(MakeShared<FJsonValueObject>(MakeWidgetStep(
			TEXT("step_add_body"),
			AssetPath,
			MakeAddOp(TEXT("BodyText"), TEXT("TextBlock"), TEXT("RootCanvas"), 1))));
		Steps.Add(MakeShared<FJsonValueObject>(MakeWidgetStep(
			TEXT("step_move_body_to_named_slot"),
			AssetPath,
			MakeMoveToNamedSlotOp())));

		TArray<TSharedPtr<FJsonValue>> TargetAssets;
		TargetAssets.Add(MakeShared<FJsonValueString>(AssetPath));

		TSharedRef<FJsonObject> ExecutionPolicy = MakeShared<FJsonObject>();
		ExecutionPolicy->SetStringField(TEXT("dry_run_mode"), TEXT("full"));
		ExecutionPolicy->SetBoolField(TEXT("should_compile"), false);
		ExecutionPolicy->SetBoolField(TEXT("should_save"), false);

		TSharedRef<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
		TaskPlan->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.TaskPlan.v1"));
		TaskPlan->SetStringField(TEXT("task_type"), TEXT("edit_umg_widget"));
		TaskPlan->SetStringField(TEXT("task_name"), TEXT("UMGWidgetPreviewMoveToNamedSlot"));
		TaskPlan->SetArrayField(TEXT("target_assets"), TargetAssets);
		TaskPlan->SetObjectField(TEXT("execution_policy"), ExecutionPolicy);
		TaskPlan->SetArrayField(TEXT("steps"), Steps);

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetObjectField(TEXT("task_plan"), TaskPlan);
		return Payload;
	}

	static TSharedRef<FJsonObject> MakeNamedSlotReplacementReusesRetiringSubtreeNamePayload(const FString& AssetPath)
	{
		TArray<TSharedPtr<FJsonValue>> Steps;
		Steps.Add(MakeShared<FJsonValueObject>(MakeWidgetStep(
			TEXT("step_replace_slot_content"),
			AssetPath,
			MakeNamedSlotOp(TEXT("ReusableName"), TEXT("TextBlock"), TEXT("OldBodyPanel")))));

		TArray<TSharedPtr<FJsonValue>> TargetAssets;
		TargetAssets.Add(MakeShared<FJsonValueString>(AssetPath));

		TSharedRef<FJsonObject> ExecutionPolicy = MakeShared<FJsonObject>();
		ExecutionPolicy->SetStringField(TEXT("dry_run_mode"), TEXT("full"));
		ExecutionPolicy->SetBoolField(TEXT("should_compile"), false);
		ExecutionPolicy->SetBoolField(TEXT("should_save"), false);

		TSharedRef<FJsonObject> TaskPlan = MakeShared<FJsonObject>();
		TaskPlan->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.TaskPlan.v1"));
		TaskPlan->SetStringField(TEXT("task_type"), TEXT("edit_umg_widget"));
		TaskPlan->SetStringField(TEXT("task_name"), TEXT("UMGWidgetPreviewNamedSlotReplacementReuse"));
		TaskPlan->SetArrayField(TEXT("target_assets"), TargetAssets);
		TaskPlan->SetObjectField(TEXT("execution_policy"), ExecutionPolicy);
		TaskPlan->SetArrayField(TEXT("steps"), Steps);

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetObjectField(TEXT("task_plan"), TaskPlan);
		return Payload;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperUMGWidgetPreviewPlannedStateChainsStructuralOpsTest,
	"BlueprintHelper.TaskRuntime.UMGWidgetPreview.PlannedStateChainsStructuralOps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperUMGWidgetPreviewPlannedStateChainsStructuralOpsTest::RunTest(const FString& Parameters)
{
	UWidgetBlueprint* WidgetBlueprint =
		FBlueprintHelperUMGWidgetPreviewPlannedStateTestsLocalUtils::MakeEmptyWidgetBlueprint();
	TestNotNull(TEXT("empty WidgetBlueprint fixture exists"), WidgetBlueprint);
	TestNotNull(TEXT("empty WidgetBlueprint has a WidgetTree"), WidgetBlueprint ? WidgetBlueprint->WidgetTree.Get() : nullptr);
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return false;
	}

	const FString AssetPath = WidgetBlueprint->GetPathName();
	FBlueprintHelperUMGWidgetPreviewPlannedStateTestsLocalUtils::FRuntimeServices Services;
	const FBlueprintHelperToolResultBase Result = Services.RuntimeService.PreviewTaskPlan(
		FBlueprintHelperUMGWidgetPreviewPlannedStateTestsLocalUtils::MakeTaskPlanPayload(AssetPath));

	bool bPassed = false;
	bool bBlocked = true;
	if (Result.Data.IsValid())
	{
		Result.Data->TryGetBoolField(TEXT("passed"), bPassed);
		Result.Data->TryGetBoolField(TEXT("blocked"), bBlocked);
	}

	TestTrue(TEXT("preview succeeds through planned WidgetTree state"), Result.bOk);
	TestTrue(TEXT("preview data passed is true"), bPassed);
	TestFalse(TEXT("preview data blocked is false"), bBlocked);
	TestEqual(TEXT("preview remains dry-run"), static_cast<int32>(Result.Status), static_cast<int32>(EBlueprintHelperToolStatus::DryRun));
	TestNull(TEXT("preview does not mutate real root widget"), WidgetBlueprint->WidgetTree->RootWidget);
	TestFalse(TEXT("preview does not dirty package"), WidgetBlueprint->GetOutermost()->IsDirty());
	return Result.bOk && bPassed && !bBlocked && WidgetBlueprint->WidgetTree->RootWidget == nullptr;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperUMGWidgetPreviewPlannedStateMovesExistingWidgetToNamedSlotTest,
	"BlueprintHelper.TaskRuntime.UMGWidgetPreview.PlannedStateMovesExistingWidgetToNamedSlot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperUMGWidgetPreviewPlannedStateMovesExistingWidgetToNamedSlotTest::RunTest(const FString& Parameters)
{
	UWidgetBlueprint* WidgetBlueprint =
		FBlueprintHelperUMGWidgetPreviewPlannedStateTestsLocalUtils::MakeEmptyWidgetBlueprint();
	TestNotNull(TEXT("empty WidgetBlueprint fixture exists"), WidgetBlueprint);
	TestNotNull(TEXT("empty WidgetBlueprint has a WidgetTree"), WidgetBlueprint ? WidgetBlueprint->WidgetTree.Get() : nullptr);
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return false;
	}

	const FString AssetPath = WidgetBlueprint->GetPathName();
	FBlueprintHelperUMGWidgetPreviewPlannedStateTestsLocalUtils::FRuntimeServices Services;
	const FBlueprintHelperToolResultBase Result = Services.RuntimeService.PreviewTaskPlan(
		FBlueprintHelperUMGWidgetPreviewPlannedStateTestsLocalUtils::MakeMoveToNamedSlotPayload(AssetPath));

	bool bPassed = false;
	bool bBlocked = true;
	if (Result.Data.IsValid())
	{
		Result.Data->TryGetBoolField(TEXT("passed"), bPassed);
		Result.Data->TryGetBoolField(TEXT("blocked"), bBlocked);
	}

	TestTrue(TEXT("preview succeeds when existing widget is moved into named slot"), Result.bOk);
	TestTrue(TEXT("named-slot move preview passes"), bPassed);
	TestFalse(TEXT("named-slot move preview is not blocked"), bBlocked);
	TestNull(TEXT("preview does not mutate real root widget"), WidgetBlueprint->WidgetTree->RootWidget);
	TestFalse(TEXT("preview does not dirty package"), WidgetBlueprint->GetOutermost()->IsDirty());
	return Result.bOk && bPassed && !bBlocked && WidgetBlueprint->WidgetTree->RootWidget == nullptr;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperUMGWidgetPreviewPlannedStateSetNamedSlotContentReusesRetiringSubtreeNameTest,
	"BlueprintHelper.TaskRuntime.UMGWidgetPreview.PlannedStateSetNamedSlotContentReusesRetiringSubtreeName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperUMGWidgetPreviewPlannedStateSetNamedSlotContentReusesRetiringSubtreeNameTest::RunTest(const FString& Parameters)
{
	UWidgetBlueprint* WidgetBlueprint =
		FBlueprintHelperUMGWidgetPreviewPlannedStateTestsLocalUtils::MakeEmptyWidgetBlueprint();
	TestNotNull(TEXT("empty WidgetBlueprint fixture exists"), WidgetBlueprint);
	TestNotNull(TEXT("empty WidgetBlueprint has a WidgetTree"), WidgetBlueprint ? WidgetBlueprint->WidgetTree.Get() : nullptr);
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return false;
	}

	UCanvasPanel* Root = WidgetBlueprint->WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("RootCanvas"));
	WidgetBlueprint->WidgetTree->RootWidget = Root;
	FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(WidgetBlueprint, Root);

	UExpandableArea* DialogShell = WidgetBlueprint->WidgetTree->ConstructWidget<UExpandableArea>(
		UExpandableArea::StaticClass(),
		TEXT("DialogShell"));
	Root->AddChild(DialogShell);
	FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(WidgetBlueprint, DialogShell);

	UCanvasPanel* OldBody = WidgetBlueprint->WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("OldBodyPanel"));
	FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(WidgetBlueprint, OldBody);

	UTextBlock* RetiringChild = WidgetBlueprint->WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("ReusableName"));
	OldBody->AddChild(RetiringChild);
	FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(WidgetBlueprint, RetiringChild);
	DialogShell->SetContentForSlot(FName(TEXT("Body")), OldBody);
	WidgetBlueprint->GetOutermost()->SetDirtyFlag(false);

	const FString AssetPath = WidgetBlueprint->GetPathName();
	FBlueprintHelperUMGWidgetPreviewPlannedStateTestsLocalUtils::FRuntimeServices Services;
	const FBlueprintHelperToolResultBase Result = Services.RuntimeService.PreviewTaskPlan(
		FBlueprintHelperUMGWidgetPreviewPlannedStateTestsLocalUtils::MakeNamedSlotReplacementReusesRetiringSubtreeNamePayload(AssetPath));

	bool bPassed = false;
	bool bBlocked = true;
	if (Result.Data.IsValid())
	{
		Result.Data->TryGetBoolField(TEXT("passed"), bPassed);
		Result.Data->TryGetBoolField(TEXT("blocked"), bBlocked);
	}

	TestTrue(TEXT("preview succeeds when replacement reuses a retiring subtree child name"), Result.bOk);
	TestTrue(TEXT("named-slot replacement preview passes"), bPassed);
	TestFalse(TEXT("named-slot replacement preview is not blocked"), bBlocked);
	TestEqual(
		TEXT("preview does not mutate real named slot content"),
		DialogShell->GetContentForSlot(FName(TEXT("Body"))),
		Cast<UWidget>(OldBody));
	TestFalse(TEXT("preview does not dirty package"), WidgetBlueprint->GetOutermost()->IsDirty());
	return Result.bOk && bPassed && !bBlocked && DialogShell->GetContentForSlot(FName(TEXT("Body"))) == OldBody;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperUMGWidgetPreviewPlannedStateChainsP2StructuralOpsTest,
	"BlueprintHelper.TaskRuntime.UMGWidgetPreview.PlannedStateChainsP2StructuralOps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperUMGWidgetPreviewPlannedStateChainsP2StructuralOpsTest::RunTest(const FString& Parameters)
{
	UWidgetBlueprint* WidgetBlueprint =
		FBlueprintHelperUMGWidgetPreviewPlannedStateTestsLocalUtils::MakeEmptyWidgetBlueprint();
	TestNotNull(TEXT("empty WidgetBlueprint fixture exists"), WidgetBlueprint);
	TestNotNull(TEXT("empty WidgetBlueprint has a WidgetTree"), WidgetBlueprint ? WidgetBlueprint->WidgetTree.Get() : nullptr);
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return false;
	}

	const FString AssetPath = WidgetBlueprint->GetPathName();
	FBlueprintHelperUMGWidgetPreviewPlannedStateTestsLocalUtils::FRuntimeServices Services;
	const FBlueprintHelperToolResultBase Result = Services.RuntimeService.PreviewTaskPlan(
		FBlueprintHelperUMGWidgetPreviewPlannedStateTestsLocalUtils::MakeP2StructuralOpsPayload(AssetPath));

	bool bPassed = false;
	bool bBlocked = true;
	FString PlannedRootName;
	FString PlannedRootClass;
	bool bHasWrapperPanel = false;
	bool bHasHeaderCopy = false;
	bool bHasOriginalHeader = true;
	bool bHasOriginalRoot = true;
	if (Result.Data.IsValid())
	{
		Result.Data->TryGetBoolField(TEXT("passed"), bPassed);
		Result.Data->TryGetBoolField(TEXT("blocked"), bBlocked);

		const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
		TSharedPtr<FJsonObject> ReadbackContext;
		if (Result.Data->TryGetArrayField(TEXT("steps"), Steps) && Steps && Steps->Num() > 0)
		{
			const TSharedPtr<FJsonObject> LastStep = (*Steps)[Steps->Num() - 1]->AsObject();
			const TSharedPtr<FJsonObject>* StepResult = nullptr;
			const TSharedPtr<FJsonObject>* StepData = nullptr;
			const TSharedPtr<FJsonObject>* StepReadbackContext = nullptr;
			if (LastStep.IsValid() &&
				LastStep->TryGetObjectField(TEXT("result"), StepResult) &&
				StepResult &&
				StepResult->IsValid() &&
				(*StepResult)->TryGetObjectField(TEXT("data"), StepData) &&
				StepData &&
				StepData->IsValid() &&
				(*StepData)->TryGetObjectField(TEXT("readback_context"), StepReadbackContext) &&
				StepReadbackContext &&
				StepReadbackContext->IsValid())
			{
				ReadbackContext = *StepReadbackContext;
			}
		}

		if (ReadbackContext.IsValid())
		{
			const TSharedPtr<FJsonObject>* RootObject = nullptr;
			if (ReadbackContext->TryGetObjectField(TEXT("root"), RootObject) && RootObject && RootObject->IsValid())
			{
				(*RootObject)->TryGetStringField(TEXT("widget_name"), PlannedRootName);
				(*RootObject)->TryGetStringField(TEXT("widget_class"), PlannedRootClass);
			}

			const TSharedPtr<FJsonObject>* IndexObject = nullptr;
			if (ReadbackContext->TryGetObjectField(TEXT("index"), IndexObject) && IndexObject && IndexObject->IsValid())
			{
				bHasWrapperPanel = (*IndexObject)->HasField(TEXT("WrapperPanel"));
				bHasHeaderCopy = (*IndexObject)->HasField(TEXT("HeaderCopy"));
				bHasOriginalHeader = (*IndexObject)->HasField(TEXT("HeaderText"));
				bHasOriginalRoot = (*IndexObject)->HasField(TEXT("RootCanvas"));
			}
		}
	}

	TestTrue(TEXT("preview succeeds through planned P2 WidgetTree state"), Result.bOk);
	TestTrue(TEXT("P2 chain preview passes"), bPassed);
	TestFalse(TEXT("P2 chain preview is not blocked"), bBlocked);
	TestEqual(TEXT("remove_root promotes wrapper as planned root"), PlannedRootName, FString(TEXT("WrapperPanel")));
	TestEqual(TEXT("replace_widget_class updates planned wrapper class"), PlannedRootClass, FString(TEXT("Border")));
	TestTrue(TEXT("duplicate subtree copy remains in planned index"), bHasHeaderCopy);
	TestTrue(TEXT("wrapper remains in planned index"), bHasWrapperPanel);
	TestFalse(TEXT("removed original widget is absent from planned index"), bHasOriginalHeader);
	TestFalse(TEXT("removed original root is absent from planned index"), bHasOriginalRoot);
	TestNull(TEXT("preview does not mutate real root widget"), WidgetBlueprint->WidgetTree->RootWidget);
	TestFalse(TEXT("preview does not dirty package"), WidgetBlueprint->GetOutermost()->IsDirty());
	return Result.bOk &&
		bPassed &&
		!bBlocked &&
		PlannedRootName == TEXT("WrapperPanel") &&
		PlannedRootClass == TEXT("Border") &&
		bHasHeaderCopy &&
		bHasWrapperPanel &&
		!bHasOriginalHeader &&
		!bHasOriginalRoot &&
		WidgetBlueprint->WidgetTree->RootWidget == nullptr;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperUMGWidgetPreviewPlannedStateRejectsImplicitNonPanelRootParentTest,
	"BlueprintHelper.TaskRuntime.UMGWidgetPreview.PlannedStateRejectsImplicitNonPanelRootParent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperUMGWidgetPreviewPlannedStateRejectsImplicitNonPanelRootParentTest::RunTest(const FString& Parameters)
{
	UWidgetBlueprint* WidgetBlueprint =
		FBlueprintHelperUMGWidgetPreviewPlannedStateTestsLocalUtils::MakeEmptyWidgetBlueprint();
	TestNotNull(TEXT("empty WidgetBlueprint fixture exists"), WidgetBlueprint);
	TestNotNull(TEXT("empty WidgetBlueprint has a WidgetTree"), WidgetBlueprint ? WidgetBlueprint->WidgetTree.Get() : nullptr);
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return false;
	}

	const FString AssetPath = WidgetBlueprint->GetPathName();
	FBlueprintHelperUMGWidgetPreviewPlannedStateTestsLocalUtils::FRuntimeServices Services;
	const FBlueprintHelperToolResultBase Result = Services.RuntimeService.PreviewTaskPlan(
		FBlueprintHelperUMGWidgetPreviewPlannedStateTestsLocalUtils::MakeImplicitRootParentMismatchPayload(AssetPath));

	bool bPassed = true;
	bool bBlocked = false;
	FString DryRunResult;
	if (Result.Data.IsValid())
	{
		Result.Data->TryGetBoolField(TEXT("passed"), bPassed);
		Result.Data->TryGetBoolField(TEXT("blocked"), bBlocked);
		const TSharedPtr<FJsonObject>* DryRunObject = nullptr;
		if (Result.Data->TryGetObjectField(TEXT("dry_run"), DryRunObject) && DryRunObject && DryRunObject->IsValid())
		{
			(*DryRunObject)->TryGetStringField(TEXT("result"), DryRunResult);
		}
	}

	TestTrue(TEXT("preview request returns dry-run result"), Result.bOk);
	TestFalse(TEXT("preview data rejects implicit child under non-panel root"), bPassed);
	TestTrue(TEXT("preview data is blocked"), bBlocked);
	TestEqual(TEXT("dry-run result is blocked"), DryRunResult, FString(TEXT("blocked")));
	TestEqual(TEXT("preview status remains dry-run"), static_cast<int32>(Result.Status), static_cast<int32>(EBlueprintHelperToolStatus::DryRun));
	TestNull(TEXT("failed preview does not mutate real root widget"), WidgetBlueprint->WidgetTree->RootWidget);
	TestFalse(TEXT("failed preview does not dirty package"), WidgetBlueprint->GetOutermost()->IsDirty());
	return Result.bOk && !bPassed && bBlocked && WidgetBlueprint->WidgetTree->RootWidget == nullptr;
}

#endif
