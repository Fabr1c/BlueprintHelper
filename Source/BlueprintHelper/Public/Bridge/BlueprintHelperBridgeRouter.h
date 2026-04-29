// BlueprintHelper Bridge Layer — 命令路由

#pragma once

#include "CoreMinimal.h"
#include "Bridge/BlueprintHelperBridgeTypes.h"

class FBlueprintHelperImportService;
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

/**
 * 命令路由器，将 Bridge 请求分发到对应的 Service 方法。
 * 所有 Handle 方法必须在 GameThread 上调用。
 */
class BLUEPRINTHELPER_API FBlueprintHelperBridgeRouter
{
public:
	FBlueprintHelperBridgeRouter(
		const FBlueprintHelperImportService& InImport,
		const FBlueprintHelperExportService& InExport,
		const FBlueprintHelperCompileService& InCompile,
		const FBlueprintHelperValidationService& InValidation,
		const FBlueprintHelperContextService& InContext,
		const FBlueprintHelperAssetBrowseService& InAssetBrowse,
		const FBlueprintHelperBlueprintStructureService& InStructure,
		const FBlueprintHelperWidgetService& InWidget,
		const FBlueprintHelperPropertyReflectionService& InPropertyReflection,
		const FBlueprintHelperDataTableService& InDataTable,
		const FBlueprintHelperEditorCommandService& InEditorCommand);

	/** 路由并执行命令。必须在 GameThread 调用。 */
	FBlueprintHelperBridgeResponse HandleRequest(const FBlueprintHelperBridgeRequest& Request) const;

private:
	FBlueprintHelperBridgeResponse HandleGetRuleMarkdown(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleGetEditorContext(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleValidateJson(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleExportToJson(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleImportJson(const FBlueprintHelperBridgeRequest& Req) const;
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
	FBlueprintHelperBridgeResponse HandleCreateBlueprint(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleExecConsoleCommand(const FBlueprintHelperBridgeRequest& Req) const;
	FBlueprintHelperBridgeResponse HandleCloseEditor(const FBlueprintHelperBridgeRequest& Req) const;

	const FBlueprintHelperImportService& ImportService;
	const FBlueprintHelperExportService& ExportService;
	const FBlueprintHelperCompileService& CompileService;
	const FBlueprintHelperValidationService& ValidationService;
	const FBlueprintHelperContextService& ContextService;
	const FBlueprintHelperAssetBrowseService& AssetBrowseService;
	const FBlueprintHelperBlueprintStructureService& StructureService;
	const FBlueprintHelperWidgetService& WidgetService;
	const FBlueprintHelperPropertyReflectionService& PropertyReflectionService;
	const FBlueprintHelperDataTableService& DataTableService;
	const FBlueprintHelperEditorCommandService& EditorCommandService;
};
