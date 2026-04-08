// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlueprintHelper.h"

#include "BlueprintEditor.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SHelperMainWidget.h"
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

	UToolMenus::Get()->RefreshAllWidgets();
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