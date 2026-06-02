// Copyright Epic Games, Inc. All Rights Reserved.

#include "Entry/BlueprintHelper.h"

#include "BlueprintEditor.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UI/SHelperMainWidget.h"
#include "UI/SBlueprintHelperMainWindow.h"
#include "UI/Review/BlueprintHelperReviewDebugBundleService.h"
#include "UI/Review/SBlueprintHelperReviewPanel.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/Debug/BlueprintHelperValidationService.h"
#include "Shared/Services/BlueprintHelperExportService.h"
#include "Shared/Services/BlueprintHelperImportService.h"
#include "Systems/Debug/BlueprintHelperCompileService.h"
#include "Systems/Debug/BlueprintHelperContextService.h"
#include "Systems/Debug/BlueprintHelperAssetBrowseService.h"
#include "Shared/Services/BlueprintHelperBlueprintStructureService.h"
#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetService.h"
#include "Systems/ToolClusters/ObjectProperty/BlueprintHelperPropertyReflectionService.h"
#include "Systems/ToolClusters/DataTable/BlueprintHelperDataTableService.h"
#include "Systems/Debug/BlueprintHelperEditorCommandService.h"
#include "Systems/Debug/BlueprintHelperRuntimeProfileService.h"
#include "Systems/Debug/BlueprintHelperDiagnosticsService.h"
#include "Systems/Debug/BlueprintHelperDebugCaseStoreService.h"
#include "Systems/Debug/BlueprintHelperDebugEntryService.h"
#include "Systems/Debug/BlueprintHelperEditorFocusService.h"
#include "Systems/Debug/BlueprintHelperScreenshotCaptureService.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicMdReadService.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonReadService.h"
#include "Systems/ToolClusters/AssetDiscovery/BlueprintHelperAssetDiscoveryService.h"
#include "Systems/ToolClusters/AssetFactory/BlueprintHelperAssetFactoryService.h"
#include "Systems/ToolClusters/BlueprintComponent/BlueprintHelperComponentService.h"
#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassSettingsService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperAppendBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperGraphWriteServiceRegistry.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperMergeExternalFlowService.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperPatchExternalGraphService.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalBodySnapshotService.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalDependentsAnalysisService.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperReplaceExternalBodyService.h"
#include "Systems/Debug/BlueprintHelperCompileAssetService.h"
#include "Systems/Review/BlueprintHelperReviewActionService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Systems/ToolClusters/BlueprintVariables/BlueprintHelperBlueprintVariableService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperBlockIdService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphSnapshotService.h"
#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicJsonPathService.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutCoordinator.h"
#include "Entry/Bridge/BlueprintHelperBridgeRouter.h"
#include "Entry/Bridge/BlueprintHelperBridgeServer.h"
#include "Entry/Bridge/BlueprintHelperBridgeRuntimeConfigResolver.h"
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
	FBlueprintHelperGraphLayoutCoordinator::Startup();


	// ─── Service Layer 初始。───
	GraphResolver    = MakeUnique<FBlueprintHelperGraphResolver>();
	ValidationService = MakeUnique<FBlueprintHelperValidationService>();
	ExportService    = MakeUnique<FBlueprintHelperExportService>(*GraphResolver);
	CompileService   = MakeUnique<FBlueprintHelperCompileService>(*GraphResolver);
	ImportService    = MakeUnique<FBlueprintHelperImportService>(*GraphResolver, *ValidationService);
	ImportService->SetCompileService(CompileService.Get());
	AssetBrowseService = MakeUnique<FBlueprintHelperAssetBrowseService>();
	StructureService = MakeUnique<FBlueprintHelperBlueprintStructureService>(*GraphResolver);
	WidgetService  = MakeUnique<FBlueprintHelperWidgetService>();
	PropertyReflectionService = MakeUnique<FBlueprintHelperPropertyReflectionService>();
	DataTableService = MakeUnique<FBlueprintHelperDataTableService>();
	EditorCommandService = MakeUnique<FBlueprintHelperEditorCommandService>();
	RuntimeProfileService = MakeUnique<FBlueprintHelperRuntimeProfileService>([this]()
	{
		return IsBridgeServerRunning();
	});
	DiagnosticsService = MakeUnique<FBlueprintHelperDiagnosticsService>();
	DebugCaseStoreService = MakeUnique<FBlueprintHelperDebugCaseStoreService>();
	ReviewStoreService = MakeUnique<FBlueprintHelperReviewStoreService>();
	DebugEntryService = MakeUnique<FBlueprintHelperDebugEntryService>(*DebugCaseStoreService, ReviewStoreService.Get());
	LogicMdReadService = MakeUnique<FBlueprintHelperLogicMdReadService>(*ExportService);
	LogicJsonReadService = MakeUnique<FBlueprintHelperLogicJsonReadService>(*ExportService);
	AssetDiscoveryService = MakeUnique<FBlueprintHelperAssetDiscoveryService>();
	AssetFactoryService = MakeUnique<FBlueprintHelperAssetFactoryService>();
	ComponentService = MakeUnique<FBlueprintHelperComponentService>(*GraphResolver);
	ClassSettingsService = MakeUnique<FBlueprintHelperClassSettingsService>(*GraphResolver);

	BlockIdService = MakeUnique<FBlueprintHelperBlockIdService>();
	OwnershipService = MakeUnique<FBlueprintHelperOwnershipService>();
	SnapshotService = MakeUnique<FBlueprintHelperGraphSnapshotService>();
	AppendGraphService = MakeUnique<FBlueprintHelperAppendBlueprintGraphService>(
		*GraphResolver, *BlockIdService, *OwnershipService);
	ReplaceGraphService = MakeUnique<FBlueprintHelperReplaceBlueprintGraphService>(
		*GraphResolver, *BlockIdService, *OwnershipService, *SnapshotService);
	LogicJsonPathService = MakeUnique<FBlueprintHelperLogicJsonPathService>();
	PatchGraphService = MakeUnique<FBlueprintHelperPatchBlueprintGraphService>(
		*GraphResolver, *LogicJsonPathService);
	MergeGraphService = MakeUnique<FBlueprintHelperMergeBlueprintGraphService>(
		*GraphResolver, *LogicJsonPathService);
	MergeExternalFlowService = MakeUnique<FBlueprintHelperMergeExternalFlowService>(
		*GraphResolver, *BlockIdService, *OwnershipService, *LogicJsonPathService);
	PatchExternalGraphService = MakeUnique<FBlueprintHelperPatchExternalGraphService>();
	ExternalBodySnapshotService = MakeUnique<FBlueprintHelperExternalBodySnapshotService>();
	ExternalDependentsAnalysisService = MakeUnique<FBlueprintHelperExternalDependentsAnalysisService>();
	ReplaceExternalBodyService = MakeUnique<FBlueprintHelperReplaceExternalBodyService>(
		*BlockIdService,
		*OwnershipService,
		*ExternalBodySnapshotService,
		*ExternalDependentsAnalysisService);
	GraphWriteServiceRegistry = MakeUnique<FBlueprintHelperGraphWriteServiceRegistry>();
	GraphWriteServiceRegistry->RegisterHandler(
		TEXT("append_blueprint_graph"),
		[this](const TSharedRef<FJsonObject>& Payload)
		{
			return AppendGraphService->Execute(Payload);
		});
	GraphWriteServiceRegistry->RegisterHandler(
		TEXT("replace_blueprint_graph"),
		[this](const TSharedRef<FJsonObject>& Payload)
		{
			return ReplaceGraphService->Execute(Payload);
		});
	GraphWriteServiceRegistry->RegisterHandler(
		TEXT("patch_blueprint_graph"),
		[this](const TSharedRef<FJsonObject>& Payload)
		{
			return PatchGraphService->Execute(Payload);
		});
	GraphWriteServiceRegistry->RegisterHandler(
		TEXT("merge_blueprint_graph"),
		[this](const TSharedRef<FJsonObject>& Payload)
		{
			return MergeGraphService->Execute(Payload);
		});
	GraphWriteServiceRegistry->RegisterHandler(
		TEXT("merge_external_flow"),
		[this](const TSharedRef<FJsonObject>& Payload)
		{
			return MergeExternalFlowService->Execute(Payload);
		});
	GraphWriteServiceRegistry->RegisterHandler(
		TEXT("patch_external_graph"),
		[this](const TSharedRef<FJsonObject>& Payload)
		{
			return PatchExternalGraphService->Execute(Payload);
		});
	GraphWriteServiceRegistry->RegisterHandler(
		TEXT("replace_external_body"),
		[this](const TSharedRef<FJsonObject>& Payload)
		{
			return ReplaceExternalBodyService->Execute(Payload);
		});
	CompileAssetService = MakeUnique<FBlueprintHelperCompileAssetService>(*CompileService, DebugEntryService.Get());
	VariableService = MakeUnique<FBlueprintHelperBlueprintVariableService>(*GraphResolver, *StructureService);
	ReviewActionService = MakeUnique<FBlueprintHelperReviewActionService>(DebugEntryService.Get());
	EditorFocusService = MakeUnique<FBlueprintHelperEditorFocusService>(
		*AssetBrowseService,
		*GraphResolver,
		*BlockIdService,
		*LogicJsonPathService);
	ScreenshotCaptureService = MakeUnique<FBlueprintHelperScreenshotCaptureService>();

	// ─── Bridge Layer 初始。───
	ContextService = MakeUnique<FBlueprintHelperContextService>(*GraphResolver);
	BridgeRouter = MakeUnique<FBlueprintHelperBridgeRouter>(
		*ExportService, *CompileService, *ValidationService, *ContextService, *AssetBrowseService, *AssetDiscoveryService, *StructureService, *WidgetService, *PropertyReflectionService, *DataTableService, *EditorCommandService, *RuntimeProfileService, *DiagnosticsService, *DebugEntryService, *LogicMdReadService, *LogicJsonReadService, *AssetFactoryService, *ComponentService, *ClassSettingsService, *GraphWriteServiceRegistry, *CompileAssetService, *VariableService, *ReviewStoreService, *EditorFocusService, *ScreenshotCaptureService);
	const FBlueprintHelperBridgeRuntimeConfig BridgeRuntimeConfig = FBlueprintHelperBridgeRuntimeConfigResolver::Load();
	BridgeServer = MakeUnique<FBlueprintHelperBridgeServer>(*BridgeRouter, BridgeRuntimeConfig, DebugEntryService.Get());
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
	SBlueprintHelperMainWindow::ShutdownCleanupTasks();
	SBlueprintHelperReviewPanel::ShutdownAsyncTasks();
	FBlueprintHelperReviewDebugBundleService::ShutdownAsyncWrites();

	// ─── Bridge Layer 销。───
	if (BridgeServer) { BridgeServer->Shutdown(); }
	FBlueprintHelperGraphLayoutCoordinator::Shutdown();
	BridgeServer.Reset();
	BridgeRouter.Reset();
	ContextService.Reset();
	ScreenshotCaptureService.Reset();
	EditorFocusService.Reset();

	// ─── Service Layer 销毁（逆序）───
	ReviewActionService.Reset();
	VariableService.Reset();
	CompileAssetService.Reset();
	GraphWriteServiceRegistry.Reset();
	ReplaceExternalBodyService.Reset();
	ExternalDependentsAnalysisService.Reset();
	ExternalBodySnapshotService.Reset();
	PatchExternalGraphService.Reset();
	MergeExternalFlowService.Reset();
	MergeGraphService.Reset();
	PatchGraphService.Reset();
	LogicJsonPathService.Reset();
	ReplaceGraphService.Reset();
	AppendGraphService.Reset();
	SnapshotService.Reset();
	OwnershipService.Reset();
	BlockIdService.Reset();
	DebugEntryService.Reset();
	ReviewStoreService.Reset();
	EditorCommandService.Reset();
	ClassSettingsService.Reset();
	ComponentService.Reset();
	AssetFactoryService.Reset();
	AssetDiscoveryService.Reset();
	LogicJsonReadService.Reset();
	LogicMdReadService.Reset();
	DiagnosticsService.Reset();
	DebugCaseStoreService.Reset();
	RuntimeProfileService.Reset();
	DataTableService.Reset();
	PropertyReflectionService.Reset();
	StructureService.Reset();
	WidgetService.Reset();
	AssetBrowseService.Reset();
	ImportService.Reset();
	CompileService.Reset();
	ExportService.Reset();
	ValidationService.Reset();
	GraphResolver.Reset();


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

bool FBlueprintHelperModule::IsBridgeServerRunning() const
{
	return BridgeServer.IsValid() && BridgeServer->IsRunning();
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
			SNew(SBlueprintHelperMainWindow)
			.ImportService(ImportService.Get())
			.GraphResolver(GraphResolver.Get())
			.ReviewStoreService(ReviewStoreService.Get())
			.ReviewActionService(ReviewActionService.Get())
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBlueprintHelperModule, BlueprintHelper)
