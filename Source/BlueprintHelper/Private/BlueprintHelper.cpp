// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlueprintHelper.h"

#include "BlueprintEditor.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NodeHandlers/BlueprintNodeHandler.h"
#include "NodeHandlers/BranchNodeHandler.h"
#include "NodeHandlers/CallFunctionNodeHandler.h"
#include "NodeHandlers/MacroInstanceNodeHandler.h"
#include "NodeHandlers/SequenceNodeHandler.h"
#include "NodeHandlers/VariableGetNodeHandler.h"
#include "NodeHandlers/VariableSetNodeHandler.h"
#include "NodeHandlers/CustomEventNodeHandler.h"
#include "NodeHandlers/EventNodeHandler.h"
#include "NodeHandlers/CallDelegateNodeHandler.h"
#include "NodeHandlers/AddDelegateNodeHandler.h"
#include "NodeHandlers/RemoveDelegateNodeHandler.h"
#include "NodeHandlers/ClearDelegateNodeHandler.h"
#include "NodeHandlers/AssignDelegateNodeHandler.h"
#include "NodeHandlers/CreateDelegateNodeHandler.h"
#include "NodeHandlers/MakeContainerNodeHandler.h"
#include "NodeHandlers/StructOperationNodeHandler.h"
#include "NodeHandlers/SelfNodeHandler.h"
#include "NodeHandlers/DynamicCastNodeHandler.h"
#include "NodeHandlers/SpawnActorNodeHandler.h"
#include "NodeHandlers/FormatTextNodeHandler.h"
#include "NodeHandlers/GetArrayItemNodeHandler.h"
#include "NodeHandlers/TimelineNodeHandler.h"
#include "NodeHandlers/KnotNodeHandler.h"
#include "NodeHandlers/LiteralNodeHandler.h"
#include "NodeHandlers/EnumNameNodeHandler.h"
#include "NodeHandlers/ComponentBoundEventNodeHandler.h"
#include "NodeHandlers/EnhancedInputActionNodeHandler.h"
#include "NodeHandlers/PromotableOperatorNodeHandler.h"
#include "NodeHandlers/CommutativeAssociativeBinaryOperatorNodeHandler.h"
#include "NodeHandlers/SwitchNodeHandler.h"
#include "NodeHandlers/SelectNodeHandler.h"
#include "OperationHandlers/BlueprintOperationHandler.h"
#include "OperationHandlers/AddMemberVariableHandler.h"
#include "OperationHandlers/AddFunctionGraphHandler.h"
#include "OperationHandlers/AddEventDispatcherHandler.h"
#include "OperationHandlers/AddMacroGraphHandler.h"
#include "OperationHandlers/RemoveGraphHandler.h"
#include "OperationHandlers/RemoveMemberVariableHandler.h"
#include "SHelperMainWidget.h"
#include "Services/BlueprintHelperGraphResolver.h"
#include "Services/BlueprintHelperValidationService.h"
#include "Services/BlueprintHelperExportService.h"
#include "Services/BlueprintHelperImportService.h"
#include "Services/BlueprintHelperAgentImportService.h"
#include "Services/BlueprintHelperCompileService.h"
#include "Services/BlueprintHelperContextService.h"
#include "Services/BlueprintHelperAssetBrowseService.h"
#include "Services/BlueprintHelperBlueprintStructureService.h"
#include "Services/BlueprintHelperWidgetService.h"
#include "Services/BlueprintHelperPropertyReflectionService.h"
#include "Services/BlueprintHelperDataTableService.h"
#include "Services/BlueprintHelperEditorCommandService.h"
#include "Services/BlueprintHelperRuntimeProfileService.h"
#include "Services/BlueprintHelperDiagnosticsService.h"
#include "Services/BlueprintHelperLogicMdReadService.h"
#include "Services/BlueprintHelperLogicJsonReadService.h"
#include "Services/BlueprintHelperAssetFactoryService.h"
#include "Services/BlueprintHelperComponentService.h"
#include "Services/BlueprintHelperClassSettingsService.h"
#include "Services/BlueprintHelperAppendBlueprintGraphService.h"
#include "Services/BlueprintHelperReplaceBlueprintGraphService.h"
#include "Services/BlueprintHelperPatchBlueprintGraphService.h"
#include "Services/BlueprintHelperMergeBlueprintGraphService.h"
#include "Services/BlueprintHelperCleanupBlueprintHelperBlockService.h"
#include "Services/BlueprintHelperRollbackCleanupTransactionService.h"
#include "Services/BlueprintHelperConvertBlockToUserOwnedService.h"
#include "Services/BlueprintHelperCompileAssetService.h"
#include "Services/BlueprintHelperTransactionQueryService.h"
#include "Services/BlueprintHelperBlockIdService.h"
#include "Services/BlueprintHelperOwnershipService.h"
#include "Services/BlueprintHelperTransactionJournalService.h"
#include "Services/BlueprintHelperGraphSnapshotService.h"
#include "Services/BlueprintHelperLogicJsonPathService.h"
#include "Bridge/BlueprintHelperBridgeRouter.h"
#include "Bridge/BlueprintHelperBridgeServer.h"
#include "Styling/AppStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FBlueprintHelperModule"

DEFINE_LOG_CATEGORY_STATIC(LogBlueprintHelperEditor, Log, All);

const FName FBlueprintHelperModule::HelperTabName(TEXT("BlueprintHelper.MainWindow"));

FBlueprintHelperModule::FBlueprintHelperModule() = default;
FBlueprintHelperModule::~FBlueprintHelperModule() = default;

FBlueprintHelperModule& FBlueprintHelperModule::Get()
{
	return FModuleManager::LoadModuleChecked<FBlueprintHelperModule>(TEXT("BlueprintHelper"));
}

void FBlueprintHelperModule::StartupModule()
{
	UE_LOG(LogBlueprintHelperEditor, Log, TEXT("BlueprintHelper StartupModule begin."));

	FBlueprintNodeHandlerRegistry& Registry = FBlueprintNodeHandlerRegistry::Get();
	Registry.Register(MakeShared<FCallFunctionNodeHandler>());
	Registry.Register(MakeShared<FVariableGetNodeHandler>());
	Registry.Register(MakeShared<FVariableSetNodeHandler>());
	Registry.Register(MakeShared<FMacroInstanceNodeHandler>());
	Registry.Register(MakeShared<FBranchNodeHandler>());
	Registry.Register(MakeShared<FSequenceNodeHandler>());
	Registry.Register(MakeShared<FCustomEventNodeHandler>());
	Registry.Register(MakeShared<FEventNodeHandler>());
	Registry.Register(MakeShared<FCallDelegateNodeHandler>());
	Registry.Register(MakeShared<FAddDelegateNodeHandler>());
	Registry.Register(MakeShared<FRemoveDelegateNodeHandler>());
	Registry.Register(MakeShared<FClearDelegateNodeHandler>());
	Registry.Register(MakeShared<FAssignDelegateNodeHandler>());
	Registry.Register(MakeShared<FCreateDelegateNodeHandler>());
	Registry.Register(MakeShared<FMakeContainerNodeHandler>());
	Registry.Register(MakeShared<FStructOperationNodeHandler>());
	Registry.Register(MakeShared<FSelfNodeHandler>());
	Registry.Register(MakeShared<FDynamicCastNodeHandler>());
	Registry.Register(MakeShared<FSpawnActorNodeHandler>());
	Registry.Register(MakeShared<FFormatTextNodeHandler>());
	Registry.Register(MakeShared<FGetArrayItemNodeHandler>());
	Registry.Register(MakeShared<FTimelineNodeHandler>());
	// v2.3 — 全覆盖收尾
	Registry.Register(MakeShared<FKnotNodeHandler>());
	Registry.Register(MakeShared<FLiteralNodeHandler>());
	Registry.Register(MakeShared<FEnumNameNodeHandler>());
	Registry.Register(MakeShared<FComponentBoundEventNodeHandler>());
	// v2.9 — Enhanced Input / 数学运算 / 流程控制
	Registry.Register(MakeShared<FEnhancedInputActionNodeHandler>());
	Registry.Register(MakeShared<FPromotableOperatorNodeHandler>());
	Registry.Register(MakeShared<FCommutativeAssociativeBinaryOperatorNodeHandler>());
	Registry.Register(MakeShared<FSwitchNodeHandler>());
	Registry.Register(MakeShared<FSelectNodeHandler>());

	FBlueprintOperationHandlerRegistry& OpRegistry = FBlueprintOperationHandlerRegistry::Get();
	OpRegistry.Register(MakeShared<FAddMemberVariableHandler>());
	OpRegistry.Register(MakeShared<FAddFunctionGraphHandler>());
	OpRegistry.Register(MakeShared<FAddEventDispatcherHandler>());
	OpRegistry.Register(MakeShared<FAddMacroGraphHandler>());
	OpRegistry.Register(MakeShared<FRemoveGraphHandler>());
	OpRegistry.Register(MakeShared<FRemoveMemberVariableHandler>());

	// ─── Service Layer 初始化 ───
	GraphResolver    = MakeUnique<FBlueprintHelperGraphResolver>();
	ValidationService = MakeUnique<FBlueprintHelperValidationService>();
	ExportService    = MakeUnique<FBlueprintHelperExportService>(*GraphResolver);
	CompileService   = MakeUnique<FBlueprintHelperCompileService>(*GraphResolver);
	ImportService    = MakeUnique<FBlueprintHelperImportService>(*GraphResolver, *ValidationService);
	ImportService->SetCompileService(CompileService.Get());
	AssetBrowseService = MakeUnique<FBlueprintHelperAssetBrowseService>();
	AgentImportService = MakeUnique<FBlueprintHelperAgentImportService>(*GraphResolver, *CompileService, *AssetBrowseService);
	StructureService = MakeUnique<FBlueprintHelperBlueprintStructureService>(*GraphResolver);
	WidgetService  = MakeUnique<FBlueprintHelperWidgetService>();
	PropertyReflectionService = MakeUnique<FBlueprintHelperPropertyReflectionService>();
	DataTableService = MakeUnique<FBlueprintHelperDataTableService>();
	EditorCommandService = MakeUnique<FBlueprintHelperEditorCommandService>();
	RuntimeProfileService = MakeUnique<FBlueprintHelperRuntimeProfileService>();
	DiagnosticsService = MakeUnique<FBlueprintHelperDiagnosticsService>();
	LogicMdReadService = MakeUnique<FBlueprintHelperLogicMdReadService>();
	LogicJsonReadService = MakeUnique<FBlueprintHelperLogicJsonReadService>();
	AssetFactoryService = MakeUnique<FBlueprintHelperAssetFactoryService>();
	ComponentService = MakeUnique<FBlueprintHelperComponentService>(*GraphResolver);
	ClassSettingsService = MakeUnique<FBlueprintHelperClassSettingsService>(*GraphResolver);

	BlockIdService = MakeUnique<FBlueprintHelperBlockIdService>();
	OwnershipService = MakeUnique<FBlueprintHelperOwnershipService>();
	JournalService = MakeUnique<FBlueprintHelperTransactionJournalService>();
	SnapshotService = MakeUnique<FBlueprintHelperGraphSnapshotService>();
	AppendGraphService = MakeUnique<FBlueprintHelperAppendBlueprintGraphService>(
		*GraphResolver, *AgentImportService, *BlockIdService, *OwnershipService, *JournalService);
	ReplaceGraphService = MakeUnique<FBlueprintHelperReplaceBlueprintGraphService>(
		*GraphResolver, *AgentImportService, *BlockIdService, *OwnershipService, *JournalService, *SnapshotService);
	LogicJsonPathService = MakeUnique<FBlueprintHelperLogicJsonPathService>();
	PatchGraphService = MakeUnique<FBlueprintHelperPatchBlueprintGraphService>(
		*GraphResolver, *LogicJsonPathService, *JournalService);
	MergeGraphService = MakeUnique<FBlueprintHelperMergeBlueprintGraphService>(
		*GraphResolver, *LogicJsonPathService, *JournalService);
	CleanupBlockService = MakeUnique<FBlueprintHelperCleanupBlueprintHelperBlockService>(
		*GraphResolver, *JournalService);
	RollbackCleanupService = MakeUnique<FBlueprintHelperRollbackCleanupTransactionService>(
		*GraphResolver, *JournalService);
	ConvertBlockService = MakeUnique<FBlueprintHelperConvertBlockToUserOwnedService>(
		*GraphResolver, *OwnershipService, *JournalService);
	CompileAssetService = MakeUnique<FBlueprintHelperCompileAssetService>(*CompileService);
	TransactionQueryService = MakeUnique<FBlueprintHelperTransactionQueryService>();

	// ─── Bridge Layer 初始化 ───
	ContextService = MakeUnique<FBlueprintHelperContextService>(*GraphResolver);
	BridgeRouter = MakeUnique<FBlueprintHelperBridgeRouter>(
		*ImportService, *AgentImportService, *ExportService, *CompileService, *ValidationService, *ContextService, *AssetBrowseService, *StructureService, *WidgetService, *PropertyReflectionService, *DataTableService, *EditorCommandService, *RuntimeProfileService, *DiagnosticsService, *LogicMdReadService, *LogicJsonReadService, *AssetFactoryService, *ComponentService, *ClassSettingsService, *AppendGraphService, *ReplaceGraphService, *PatchGraphService, *MergeGraphService, *CleanupBlockService, *RollbackCleanupService, *ConvertBlockService, *CompileAssetService, *TransactionQueryService);
	BridgeServer = MakeUnique<FBlueprintHelperBridgeServer>(*BridgeRouter);
	BridgeServer->Start();

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(HelperTabName, FOnSpawnTab::CreateRaw(this, &FBlueprintHelperModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("BlueprintHelperTabTitle", "Blueprint Helper"))
		.SetTooltipText(LOCTEXT("BlueprintHelperTabTooltip", "打开 Blueprint Helper 工具窗口。"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsMiscCategory())
		.SetMenuType(ETabSpawnerMenuType::Enabled);

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FBlueprintHelperModule::RegisterMenus));
	UE_LOG(LogBlueprintHelperEditor, Log, TEXT("BlueprintHelper StartupModule end."));
}

void FBlueprintHelperModule::ShutdownModule()
{
	// ─── Bridge Layer 销毁 ───
	if (BridgeServer) { BridgeServer->Shutdown(); }
	BridgeServer.Reset();
	BridgeRouter.Reset();
	ContextService.Reset();

	// ─── Service Layer 销毁（逆序）───
	EditorCommandService.Reset();
	ClassSettingsService.Reset();
	ComponentService.Reset();
	AssetFactoryService.Reset();
	LogicJsonReadService.Reset();
	LogicMdReadService.Reset();
	DiagnosticsService.Reset();
	RuntimeProfileService.Reset();
	DataTableService.Reset();
	PropertyReflectionService.Reset();
	StructureService.Reset();
	WidgetService.Reset();
	AgentImportService.Reset();
	AssetBrowseService.Reset();
	ImportService.Reset();
	CompileService.Reset();
	ExportService.Reset();
	ValidationService.Reset();
	GraphResolver.Reset();

	FBlueprintNodeHandlerRegistry::Get().Reset();
	FBlueprintOperationHandlerRegistry::Get().Reset();

	if (UToolMenus::TryGet())
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
	}

	if (FModuleManager::Get().IsModuleLoaded(TEXT("WorkspaceMenuStructure")))
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(HelperTabName);
	}
}

void FBlueprintHelperModule::OpenMainWindow()
{
	UE_LOG(LogBlueprintHelperEditor, Log, TEXT("BlueprintHelper OpenMainWindow invoked."));
	FGlobalTabmanager::Get()->TryInvokeTab(HelperTabName);
}

UEdGraph* FBlueprintHelperModule::GetActiveBlueprintGraph() const
{
	if (!GEditor)
	{
		return nullptr;
	}

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem)
	{
		return nullptr;
	}

	const TArray<UObject*> EditedAssets = AssetEditorSubsystem->GetAllEditedAssets();
	for (UObject* Asset : EditedAssets)
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
		if (!Blueprint)
		{
			continue;
		}

		IAssetEditorInstance* EditorInstance = AssetEditorSubsystem->FindEditorForAsset(Blueprint, false);
		if (!EditorInstance || EditorInstance->GetEditorName() != TEXT("BlueprintEditor"))
		{
			continue;
		}

		FBlueprintEditor* BlueprintEditor = static_cast<FBlueprintEditor*>(EditorInstance);
		if (!BlueprintEditor)
		{
			continue;
		}

		if (UEdGraph* FocusedGraph = BlueprintEditor->GetFocusedGraph())
		{
			return FocusedGraph;
		}
	}

	return nullptr;
}

FString FBlueprintHelperModule::GetJsonToBlueprintRuleMarkdown() const
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BlueprintHelper"));
	if (!Plugin.IsValid())
	{
		return TEXT("");
	}

	const FString RuleFilePath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/JsonToBlueprintRules.md"));
	if (!IFileManager::Get().FileExists(*RuleFilePath))
	{
		return TEXT("");
	}

	FString RuleMarkdown;
	if (!FFileHelper::LoadFileToString(RuleMarkdown, *RuleFilePath))
	{
		return TEXT("");
	}

	return RuleMarkdown;
}

void FBlueprintHelperModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	UE_LOG(LogBlueprintHelperEditor, Log, TEXT("BlueprintHelper RegisterMenus invoked."));
	RegisterLevelEditorMenus();
	RegisterBlueprintEditorToolbar();
	UToolMenus::Get()->RefreshAllWidgets();
}

void FBlueprintHelperModule::RegisterLevelEditorMenus()
{
	UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window"));
	FToolMenuSection& WindowSection = WindowMenu->FindOrAddSection(TEXT("WindowLayout"));
	WindowSection.AddMenuEntry(
		TEXT("OpenBlueprintHelper"),
		LOCTEXT("BlueprintHelperMenuLabel", "Blueprint Helper"),
		LOCTEXT("BlueprintHelperMenuTooltip", "打开 Blueprint Helper 工具窗口。"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FBlueprintHelperModule::OpenMainWindow)));

	UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
	FToolMenuSection& ToolsSection = ToolsMenu->FindOrAddSection(TEXT("BlueprintHelper"));
	ToolsSection.AddMenuEntry(
		TEXT("OpenBlueprintHelperFromTools"),
		LOCTEXT("BlueprintHelperToolsMenuLabel", "Blueprint Helper"),
		LOCTEXT("BlueprintHelperToolsMenuTooltip", "打开 Blueprint Helper 工具窗口。"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FBlueprintHelperModule::OpenMainWindow)));

}

void FBlueprintHelperModule::RegisterBlueprintEditorToolbar()
{
	UToolMenu* BlueprintToolbarMenu = UToolMenus::Get()->ExtendMenu(TEXT("AssetEditor.BlueprintEditor.ToolBar"));
	FToolMenuSection& BlueprintToolbarSection = BlueprintToolbarMenu->FindOrAddSection(TEXT("Asset"));

	FToolMenuEntry ToolbarEntry = FToolMenuEntry::InitToolBarButton(
		TEXT("OpenBlueprintHelperToolbar"),
		FUIAction(FExecuteAction::CreateRaw(this, &FBlueprintHelperModule::OpenMainWindow)),
		LOCTEXT("BlueprintHelperToolbarLabel", "Blueprint Helper"),
		LOCTEXT("BlueprintHelperToolbarTooltip", "打开 Blueprint Helper 工具窗口。"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Kismet.Tabs.Palette")));
	ToolbarEntry.StyleNameOverride = TEXT("CalloutToolbar");
	BlueprintToolbarSection.AddEntry(ToolbarEntry);

}

TSharedRef<SDockTab> FBlueprintHelperModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SHelperMainWidget)
			.ImportService(ImportService.Get())
			.GraphResolver(GraphResolver.Get())
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBlueprintHelperModule, BlueprintHelper)
