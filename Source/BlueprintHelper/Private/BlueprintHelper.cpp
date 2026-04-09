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
#include "OperationHandlers/BlueprintOperationHandler.h"
#include "OperationHandlers/AddMemberVariableHandler.h"
#include "OperationHandlers/AddFunctionGraphHandler.h"
#include "OperationHandlers/AddEventDispatcherHandler.h"
#include "SHelperMainWidget.h"
#include "Styling/AppStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FBlueprintHelperModule"

DEFINE_LOG_CATEGORY_STATIC(LogBlueprintHelperEditor, Log, All);

const FName FBlueprintHelperModule::HelperTabName(TEXT("BlueprintHelper.MainWindow"));

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

	FBlueprintOperationHandlerRegistry& OpRegistry = FBlueprintOperationHandlerRegistry::Get();
	OpRegistry.Register(MakeShared<FAddMemberVariableHandler>());
	OpRegistry.Register(MakeShared<FAddFunctionGraphHandler>());
	OpRegistry.Register(MakeShared<FAddEventDispatcherHandler>());

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
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBlueprintHelperModule, BlueprintHelper)