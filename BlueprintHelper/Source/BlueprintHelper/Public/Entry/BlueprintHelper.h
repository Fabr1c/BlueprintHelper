// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class SDockTab;
class UEdGraph;
class FSpawnTabArgs;
class FBlueprintHelperGraphResolver;
class FBlueprintHelperValidationService;
class FBlueprintHelperExportService;
class FBlueprintHelperImportService;
class FBlueprintHelperCompileService;
class FBlueprintHelperContextService;
class FBlueprintHelperBridgeRouter;
class FBlueprintHelperBridgeServer;
class FBlueprintHelperAssetBrowseService;
class FBlueprintHelperBlueprintStructureService;
class FBlueprintHelperWidgetService;
class FBlueprintHelperPropertyReflectionService;
class FBlueprintHelperDataTableService;
class FBlueprintHelperEditorCommandService;
class FBlueprintHelperRuntimeProfileService;
class FBlueprintHelperDiagnosticsService;
class FBlueprintHelperDebugCaseStoreService;
class FBlueprintHelperDebugEntryService;
class FBlueprintHelperLogicMdReadService;
class FBlueprintHelperLogicJsonReadService;
class FBlueprintHelperAssetDiscoveryService;
class FBlueprintHelperAssetFactoryService;
class FBlueprintHelperComponentService;
class FBlueprintHelperClassSettingsService;
class FBlueprintHelperAppendBlueprintGraphService;
class FBlueprintHelperReplaceBlueprintGraphService;
class FBlueprintHelperPatchBlueprintGraphService;
class FBlueprintHelperMergeBlueprintGraphService;
class FBlueprintHelperGraphWriteServiceRegistry;
class FBlueprintHelperMergeExternalFlowService;
class FBlueprintHelperPatchExternalGraphService;
class FBlueprintHelperExternalBodySnapshotService;
class FBlueprintHelperExternalDependentsAnalysisService;
class FBlueprintHelperReplaceExternalBodyService;
class FBlueprintHelperCompileAssetService;
class FBlueprintHelperBlueprintVariableService;
class FBlueprintHelperBlockIdService;
class FBlueprintHelperOwnershipService;
class FBlueprintHelperGraphSnapshotService;
class FBlueprintHelperLogicJsonPathService;
class FBlueprintHelperReviewStoreService;
class FBlueprintHelperReviewActionService;

/**
 * BlueprintHelper 模块，负责注册编辑器窗口与提供当前蓝图图表访问能力。
 */
class BLUEPRINTHELPER_API FBlueprintHelperModule : public IModuleInterface
{
public:
	FBlueprintHelperModule();
	~FBlueprintHelperModule() override;

	/** 获取模块实例。 */
	static FBlueprintHelperModule& Get();

	/** 模块启动，注册页签与菜单入口。 */
	virtual void StartupModule() override;

	/** 模块关闭，注销页签与菜单入口。 */
	virtual void ShutdownModule() override;

	/** 打开插件主窗口。 */
	void OpenMainWindow();

	/** 获取当前激活的蓝图图表。 */
	UEdGraph* GetActiveBlueprintGraph() const;

	/** 读取 Json -> 蓝图规则 Markdown。 */
	FString GetJsonToBlueprintRuleMarkdown() const;

	/** 获取 BlueprintHelper Bridge 当前是否运行。 */
	bool IsBridgeServerRunning() const;

	// ─── Service Layer 访问 ───

	const FBlueprintHelperGraphResolver& GetGraphResolver() const { return *GraphResolver; }
	const FBlueprintHelperValidationService& GetValidationService() const { return *ValidationService; }
	const FBlueprintHelperExportService& GetExportService() const { return *ExportService; }
	const FBlueprintHelperImportService& GetImportService() const { return *ImportService; }
	const FBlueprintHelperCompileService& GetCompileService() const { return *CompileService; }
	const FBlueprintHelperAssetBrowseService& GetAssetBrowseService() const { return *AssetBrowseService; }
	const FBlueprintHelperBlueprintStructureService& GetStructureService() const { return *StructureService; }
	const FBlueprintHelperWidgetService& GetWidgetService() const { return *WidgetService; }
	const FBlueprintHelperPropertyReflectionService& GetPropertyReflectionService() const { return *PropertyReflectionService; }
	const FBlueprintHelperDataTableService& GetDataTableService() const { return *DataTableService; }
	const FBlueprintHelperEditorCommandService& GetEditorCommandService() const { return *EditorCommandService; }
	const FBlueprintHelperComponentService& GetComponentService() const { return *ComponentService; }
	const FBlueprintHelperClassSettingsService& GetClassSettingsService() const { return *ClassSettingsService; }

private:
	/** 注册编辑器菜单。 */
	void RegisterMenus();

	/** 注册主编辑器菜单入口。 */
	void RegisterLevelEditorMenus();

	/** 注册蓝图编辑器工具栏入口。 */
	void RegisterBlueprintEditorToolbar();

	/** 生成插件主页签。 */
	TSharedRef<SDockTab> OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs);

private:
	/** 插件主页签名称。 */
	static const FName HelperTabName;

	// ─── Service Layer ───
	TUniquePtr<FBlueprintHelperGraphResolver> GraphResolver;
	TUniquePtr<FBlueprintHelperValidationService> ValidationService;
	TUniquePtr<FBlueprintHelperExportService> ExportService;
	TUniquePtr<FBlueprintHelperImportService> ImportService;
	TUniquePtr<FBlueprintHelperCompileService> CompileService;
	TUniquePtr<FBlueprintHelperAssetBrowseService> AssetBrowseService;
	TUniquePtr<FBlueprintHelperBlueprintStructureService> StructureService;
	TUniquePtr<FBlueprintHelperWidgetService> WidgetService;
	TUniquePtr<FBlueprintHelperPropertyReflectionService> PropertyReflectionService;
	TUniquePtr<FBlueprintHelperDataTableService> DataTableService;
	TUniquePtr<FBlueprintHelperEditorCommandService> EditorCommandService;

	// ─── Runtime Profile ───
	TUniquePtr<FBlueprintHelperRuntimeProfileService> RuntimeProfileService;

	// ─── Diagnostics ───
	TUniquePtr<FBlueprintHelperDiagnosticsService> DiagnosticsService;
	TUniquePtr<FBlueprintHelperDebugCaseStoreService> DebugCaseStoreService;
	TUniquePtr<FBlueprintHelperDebugEntryService> DebugEntryService;

	// ─── Logic MD ───
	TUniquePtr<FBlueprintHelperLogicMdReadService> LogicMdReadService;

	// ─── Logic Json ───
	TUniquePtr<FBlueprintHelperLogicJsonReadService> LogicJsonReadService;

	// ─── Asset Discovery ───
	TUniquePtr<FBlueprintHelperAssetDiscoveryService> AssetDiscoveryService;

	// ─── Asset Factory ───
	TUniquePtr<FBlueprintHelperAssetFactoryService> AssetFactoryService;

	// ─── Component ───
	TUniquePtr<FBlueprintHelperComponentService> ComponentService;

	// ─── Class Settings ───
	TUniquePtr<FBlueprintHelperClassSettingsService> ClassSettingsService;

	// ─── Graph Write Infrastructure ───
	TUniquePtr<FBlueprintHelperBlockIdService> BlockIdService;
	TUniquePtr<FBlueprintHelperOwnershipService> OwnershipService;
	TUniquePtr<FBlueprintHelperGraphSnapshotService> SnapshotService;
	TUniquePtr<FBlueprintHelperAppendBlueprintGraphService> AppendGraphService;
	TUniquePtr<FBlueprintHelperReplaceBlueprintGraphService> ReplaceGraphService;
	TUniquePtr<FBlueprintHelperLogicJsonPathService> LogicJsonPathService;
	TUniquePtr<FBlueprintHelperPatchBlueprintGraphService> PatchGraphService;
	TUniquePtr<FBlueprintHelperMergeBlueprintGraphService> MergeGraphService;
	TUniquePtr<FBlueprintHelperMergeExternalFlowService> MergeExternalFlowService;
	TUniquePtr<FBlueprintHelperPatchExternalGraphService> PatchExternalGraphService;
	TUniquePtr<FBlueprintHelperExternalBodySnapshotService> ExternalBodySnapshotService;
	TUniquePtr<FBlueprintHelperExternalDependentsAnalysisService> ExternalDependentsAnalysisService;
	TUniquePtr<FBlueprintHelperReplaceExternalBodyService> ReplaceExternalBodyService;
	TUniquePtr<FBlueprintHelperGraphWriteServiceRegistry> GraphWriteServiceRegistry;
	TUniquePtr<FBlueprintHelperCompileAssetService> CompileAssetService;
	TUniquePtr<FBlueprintHelperBlueprintVariableService> VariableService;
	TUniquePtr<FBlueprintHelperReviewStoreService> ReviewStoreService;
	TUniquePtr<FBlueprintHelperReviewActionService> ReviewActionService;

	// ─── Bridge Layer ───
	TUniquePtr<FBlueprintHelperContextService> ContextService;
	TUniquePtr<FBlueprintHelperBridgeRouter> BridgeRouter;
	TUniquePtr<FBlueprintHelperBridgeServer> BridgeServer;
};
