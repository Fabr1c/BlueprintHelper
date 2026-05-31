// BlueprintHelper Bridge Layer — 命令路由

#pragma once

#include "CoreMinimal.h"
#include "Entry/Bridge/BlueprintHelperBridgeRoutePlanner.h"
#include "Entry/Bridge/Routes/BlueprintHelperAssetDiscoveryBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperAssetFactoryBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperBlueprintVariablesBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperClassSettingsBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperComponentBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperDataTableBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperGraphWriteBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperObjectPropertyBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperUMGWidgetBridgeRoutes.h"
#include "Entry/Bridge/BlueprintHelperBridgeTypes.h"
#include "Shared/Safety/BlueprintHelperDependencyAnalysisService.h"
#include "Shared/FunctionChain/BlueprintHelperFunctionChainContextService.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"

class FBlueprintHelperExportService;
class FBlueprintHelperCompileService;
class FBlueprintHelperValidationService;
class FBlueprintHelperContextService;
class FBlueprintHelperAssetBrowseService;
class FBlueprintHelperAssetDiscoveryService;
class FBlueprintHelperBlueprintStructureService;
class FBlueprintHelperWidgetService;
class FBlueprintHelperPropertyReflectionService;
class FBlueprintHelperDataTableService;
class FBlueprintHelperEditorCommandService;
class FBlueprintHelperRuntimeProfileService;
class FBlueprintHelperDiagnosticsService;
class FBlueprintHelperDebugEntryService;
class FBlueprintHelperLogicMdReadService;
class FBlueprintHelperLogicJsonReadService;
class FBlueprintHelperAssetFactoryService;
class FBlueprintHelperComponentService;
class FBlueprintHelperClassSettingsService;
class FBlueprintHelperAppendBlueprintGraphService;
class FBlueprintHelperReplaceBlueprintGraphService;
class FBlueprintHelperPatchBlueprintGraphService;
class FBlueprintHelperMergeBlueprintGraphService;
class FBlueprintHelperGraphWriteServiceRegistry;
class FBlueprintHelperCompileAssetService;
class FBlueprintHelperBlueprintVariableService;
class FBlueprintHelperReviewStoreService;

/**
 * 命令路由器，将 Bridge 请求分发到对应的静态簇或系统层入口。
 * 工具簇命令由 *BridgeRoutes 执行；private Handle 仅保留系统层/遗留系统入口。
 */
class BLUEPRINTHELPER_API FBlueprintHelperBridgeRouter
{
public:
	FBlueprintHelperBridgeRouter(
		const FBlueprintHelperExportService& InExport,
		const FBlueprintHelperCompileService& InCompile,
		const FBlueprintHelperValidationService& InValidation,
		const FBlueprintHelperContextService& InContext,
		const FBlueprintHelperAssetBrowseService& InAssetBrowse,
		const FBlueprintHelperAssetDiscoveryService& InAssetDiscoveryService,
		const FBlueprintHelperBlueprintStructureService& InStructure,
		const FBlueprintHelperWidgetService& InWidget,
		const FBlueprintHelperPropertyReflectionService& InPropertyReflection,
		const FBlueprintHelperDataTableService& InDataTable,
		const FBlueprintHelperEditorCommandService& InEditorCommand,
		const FBlueprintHelperRuntimeProfileService& InRuntimeProfile,
		const FBlueprintHelperDiagnosticsService& InDiagnostics,
		const FBlueprintHelperDebugEntryService& InDebugEntryService,
		const FBlueprintHelperLogicMdReadService& InLogicMdRead,
		const FBlueprintHelperLogicJsonReadService& InLogicJsonRead,
		const FBlueprintHelperAssetFactoryService& InAssetFactory,
		const FBlueprintHelperComponentService& InComponentService,
		const FBlueprintHelperClassSettingsService& InClassSettings,
		const FBlueprintHelperGraphWriteServiceRegistry& InGraphWriteRegistry,
		const FBlueprintHelperCompileAssetService& InCompileAssetService,
		const FBlueprintHelperBlueprintVariableService& InVariableService,
		const FBlueprintHelperReviewStoreService& InReviewStoreService);

	/** 路由并执行命令。必须在 GameThread 调用。 */
	FBlueprintHelperBridgeResponse HandleRequest(const FBlueprintHelperBridgeRequest& Request) const;
	FBlueprintHelperBridgeResponse HandleRequestWithPlan(
		const FBlueprintHelperBridgeRequest& Request,
		const FBlueprintHelperBridgeRoutePlan& RoutePlan) const;

private:
	FBlueprintHelperBridgeResponse HandleGetRuleMarkdown(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleGetEditorContext(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleRequestWriteSession(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleGetRuntimeProfile(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleDiagnosticsRuntime(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleGetDebugCase(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleListDebugCases(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleExportDebugBundle(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleReadReferenceContext(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleReadFunctionChainContext(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleReadBlueprintLogicMd(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleReadBlueprintLogicJson(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleValidateJson(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleExportToJson(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleExportLogic(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleCompileBlueprint(const FBlueprintHelperBridgeRequest& Req) const;

	// ─── Phase 4: 资产浏览 ───
	FBlueprintHelperBridgeResponse HandleOpenAsset(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleSaveAsset(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleGetAssetInfo(const FBlueprintHelperBridgeRequest& Req) const;

	// ─── Phase 5: 蓝图结构操作 ───
	FBlueprintHelperBridgeResponse HandleListGraphs(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleListVariables(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleListEventDispatchers(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleAddVariable(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleRemoveVariable(const FBlueprintHelperBridgeRequest& Req) const;

	FBlueprintHelperBridgeResponse HandleAddGraph(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleRemoveGraph(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleAddEventDispatcher(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleDeleteNodes(const FBlueprintHelperBridgeRequest& Req) const;

	// ─── Phase 8: 编辑器命令 ───
	FBlueprintHelperBridgeResponse HandleUndo(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleRedo(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandlePlayInEditor(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleStopPIE(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleCreateBlueprint(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleExecConsoleCommand(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleCloseEditor(const FBlueprintHelperBridgeRequest& Req) const;

	FBlueprintHelperBridgeResponse HandlePreviewTaskPlan(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleExecuteTaskPlan(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleGetTaskRunJournal(const FBlueprintHelperBridgeRequest& Req) const;

	// ─── CompileBlueprintAsset ───
	FBlueprintHelperBridgeResponse HandleCompileBlueprintAsset(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleQueryReviewRecords(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleApplyReviewAction(const FBlueprintHelperBridgeRequest& Req) const;


	const FBlueprintHelperExportService& ExportService;
	const FBlueprintHelperCompileService& CompileService;
	const FBlueprintHelperValidationService& ValidationService;
	const FBlueprintHelperContextService& ContextService;
	const FBlueprintHelperAssetBrowseService& AssetBrowseService;
	FBlueprintHelperAssetDiscoveryBridgeRoutes AssetDiscoveryRoutes;
	const FBlueprintHelperBlueprintStructureService& StructureService;
	FBlueprintHelperUMGWidgetBridgeRoutes UMGWidgetRoutes;
	FBlueprintHelperObjectPropertyBridgeRoutes ObjectPropertyRoutes;
	FBlueprintHelperDataTableBridgeRoutes DataTableRoutes;
	const FBlueprintHelperEditorCommandService& EditorCommandService;
	const FBlueprintHelperRuntimeProfileService& RuntimeProfileService;
	const FBlueprintHelperDiagnosticsService& DiagnosticsService;
	const FBlueprintHelperDebugEntryService& DebugEntryService;
	const FBlueprintHelperLogicMdReadService& LogicMdReadService;
	const FBlueprintHelperLogicJsonReadService& LogicJsonReadService;
	FBlueprintHelperAssetFactoryBridgeRoutes AssetFactoryRoutes;
	FBlueprintHelperComponentBridgeRoutes ComponentRoutes;
	FBlueprintHelperClassSettingsBridgeRoutes ClassSettingsRoutes;
	FBlueprintHelperGraphWriteBridgeRoutes GraphWriteRoutes;
	const FBlueprintHelperBlueprintVariableService& VariableService;
	FBlueprintHelperBlueprintVariablesBridgeRoutes BlueprintVariablesRoutes;
	FBlueprintHelperDependencyAnalysisService DependencyAnalysisService;
	FBlueprintHelperFunctionChainContextService FunctionChainContextService;
	FBlueprintHelperTaskRuntimeService TaskRuntimeService;
	const FBlueprintHelperCompileAssetService& CompileAssetService;
	const FBlueprintHelperReviewStoreService& ReviewStoreService;
};
