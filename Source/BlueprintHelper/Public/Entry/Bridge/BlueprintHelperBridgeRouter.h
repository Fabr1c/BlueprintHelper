// BlueprintHelper Bridge Layer — 命令路由

#pragma once

#include "CoreMinimal.h"
#include "Entry/Bridge/BlueprintHelperBridgeRoutePlanner.h"
#include "Entry/Bridge/Routes/BlueprintHelperBlueprintVariablesBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperClassSettingsBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperCleanupOwnershipBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperComponentBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperDataTableBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperGraphWriteBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperObjectPropertyBridgeRoutes.h"
#include "Entry/Bridge/Routes/BlueprintHelperUMGWidgetBridgeRoutes.h"
#include "Entry/Bridge/BlueprintHelperBridgeTypes.h"
#include "Shared/Safety/BlueprintHelperDependencyAnalysisService.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"

class FBlueprintHelperImportService;
class FBlueprintHelperAgentImportService;
class FBlueprintHelperExportService;
class FBlueprintHelperCompileService;
class FBlueprintHelperValidationService;
class FBlueprintHelperContextService;
class FBlueprintHelperAssetBrowseService;
class FBlueprintHelperBlueprintStructureService;
class FBlueprintHelperWidgetService;
class FBlueprintHelperPropertyReflectionService;
class FBlueprintHelperDataTableService;
class FBlueprintHelperEditorCommandService;
class FBlueprintHelperRuntimeProfileService;
class FBlueprintHelperDiagnosticsService;
class FBlueprintHelperLogicMdReadService;
class FBlueprintHelperLogicJsonReadService;
class FBlueprintHelperAssetFactoryService;
class FBlueprintHelperComponentService;
class FBlueprintHelperClassSettingsService;
class FBlueprintHelperAppendBlueprintGraphService;
class FBlueprintHelperReplaceBlueprintGraphService;
class FBlueprintHelperPatchBlueprintGraphService;
class FBlueprintHelperMergeBlueprintGraphService;
class FBlueprintHelperCleanupBlueprintHelperBlockService;
class FBlueprintHelperRollbackCleanupTransactionService;
class FBlueprintHelperConvertBlockToUserOwnedService;
class FBlueprintHelperCompileAssetService;
class FBlueprintHelperTransactionQueryService;
class FBlueprintHelperBlueprintVariableService;

/**
 * 命令路由器，将 Bridge 请求分发到对应的 Service 方法。
 * 所有 Handle 方法必须在 GameThread 上调用。
 */
class BLUEPRINTHELPER_API FBlueprintHelperBridgeRouter
{
public:
	FBlueprintHelperBridgeRouter(
		const FBlueprintHelperImportService& InImport,
		const FBlueprintHelperAgentImportService& InAgentImport,
		const FBlueprintHelperExportService& InExport,
		const FBlueprintHelperCompileService& InCompile,
		const FBlueprintHelperValidationService& InValidation,
		const FBlueprintHelperContextService& InContext,
		const FBlueprintHelperAssetBrowseService& InAssetBrowse,
		const FBlueprintHelperBlueprintStructureService& InStructure,
		const FBlueprintHelperWidgetService& InWidget,
		const FBlueprintHelperPropertyReflectionService& InPropertyReflection,
		const FBlueprintHelperDataTableService& InDataTable,
		const FBlueprintHelperEditorCommandService& InEditorCommand,
		const FBlueprintHelperRuntimeProfileService& InRuntimeProfile,
		const FBlueprintHelperDiagnosticsService& InDiagnostics,
		const FBlueprintHelperLogicMdReadService& InLogicMdRead,
		const FBlueprintHelperLogicJsonReadService& InLogicJsonRead,
		const FBlueprintHelperAssetFactoryService& InAssetFactory,
		const FBlueprintHelperComponentService& InComponentService,
		const FBlueprintHelperClassSettingsService& InClassSettings,
		const FBlueprintHelperAppendBlueprintGraphService& InAppendGraphService,
		const FBlueprintHelperReplaceBlueprintGraphService& InReplaceGraphService,
		const FBlueprintHelperPatchBlueprintGraphService& InPatchGraphService,
		const FBlueprintHelperMergeBlueprintGraphService& InMergeGraphService,
		const FBlueprintHelperCleanupBlueprintHelperBlockService& InCleanupBlockService,
		const FBlueprintHelperRollbackCleanupTransactionService& InRollbackCleanupService,
		const FBlueprintHelperConvertBlockToUserOwnedService& InConvertBlockService,
		const FBlueprintHelperCompileAssetService& InCompileAssetService,
		const FBlueprintHelperTransactionQueryService& InTransactionQueryService,
		const FBlueprintHelperBlueprintVariableService& InVariableService);

	/** 路由并执行命令。必须在 GameThread 调用。 */
	FBlueprintHelperBridgeResponse HandleRequest(const FBlueprintHelperBridgeRequest& Request) const;
	FBlueprintHelperBridgeResponse HandleRequestWithPlan(
		const FBlueprintHelperBridgeRequest& Request,
		const FBlueprintHelperBridgeRoutePlan& RoutePlan) const;

private:
	FBlueprintHelperBridgeResponse HandleGetRuleMarkdown(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleGetEditorContext(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleGetRuntimeProfile(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleDiagnosticsRuntime(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleReadReferenceContext(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleReadBlueprintLogicMd(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleReadBlueprintLogicJson(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleValidateJson(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleExportToJson(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleExportLogic(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleImportJson(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleImportAgentGraph(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleCompileBlueprint(const FBlueprintHelperBridgeRequest& Req) const;

	// ─── Phase 4: 资产浏览 ───
	FBlueprintHelperBridgeResponse HandleOpenAsset(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleListAssets(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleSearchAssets(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleSaveAsset(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleGetAssetInfo(const FBlueprintHelperBridgeRequest& Req) const;

	// ─── Phase 5: 蓝图结构操作 ───
	FBlueprintHelperBridgeResponse HandleListGraphs(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleListVariables(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleListEventDispatchers(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleAddVariable(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleRemoveVariable(const FBlueprintHelperBridgeRequest& Req) const;

	// ─── Blueprint Variable Service ───
	FBlueprintHelperBridgeResponse HandleReadMemberVariables(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleAddMemberVariable(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleAddMemberVariables(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleSetMemberVariableProperties(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleRemoveMemberVariable(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleRemoveMemberVariables(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleReadMemberDefaults(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleSetMemberDefault(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleSetMemberDefaults(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleReadLocalVariables(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleAddLocalVariable(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleAddLocalVariables(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleSetLocalVariableProperties(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleRemoveLocalVariable(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleRemoveLocalVariables(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleAddGraph(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleRemoveGraph(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleAddEventDispatcher(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleDeleteNodes(const FBlueprintHelperBridgeRequest& Req) const;

	// ─── Phase 6: UMG Widget 操作 ───
	FBlueprintHelperBridgeResponse HandleGetWidgetTree(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleAddWidget(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleRemoveWidget(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleMoveWidget(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleGetWidgetProperties(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleSetWidgetProperty(const FBlueprintHelperBridgeRequest& Req) const;

	// ─── Phase 7: DataAsset & DataTable 操作 ───
	FBlueprintHelperBridgeResponse HandleGetObjectProperties(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleSetObjectProperty(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleGetDataTableRows(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleAddDataTableRow(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleUpdateDataTableRow(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleDeleteDataTableRow(const FBlueprintHelperBridgeRequest& Req) const;

	// ─── Phase 8: 编辑器命令 ───
	FBlueprintHelperBridgeResponse HandleUndo(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleRedo(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandlePlayInEditor(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleStopPIE(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleCreateAsset(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleReadComponents(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleAddComponent(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleSetComponentProperty(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleSetComponentProperties(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleRemoveComponent(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleCreateBlueprint(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleExecConsoleCommand(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleCloseEditor(const FBlueprintHelperBridgeRequest& Req) const;

	// ─── Phase 9: Blueprint Class Settings ───
	FBlueprintHelperBridgeResponse HandleReadClassSettings(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleAddImplementedInterface(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleAddImplementedInterfaces(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleRemoveImplementedInterface(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleRemoveImplementedInterfaces(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleSetClassDefaultProperty(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleSetClassDefaultProperties(const FBlueprintHelperBridgeRequest& Req) const;

	// ─── AppendBlueprintGraph ───
	FBlueprintHelperBridgeResponse HandlePreviewTaskPlan(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleExecuteTaskPlan(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleGetTaskRunJournal(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleAppendBlueprintGraph(const FBlueprintHelperBridgeRequest& Req) const;

	// ─── ReplaceBlueprintGraph ───
	FBlueprintHelperBridgeResponse HandleReplaceBlueprintGraph(const FBlueprintHelperBridgeRequest& Req) const;

	// ─── PatchBlueprintGraph ───
	FBlueprintHelperBridgeResponse HandlePatchBlueprintGraph(const FBlueprintHelperBridgeRequest& Req) const;

	// ─── MergeBlueprintGraph ───
	FBlueprintHelperBridgeResponse HandleMergeBlueprintGraph(const FBlueprintHelperBridgeRequest& Req) const;

	// ─── CleanupBlueprintHelperBlock ───
	FBlueprintHelperBridgeResponse HandleCleanupBlueprintHelperBlock(const FBlueprintHelperBridgeRequest& Req) const;

	// ─── RollbackCleanupTransaction ───
	FBlueprintHelperBridgeResponse HandleRollbackCleanupTransaction(const FBlueprintHelperBridgeRequest& Req) const;

	// ─── ConvertBlockToUserOwned ───
	FBlueprintHelperBridgeResponse HandleConvertBlockToUserOwned(const FBlueprintHelperBridgeRequest& Req) const;

	// ─── CompileBlueprintAsset ───
	FBlueprintHelperBridgeResponse HandleCompileBlueprintAsset(const FBlueprintHelperBridgeRequest& Req) const;

	// ─── Transaction Query ───
	FBlueprintHelperBridgeResponse HandleListTransactions(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleReadTransaction(const FBlueprintHelperBridgeRequest& Req) const;

	const FBlueprintHelperImportService& ImportService;
	const FBlueprintHelperAgentImportService& AgentImportService;
	const FBlueprintHelperExportService& ExportService;
	const FBlueprintHelperCompileService& CompileService;
	const FBlueprintHelperValidationService& ValidationService;
	const FBlueprintHelperContextService& ContextService;
	const FBlueprintHelperAssetBrowseService& AssetBrowseService;
	const FBlueprintHelperBlueprintStructureService& StructureService;
	FBlueprintHelperUMGWidgetBridgeRoutes UMGWidgetRoutes;
	FBlueprintHelperObjectPropertyBridgeRoutes ObjectPropertyRoutes;
	FBlueprintHelperDataTableBridgeRoutes DataTableRoutes;
	const FBlueprintHelperEditorCommandService& EditorCommandService;
	const FBlueprintHelperRuntimeProfileService& RuntimeProfileService;
	const FBlueprintHelperDiagnosticsService& DiagnosticsService;
	const FBlueprintHelperLogicMdReadService& LogicMdReadService;
	const FBlueprintHelperLogicJsonReadService& LogicJsonReadService;
	const FBlueprintHelperAssetFactoryService& AssetFactoryService;
	FBlueprintHelperComponentBridgeRoutes ComponentRoutes;
	FBlueprintHelperClassSettingsBridgeRoutes ClassSettingsRoutes;
	FBlueprintHelperGraphWriteBridgeRoutes GraphWriteRoutes;
	FBlueprintHelperCleanupOwnershipBridgeRoutes CleanupOwnershipRoutes;
	const FBlueprintHelperBlueprintVariableService& VariableService;
	FBlueprintHelperBlueprintVariablesBridgeRoutes BlueprintVariablesRoutes;
	FBlueprintHelperDependencyAnalysisService DependencyAnalysisService;
	FBlueprintHelperTaskRuntimeService TaskRuntimeService;
	const FBlueprintHelperCompileAssetService& CompileAssetService;
	const FBlueprintHelperTransactionQueryService& TransactionQueryService;
};
