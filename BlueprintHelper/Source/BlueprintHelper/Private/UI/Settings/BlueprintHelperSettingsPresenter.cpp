// BlueprintHelper settings presenter implementation.

#include "UI/Settings/BlueprintHelperSettingsPresenter.h"
#include "UI/Settings/Utils/BlueprintHelperSettingsUIUtils.h"

#include "Systems/Config/BlueprintHelperSafetyProfileResolver.h"

#define LOCTEXT_NAMESPACE "BlueprintHelperSettingsPresenter"

const FBlueprintHelperSettingView& FBlueprintHelperSettingsPresenter::Reload()
{
	View = FBlueprintHelperSettingStore::Load();
	ReloadRows();
	return View;
}

const FBlueprintHelperSettingView& FBlueprintHelperSettingsPresenter::EnsureProjectSetting()
{
	FString Path;
	FString Error;
	if (!FBlueprintHelperSettingStore::EnsureProjectSetting(Path, Error))
	{
		View = FBlueprintHelperSettingStore::Load();
		View.ErrorText = Error;
		View.StatusText = Error;
		ReloadRows();
		return View;
	}

	View = FBlueprintHelperSettingStore::Load();
	View.StatusText = FString::Printf(TEXT("Project setting ready: %s"), *Path);
	ReloadRows();
	return View;
}

const FBlueprintHelperSettingView& FBlueprintHelperSettingsPresenter::GetView() const
{
	return View;
}

const TArray<FBlueprintHelperSettingRowViewModel>& FBlueprintHelperSettingsPresenter::GetRows() const
{
	return Rows;
}

FBlueprintHelperSettingsRowsChanged& FBlueprintHelperSettingsPresenter::OnRowsChanged()
{
	return RowsChanged;
}

void FBlueprintHelperSettingsPresenter::ReloadRows()
{
	Rows.Reset();

	const FText ReviewVisualCategory = LOCTEXT("SettingsCategoryReviewVisual", "Review 可视化");
	const FText ReviewDebugCategory = LOCTEXT("SettingsCategoryReviewDebug", "Review 调试");
	const FText DebugExportCategory = LOCTEXT("SettingsCategoryDebugExport", "调试导出");
	const FText ToolOutputCategory = LOCTEXT("SettingsCategoryToolOutput", "工具输出");
	const FText DeveloperDryRunCategory = LOCTEXT("SettingsCategoryDeveloperDryRun", "DryRun");
	const FText DeveloperToolClusterCategory = LOCTEXT("SettingsCategoryDeveloperToolCluster", "开发者 ToolCluster");
	const FText SafetyCategory = LOCTEXT("SettingsCategorySafety", "安全");
	const FText DeveloperUiCategory = LOCTEXT("SettingsCategoryDeveloperUi", "开发者 UI");
	const FText DeveloperGraphLayoutCategory = LOCTEXT("SettingsCategoryDeveloperGraphLayout", "开发者 GraphLayout");

	Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
		TEXT("ui.review_panel.diff_frame_outer_padding"),
		ReviewVisualCategory,
		LOCTEXT("DiffFrameOuterPaddingLabel", "Diff 外边距"),
		LOCTEXT("DiffFrameOuterPaddingHint", "调整 Diff 框与被标记区域之间的外扩距离。只影响 Review 可视化，不改变资产。"),
		0.0,
		64.0));
	Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
		TEXT("ui.review_panel.diff_action_padding"),
		ReviewVisualCategory,
		LOCTEXT("DiffActionPaddingLabel", "操作按钮内边距"),
		LOCTEXT("DiffActionPaddingHint", "调整 Accept / Reject 按钮区域的内边距。"),
		0.0,
		64.0));
	Rows.Add(UBlueprintHelperSettingsUIUtils::MakeMarginRow(
		TEXT("ui.review_panel.diff_action_spacing"),
		ReviewVisualCategory,
		LOCTEXT("DiffActionSpacingLabel", "操作按钮间距"),
		LOCTEXT("DiffActionSpacingHint", "调整 Accept / Reject 按钮之间的间距，格式为左,上,右,下。"),
		0.0,
		64.0));
	Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
		TEXT("ui.review_panel.surface_overlay_fill_alpha"),
		ReviewVisualCategory,
		LOCTEXT("SurfaceOverlayFillAlphaLabel", "Diff 填充透明度"),
		LOCTEXT("SurfaceOverlayFillAlphaHint", "控制普通 Diff 区域背景填充透明度，取值范围 0 到 1。"),
		0.0,
		1.0));
	Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
		TEXT("ui.review_panel.surface_overlay_selected_fill_alpha"),
		ReviewVisualCategory,
		LOCTEXT("SurfaceOverlaySelectedFillAlphaLabel", "选中 Diff 填充透明度"),
		LOCTEXT("SurfaceOverlaySelectedFillAlphaHint", "控制选中 Diff 区域背景填充透明度，取值范围 0 到 1。"),
		0.0,
		1.0));
	Rows.Add(UBlueprintHelperSettingsUIUtils::MakeVector2Row(
		TEXT("ui.review_panel.surface_geometry_padding"),
		ReviewVisualCategory,
		LOCTEXT("SurfaceGeometryPaddingLabel", "Diff 几何外扩"),
		LOCTEXT("SurfaceGeometryPaddingHint", "调整 Surface 几何匹配后的 Diff 绘制外扩量，格式为 X,Y。"),
		0.0,
		128.0));
	Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
		TEXT("ui.review_panel.debug_max_messages"),
		ReviewDebugCategory,
		LOCTEXT("DebugMaxMessagesLabel", "Debug 最大消息数"),
		LOCTEXT("DebugMaxMessagesHint", "控制 Review Debug 面板保留的最近消息数量。"),
		10,
		5000));
	Rows.Add(UBlueprintHelperSettingsUIUtils::MakeChoiceRow(
		TEXT("review.debug_bundle.retention"),
		ReviewDebugCategory,
		LOCTEXT("DebugBundleRetentionLabel", "DebugBundle 保留策略"),
		LOCTEXT("DebugBundleRetentionHint", "控制 Review DebugBundle 的保留方式。"),
		{
			{ TEXT("standard"), LOCTEXT("DebugBundleRetentionStandard", "标准") },
			{ TEXT("aggressive"), LOCTEXT("DebugBundleRetentionAggressive", "积极清理") },
			{ TEXT("keep_all"), LOCTEXT("DebugBundleRetentionKeepAll", "保留全部") }
		}));
	Rows.Add(UBlueprintHelperSettingsUIUtils::MakeChoiceRow(
		TEXT("debug.export_profile"),
		DebugExportCategory,
		LOCTEXT("DebugExportProfileLabel", "Debug 导出级别"),
		LOCTEXT("DebugExportProfileHint", "控制 DebugBundle 导出的详细程度。"),
		{
			{ TEXT("minimal"), LOCTEXT("DebugExportProfileMinimal", "最小") },
			{ TEXT("standard"), LOCTEXT("DebugExportProfileStandard", "标准") },
			{ TEXT("full"), LOCTEXT("DebugExportProfileFull", "完整") }
		}));
	Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
		TEXT("debug.contains_full_settings"),
		DebugExportCategory,
		LOCTEXT("DebugContainsFullSettingsLabel", "导出完整设置"),
		LOCTEXT("DebugContainsFullSettingsHint", "控制 DebugBundle 是否包含完整设置快照。")));
	Rows.Add(UBlueprintHelperSettingsUIUtils::MakeChoiceRow(
		TEXT("profiles.default.safety_profile"),
		SafetyCategory,
		LOCTEXT("SafetyProfileLabel", "安全等级"),
		LOCTEXT("SafetyProfileHint", "BlueprintHelper 写入授权使用的运行时安全等级。AutoRepair 可跳过写请求弹窗。"),
		{
			{ TEXT("ReadOnly"), LOCTEXT("SafetyProfileReadOnly", "只读") },
			{ TEXT("Conservative"), LOCTEXT("SafetyProfileConservative", "保守") },
			{ TEXT("Standard"), LOCTEXT("SafetyProfileStandard", "标准") },
			{ TEXT("AutoRepair"), LOCTEXT("SafetyProfileAutoRepair", "自动修复") }
		},
		true));
	Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
		TEXT("safety.preview_required"),
		SafetyCategory,
		LOCTEXT("SafetyPreviewRequiredLabel", "需要 Preview"),
		LOCTEXT("SafetyPreviewRequiredHint", "开启后，写入执行前必须先通过 Preview。"),
		true));
	Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
		TEXT("safety.write_approval_required"),
		SafetyCategory,
		LOCTEXT("SafetyWriteApprovalRequiredLabel", "需要写入批准"),
		LOCTEXT("SafetyWriteApprovalRequiredHint", "开启后，写入执行前需要弹窗批准。"),
		true));
	Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
		TEXT("safety.approval_bypass"),
		SafetyCategory,
		LOCTEXT("SafetyApprovalBypassLabel", "跳过批准弹窗"),
		LOCTEXT("SafetyApprovalBypassHint", "允许受信任的 AutoRepair 流程跳过写请求弹窗。"),
		true));
	Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
		TEXT("tool_clusters.signature.reference_context_max_results"),
		ToolOutputCategory,
		LOCTEXT("SignatureReferenceContextMaxResultsLabel", "签名引用最大结果数"),
		LOCTEXT("SignatureReferenceContextMaxResultsHint", "控制签名引用上下文最多返回多少条结果。"),
		0,
		1000));
	Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
		TEXT("tool_clusters.read_context.max_output_rows"),
		ToolOutputCategory,
		LOCTEXT("ReadContextMaxOutputRowsLabel", "Read Context 最大行数"),
		LOCTEXT("ReadContextMaxOutputRowsHint", "控制 Read Context 输出最大行数，0 表示不限制。"),
		0,
		100000));
	Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
		TEXT("tool_clusters.read_context.max_output_bytes"),
		ToolOutputCategory,
		LOCTEXT("ReadContextMaxOutputBytesLabel", "Read Context 最大字节数"),
		LOCTEXT("ReadContextMaxOutputBytesHint", "控制 Read Context 输出最大字节数，0 表示不限制。"),
		0,
		104857600));

	if (UBlueprintHelperSettingsUIUtils::ShouldShowDeveloperSettings())
	{
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("tool_clusters.asset_factory.dry_run"),
			DeveloperDryRunCategory,
			LOCTEXT("AssetFactoryDryRunLabel", "AssetFactory DryRun"),
			LOCTEXT("AssetFactoryDryRunHint", "控制 AssetFactory 写入类请求的默认 dry_run；开启后默认只预演，不创建资产。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("tool_clusters.component.dry_run"),
			DeveloperDryRunCategory,
			LOCTEXT("ComponentDryRunLabel", "Component DryRun"),
			LOCTEXT("ComponentDryRunHint", "控制 Component 写入请求的默认 dry_run；开启后默认只预演，不修改组件。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("tool_clusters.class_settings.dry_run"),
			DeveloperDryRunCategory,
			LOCTEXT("ClassSettingsDryRunLabel", "ClassSettings DryRun"),
			LOCTEXT("ClassSettingsDryRunHint", "控制 ClassSettings 写入请求的默认 dry_run；开启后默认只预演，不修改类设置。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("tool_clusters.blueprint_variables.dry_run"),
			DeveloperDryRunCategory,
			LOCTEXT("BlueprintVariablesDryRunLabel", "BlueprintVariables DryRun"),
			LOCTEXT("BlueprintVariablesDryRunHint", "控制 BlueprintVariables 写入请求的默认 dry_run；开启后默认只预演，不修改变量。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("tool_clusters.object_property.dry_run"),
			DeveloperDryRunCategory,
			LOCTEXT("ObjectPropertyDryRunLabel", "ObjectProperty DryRun"),
			LOCTEXT("ObjectPropertyDryRunHint", "控制 ObjectProperty 写入请求的默认 dry_run；开启后默认只预演，不修改对象属性。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("tool_clusters.data_table.dry_run"),
			DeveloperDryRunCategory,
			LOCTEXT("DataTableDryRunLabel", "DataTable DryRun"),
			LOCTEXT("DataTableDryRunHint", "控制 DataTable 写入请求的默认 dry_run；开启后默认只预演，不修改表格行。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("tool_clusters.umg_widget.dry_run"),
			DeveloperDryRunCategory,
			LOCTEXT("UmgWidgetDryRunLabel", "UMGWidget DryRun"),
			LOCTEXT("UmgWidgetDryRunHint", "控制 UMGWidget 写入请求的默认 dry_run；开启后默认只预演，不修改 WidgetTree。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("tool_clusters.graph_write.dry_run"),
			DeveloperDryRunCategory,
			LOCTEXT("GraphWriteDryRunLabel", "GraphWrite DryRun"),
			LOCTEXT("GraphWriteDryRunHint", "控制 GraphWrite 写入请求的默认 dry_run；开启后默认只预演，不写入蓝图图表。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeStringRow(
			TEXT("tool_clusters.asset_factory.default_parent_class"),
			DeveloperToolClusterCategory,
			LOCTEXT("AssetFactoryDefaultParentClassLabel", "AssetFactory 默认父类"),
			LOCTEXT("AssetFactoryDefaultParentClassHint", "AssetFactory 请求未提供 parent_class 时使用的开发者默认值。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeStringRow(
			TEXT("tool_clusters.asset_factory.default_value_type"),
			DeveloperToolClusterCategory,
			LOCTEXT("AssetFactoryDefaultValueTypeLabel", "AssetFactory 默认值类型"),
			LOCTEXT("AssetFactoryDefaultValueTypeHint", "AssetFactory 请求未提供 value_type 时使用的开发者默认值。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeStringRow(
			TEXT("tool_clusters.asset_factory.default_collision_policy"),
			DeveloperToolClusterCategory,
			LOCTEXT("AssetFactoryDefaultCollisionPolicyLabel", "AssetFactory 默认冲突策略"),
			LOCTEXT("AssetFactoryDefaultCollisionPolicyHint", "AssetFactory 请求未提供 collision_policy 时使用的开发者默认值。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeStringRow(
			TEXT("tool_clusters.signature.reference_context_search_scope"),
			DeveloperToolClusterCategory,
			LOCTEXT("SignatureReferenceContextSearchScopeLabel", "Signature 引用搜索范围"),
			LOCTEXT("SignatureReferenceContextSearchScopeHint", "Signature 引用上下文未提供 search_scope 时使用的开发者默认值。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeStringRow(
			TEXT("tool_clusters.signature.reference_context_resolution_policy"),
			DeveloperToolClusterCategory,
			LOCTEXT("SignatureReferenceContextResolutionPolicyLabel", "Signature 解析策略"),
			LOCTEXT("SignatureReferenceContextResolutionPolicyHint", "Signature 引用上下文未提供 resolution_policy 时使用的开发者默认值。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeStringRow(
			TEXT("tool_clusters.signature.reference_context_detail"),
			DeveloperToolClusterCategory,
			LOCTEXT("SignatureReferenceContextDetailLabel", "Signature 详情级别"),
			LOCTEXT("SignatureReferenceContextDetailHint", "Signature 引用上下文未提供 detail 时使用的开发者默认值。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeStringRow(
			TEXT("tool_clusters.component.default_attach_rule"),
			DeveloperToolClusterCategory,
			LOCTEXT("ComponentDefaultAttachRuleLabel", "Component 默认挂接规则"),
			LOCTEXT("ComponentDefaultAttachRuleHint", "Component 请求未提供 attach_rule 时使用的开发者默认值。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeStringRow(
			TEXT("tool_clusters.component.default_name_collision_policy"),
			DeveloperToolClusterCategory,
			LOCTEXT("ComponentDefaultNameCollisionPolicyLabel", "Component 默认命名冲突策略"),
			LOCTEXT("ComponentDefaultNameCollisionPolicyHint", "Component 请求未提供 name_collision_policy 时使用的开发者默认值。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeStringRow(
			TEXT("tool_clusters.component.default_property_mode"),
			DeveloperToolClusterCategory,
			LOCTEXT("ComponentDefaultPropertyModeLabel", "Component 默认属性模式"),
			LOCTEXT("ComponentDefaultPropertyModeHint", "Component 请求未提供 property_mode 时使用的开发者默认值。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("tool_clusters.class_settings.validation_should_compile"),
			DeveloperToolClusterCategory,
			LOCTEXT("ClassSettingsValidationCompileLabel", "ClassSettings 校验编译"),
			LOCTEXT("ClassSettingsValidationCompileHint", "ClassSettings 请求完成后是否默认执行编译校验。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("tool_clusters.class_settings.validation_should_save"),
			DeveloperToolClusterCategory,
			LOCTEXT("ClassSettingsValidationSaveLabel", "ClassSettings 校验保存"),
			LOCTEXT("ClassSettingsValidationSaveHint", "ClassSettings 请求完成后是否默认执行保存校验。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeStringRow(
			TEXT("tool_clusters.blueprint_variables.read_member_defaults_scope"),
			DeveloperToolClusterCategory,
			LOCTEXT("BlueprintVariablesReadMemberDefaultsScopeLabel", "成员默认值读取范围"),
			LOCTEXT("BlueprintVariablesReadMemberDefaultsScopeHint", "读取成员默认值时使用的开发者默认 scope。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeStringRow(
			TEXT("tool_clusters.blueprint_variables.asset_path_fallback"),
			DeveloperToolClusterCategory,
			LOCTEXT("BlueprintVariablesAssetPathFallbackLabel", "资产路径兜底策略"),
			LOCTEXT("BlueprintVariablesAssetPathFallbackHint", "读取请求未提供 asset_path 时使用的开发者默认目标策略。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("tool_clusters.data_table.write_requires_row_struct"),
			DeveloperToolClusterCategory,
			LOCTEXT("DataTableWriteRequiresRowStructLabel", "DataTable 要求 RowStruct"),
			LOCTEXT("DataTableWriteRequiresRowStructHint", "DataTable 写入时是否默认要求提供行结构。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("tool_clusters.umg_widget.asset_path_required"),
			DeveloperToolClusterCategory,
			LOCTEXT("UmgWidgetAssetPathRequiredLabel", "UMGWidget 要求资产路径"),
			LOCTEXT("UmgWidgetAssetPathRequiredHint", "UMGWidget 请求是否默认要求提供 asset_path。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("tool_clusters.graph_write.strict"),
			DeveloperToolClusterCategory,
			LOCTEXT("GraphWriteStrictLabel", "GraphWrite 严格模式"),
			LOCTEXT("GraphWriteStrictHint", "GraphWrite 请求未提供 strict 时使用的开发者默认值。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("tool_clusters.graph_write.create_missing_variables"),
			DeveloperToolClusterCategory,
			LOCTEXT("GraphWriteCreateMissingVariablesLabel", "自动创建缺失变量"),
			LOCTEXT("GraphWriteCreateMissingVariablesHint", "GraphWrite 遇到缺失变量时是否默认允许自动创建。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("tool_clusters.graph_write.reconstruct_existing_nodes"),
			DeveloperToolClusterCategory,
			LOCTEXT("GraphWriteReconstructExistingNodesLabel", "复用并重建已有节点"),
			LOCTEXT("GraphWriteReconstructExistingNodesHint", "GraphWrite 是否默认允许复用并重建已有节点。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("tool_clusters.graph_write.compile"),
			DeveloperToolClusterCategory,
			LOCTEXT("GraphWriteCompileLabel", "GraphWrite 校验编译"),
			LOCTEXT("GraphWriteCompileHint", "GraphWrite 请求完成后是否默认执行编译校验。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("tool_clusters.graph_write.save"),
			DeveloperToolClusterCategory,
			LOCTEXT("GraphWriteSaveLabel", "GraphWrite 校验保存"),
			LOCTEXT("GraphWriteSaveHint", "GraphWrite 请求完成后是否默认执行保存校验。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeStringRow(
			TEXT("graph_layout.rules_source"),
			DeveloperGraphLayoutCategory,
			LOCTEXT("GraphLayoutRulesSourceLabel", "GraphLayout 规则来源"),
			LOCTEXT("GraphLayoutRulesSourceHint", "GraphLayout 规则文件路径；允许项目配置目录下的相对路径，或受信任目录内的绝对路径。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeColorArrayRow(
			TEXT("ui.main_window.active_tab_color"),
			DeveloperUiCategory,
			LOCTEXT("MainWindowActiveTabColorLabel", "激活 Tab 颜色"),
			LOCTEXT("MainWindowActiveTabColorHint", "主窗口激活 Tab 的 RGBA 数组，格式：[R,G,B,A]。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeColorArrayRow(
			TEXT("ui.main_window.inactive_tab_color"),
			DeveloperUiCategory,
			LOCTEXT("MainWindowInactiveTabColorLabel", "未激活 Tab 颜色"),
			LOCTEXT("MainWindowInactiveTabColorHint", "主窗口未激活 Tab 的 RGBA 数组，格式：[R,G,B,A]。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeColorArrayRow(
			TEXT("ui.task_spec_workbench.block_colors.default"),
			DeveloperUiCategory,
			LOCTEXT("WorkbenchDefaultBlockColorLabel", "Workbench 默认块颜色"),
			LOCTEXT("WorkbenchDefaultBlockColorHint", "TaskSpec Workbench 默认块的 RGBA 数组，格式：[R,G,B,A]。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeColorArrayRow(
			TEXT("ui.task_spec_workbench.block_colors.graph_logic"),
			DeveloperUiCategory,
			LOCTEXT("WorkbenchGraphLogicBlockColorLabel", "Workbench 图逻辑块颜色"),
			LOCTEXT("WorkbenchGraphLogicBlockColorHint", "TaskSpec Workbench 图逻辑块的 RGBA 数组，格式：[R,G,B,A]。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeColorArrayRow(
			TEXT("ui.task_spec_workbench.block_colors.diagnostic"),
			DeveloperUiCategory,
			LOCTEXT("WorkbenchDiagnosticBlockColorLabel", "Workbench 诊断块颜色"),
			LOCTEXT("WorkbenchDiagnosticBlockColorHint", "TaskSpec Workbench 诊断块的 RGBA 数组，格式：[R,G,B,A]。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeColorArrayRow(
			TEXT("ui.task_spec_workbench.block_colors.selected"),
			DeveloperUiCategory,
			LOCTEXT("WorkbenchSelectedBlockColorLabel", "Workbench 选中块颜色"),
			LOCTEXT("WorkbenchSelectedBlockColorHint", "TaskSpec Workbench 选中块的 RGBA 数组，格式：[R,G,B,A]。"),
			true));
	}

	for (FBlueprintHelperSettingRowViewModel& Row : Rows)
	{
		if (const FString* ErrorText = RowErrorsByPath.Find(Row.DotPath))
		{
			Row.ErrorText = *ErrorText;
		}
	}
}

void FBlueprintHelperSettingsPresenter::HandleSettingValueCommitted(const FBlueprintHelperSettingEditEvent& Event)
{
	const FBlueprintHelperSettingRowViewModel* Row = FindRowByPath(Event.DotPath);
	if (!Row)
	{
		SetRowErrorAndBroadcast(Event.DotPath, LOCTEXT("UnknownSettingError", "未知设置项。"));
		return;
	}

	FString NormalizedValue;
	FText ErrorText;
	if (!ValidateRowValue(*Row, Event.NewValue, NormalizedValue, ErrorText))
	{
		SetRowErrorAndBroadcast(Event.DotPath, ErrorText);
		return;
	}

	FString StoreError;
	if (!FBlueprintHelperSettingStore::UpdateProjectSettingValue(Event.DotPath, NormalizedValue, StoreError))
	{
		SetRowErrorAndBroadcast(Event.DotPath, FText::FromString(StoreError));
		return;
	}

	RowErrorsByPath.Remove(Event.DotPath);
	View = FBlueprintHelperSettingStore::Load();
	ReloadRows();
	RowsChanged.Broadcast();
}

void FBlueprintHelperSettingsPresenter::HandleSettingResetRequested(const FString& DotPath)
{
	FString StoreError;
	if (!FBlueprintHelperSettingStore::ResetProjectSettingValue(DotPath, StoreError))
	{
		SetRowErrorAndBroadcast(DotPath, FText::FromString(StoreError));
		return;
	}

	RowErrorsByPath.Remove(DotPath);
	View = FBlueprintHelperSettingStore::Load();
	ReloadRows();
	RowsChanged.Broadcast();
}

bool FBlueprintHelperSettingsPresenter::ValidateRowValue(const FBlueprintHelperSettingRowViewModel& Row, const FString& NewValue, FString& OutNormalizedValue, FText& OutErrorText) const
{
	switch (Row.ValueType)
	{
	case EBlueprintHelperSettingValueType::Number:
	{
		double Parsed = 0.0;
		if (!LexTryParseString(Parsed, *NewValue))
		{
			OutErrorText = LOCTEXT("SettingErrorNumber", "请输入数字。");
			return false;
		}
		if (Row.bHasMinValue && Parsed < Row.MinValue)
		{
			OutErrorText = FText::Format(LOCTEXT("SettingErrorMin", "数值不能小于 {0}。"), FText::AsNumber(Row.MinValue));
			return false;
		}
		if (Row.bHasMaxValue && Parsed > Row.MaxValue)
		{
			OutErrorText = FText::Format(LOCTEXT("SettingErrorMax", "数值不能大于 {0}。"), FText::AsNumber(Row.MaxValue));
			return false;
		}
		OutNormalizedValue = FString::SanitizeFloat(Parsed);
		return true;
	}
	case EBlueprintHelperSettingValueType::Integer:
	{
		int32 Parsed = 0;
		if (!LexTryParseString(Parsed, *NewValue))
		{
			OutErrorText = LOCTEXT("SettingErrorInteger", "请输入整数。");
			return false;
		}
		if (Row.bHasMinValue && Parsed < Row.MinValue)
		{
			OutErrorText = FText::Format(LOCTEXT("SettingErrorMin", "数值不能小于 {0}。"), FText::AsNumber(Row.MinValue));
			return false;
		}
		if (Row.bHasMaxValue && Parsed > Row.MaxValue)
		{
			OutErrorText = FText::Format(LOCTEXT("SettingErrorMax", "数值不能大于 {0}。"), FText::AsNumber(Row.MaxValue));
			return false;
		}
		OutNormalizedValue = LexToString(Parsed);
		return true;
	}
	case EBlueprintHelperSettingValueType::Boolean:
		OutNormalizedValue = NewValue.Equals(TEXT("true"), ESearchCase::IgnoreCase) ? TEXT("true") : TEXT("false");
		return true;
	case EBlueprintHelperSettingValueType::Choice:
		for (const FBlueprintHelperSettingChoiceViewModel& Choice : Row.Choices)
		{
			if (Choice.Value == NewValue)
			{
				OutNormalizedValue = NewValue;
				return true;
			}
		}
		OutErrorText = LOCTEXT("SettingErrorChoice", "请选择列表中的有效选项。");
		return false;
	case EBlueprintHelperSettingValueType::Vector2:
	{
		TArray<double> Values;
		if (!UBlueprintHelperSettingsUIUtils::ParseNumberList(NewValue, 2, Values))
		{
			OutErrorText = LOCTEXT("SettingErrorVector2", "请输入两个数字，格式为 X,Y。");
			return false;
		}
		OutNormalizedValue = UBlueprintHelperSettingsUIUtils::NumberListToJsonArray(Values);
		return true;
	}
	case EBlueprintHelperSettingValueType::Margin:
	{
		TArray<double> Values;
		if (!UBlueprintHelperSettingsUIUtils::ParseNumberList(NewValue, 4, Values))
		{
			OutErrorText = LOCTEXT("SettingErrorMargin", "请输入四个数字，格式为左,上,右,下。");
			return false;
		}
		OutNormalizedValue = UBlueprintHelperSettingsUIUtils::NumberListToJsonArray(Values);
		return true;
	}
	case EBlueprintHelperSettingValueType::ColorArray:
	{
		TArray<double> Values;
		if (!UBlueprintHelperSettingsUIUtils::ParseNumberList(NewValue, 4, Values))
		{
			OutErrorText = LOCTEXT("SettingErrorColorArray", "请输入四个颜色值，格式为 [R,G,B,A]。");
			return false;
		}
		for (double Value : Values)
		{
			if (Value < 0.0 || Value > 1.0)
			{
				OutErrorText = LOCTEXT("SettingErrorColorRange", "颜色值必须在 0 到 1 之间。");
				return false;
			}
		}
		OutNormalizedValue = UBlueprintHelperSettingsUIUtils::NumberListToJsonArray(Values);
		return true;
	}
	case EBlueprintHelperSettingValueType::String:
	default:
		OutNormalizedValue = NewValue;
		return true;
	}
}

const FBlueprintHelperSettingRowViewModel* FBlueprintHelperSettingsPresenter::FindRowByPath(const FString& DotPath) const
{
	return Rows.FindByPredicate([&DotPath](const FBlueprintHelperSettingRowViewModel& Row)
	{
		return Row.DotPath == DotPath;
	});
}

void FBlueprintHelperSettingsPresenter::SetRowErrorAndBroadcast(const FString& DotPath, const FText& ErrorText)
{
	RowErrorsByPath.FindOrAdd(DotPath) = ErrorText.ToString();
	ReloadRows();
	RowsChanged.Broadcast();
}

#undef LOCTEXT_NAMESPACE
