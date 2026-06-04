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
	const FText DebugScreenshotCategory = LOCTEXT("SettingsCategoryDebugScreenshot", "截图证据");

	const FText ReviewVisualCategory = LOCTEXT("SettingsCategoryReviewVisual", "Review 可视化");
	const FText ReviewDebugCategory = LOCTEXT("SettingsCategoryReviewDebug", "Review 调试");
	const FText DebugExportCategory = LOCTEXT("SettingsCategoryDebugExport", "调试导出");
	const FText ToolOutputCategory = LOCTEXT("SettingsCategoryToolOutput", "工具输出");
	const FText DeveloperDryRunCategory = LOCTEXT("SettingsCategoryDeveloperDryRun", "DryRun");
	const FText DeveloperRuntimeCategory = LOCTEXT("SettingsCategoryDeveloperRuntime", "开发者 Runtime");
	const FText DeveloperTaskRuntimeCategory = LOCTEXT("SettingsCategoryDeveloperTaskRuntime", "开发者 TaskRuntime");
	const FText DeveloperToolClusterCategory = LOCTEXT("SettingsCategoryDeveloperToolCluster", "开发者 ToolCluster");
	const FText DeveloperReviewCategory = LOCTEXT("SettingsCategoryDeveloperReview", "开发者 Review");
	const FText SafetyCategory = LOCTEXT("SettingsCategorySafety", "安全");
	const FText DeveloperSafetyCategory = LOCTEXT("SettingsCategoryDeveloperSafety", "开发者安全");
	const FText DeveloperUiCategory = LOCTEXT("SettingsCategoryDeveloperUi", "开发者 UI");
	const FText DeveloperLayoutRuleEditorCategory = LOCTEXT("SettingsCategoryDeveloperLayoutRuleEditor", "开发者 布局规则编辑器");
	const FText DeveloperTaskSpecWorkbenchCategory = LOCTEXT("SettingsCategoryDeveloperTaskSpecWorkbench", "开发者 TaskSpec 工作台");
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
	Rows.Add(UBlueprintHelperSettingsUIUtils::MakeStringRow(
		TEXT("debug.screenshot.output_dir"),
		DebugScreenshotCategory,
		LOCTEXT("DebugScreenshotOutputDirLabel", "截图输出目录"),
		LOCTEXT("DebugScreenshotOutputDirHint", "编辑器截图证据在 BlueprintHelper Debug 根目录下使用的相对目录。")));
	Rows.Add(UBlueprintHelperSettingsUIUtils::MakeChoiceRow(
		TEXT("debug.screenshot.default_capture_target"),
		DebugScreenshotCategory,
		LOCTEXT("DebugScreenshotDefaultTargetLabel", "默认截图目标"),
		LOCTEXT("DebugScreenshotDefaultTargetHint", "capture_editor_screenshot 未指定 target 时使用的默认截图目标。"),
		{
			{ TEXT("active_window"), LOCTEXT("DebugScreenshotTargetActiveWindow", "活动窗口") },
			{ TEXT("active_viewport"), LOCTEXT("DebugScreenshotTargetActiveViewport", "活动视口") }
		}));
	Rows.Add(UBlueprintHelperSettingsUIUtils::MakeStringRow(
		TEXT("debug.screenshot.filename_prefix"),
		DebugScreenshotCategory,
		LOCTEXT("DebugScreenshotFilenamePrefixLabel", "截图文件名前缀"),
		LOCTEXT("DebugScreenshotFilenamePrefixHint", "请求未提供 label 时使用的默认安全文件名前缀。")));
	Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
		TEXT("debug.screenshot.graph_max_nodes_per_image"),
		DebugScreenshotCategory,
		LOCTEXT("DebugScreenshotGraphMaxNodesPerImageLabel", "Graph max nodes per PNG"),
		LOCTEXT("DebugScreenshotGraphMaxNodesPerImageHint", "Maximum selected Graph nodes captured in one independent PNG."),
		1,
		64));
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
	Rows.Add(UBlueprintHelperSettingsUIUtils::MakeStringRow(
		TEXT("cli.artifacts.default_output_dir"),
		ToolOutputCategory,
		LOCTEXT("CliArtifactDefaultOutputDirLabel", "CLI Artifact 默认目录"),
		LOCTEXT("CliArtifactDefaultOutputDirHint", "控制 CLI 未传 --artifact-dir 且未设置 BPH_CLI_ARTIFACT_DIR 时写入 result.json 的默认目录；相对路径基于项目根目录。")));

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
			TEXT("active_profile"),
			DeveloperSafetyCategory,
			LOCTEXT("ActiveProfileLabel", "当前 Profile"),
			LOCTEXT("ActiveProfileHint", "选择用于解析 profiles.<name>.safety_profile 的运行时 Profile；默认使用 default。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
			TEXT("runtime.bridge.port"),
			DeveloperRuntimeCategory,
			LOCTEXT("RuntimeBridgePortLabel", "Bridge 端口"),
			LOCTEXT("RuntimeBridgePortHint", "控制 UE Bridge 监听端口。修改后通常需要重启 Bridge 或编辑器。"),
			1,
			65535,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
			TEXT("runtime.bridge.max_pending_connections"),
			DeveloperRuntimeCategory,
			LOCTEXT("RuntimeBridgeMaxPendingConnectionsLabel", "Bridge 最大等待连接"),
			LOCTEXT("RuntimeBridgeMaxPendingConnectionsHint", "控制 Bridge socket 允许排队等待的连接数量。"),
			1,
			1024,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
			TEXT("runtime.bridge.accept_wait_ms"),
			DeveloperRuntimeCategory,
			LOCTEXT("RuntimeBridgeAcceptWaitMsLabel", "Bridge Accept 等待毫秒"),
			LOCTEXT("RuntimeBridgeAcceptWaitMsHint", "控制 Bridge 接受连接时每轮等待的毫秒数。"),
			1,
			60000,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
			TEXT("runtime.bridge.idle_timeout_seconds"),
			DeveloperRuntimeCategory,
			LOCTEXT("RuntimeBridgeIdleTimeoutSecondsLabel", "Bridge 空闲超时秒数"),
			LOCTEXT("RuntimeBridgeIdleTimeoutSecondsHint", "控制 Bridge socket 空闲连接超时。"),
			0.01,
			3600.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
			TEXT("runtime.bridge.max_frame_bytes"),
			DeveloperRuntimeCategory,
			LOCTEXT("RuntimeBridgeMaxFrameBytesLabel", "Bridge 最大帧字节数"),
			LOCTEXT("RuntimeBridgeMaxFrameBytesHint", "控制 Bridge 单帧消息允许的最大字节数。"),
			1,
			2147483647,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
			TEXT("runtime.bridge.socket_buffer_bytes"),
			DeveloperRuntimeCategory,
			LOCTEXT("RuntimeBridgeSocketBufferBytesLabel", "Bridge Socket 缓冲字节数"),
			LOCTEXT("RuntimeBridgeSocketBufferBytesHint", "控制 Bridge socket 收发缓冲区大小。"),
			4096,
			2147483647,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
			TEXT("runtime.task_runtime.cache.partial_preview.ttl_seconds"),
			DeveloperTaskRuntimeCategory,
			LOCTEXT("TaskRuntimePartialPreviewTtlLabel", "PartialPreview 缓存 TTL"),
			LOCTEXT("TaskRuntimePartialPreviewTtlHint", "控制 TaskRuntime partial preview 缓存保留秒数。"),
			0.0,
			3600.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
			TEXT("runtime.task_runtime.cache.partial_preview.max_groups"),
			DeveloperTaskRuntimeCategory,
			LOCTEXT("TaskRuntimePartialPreviewMaxGroupsLabel", "PartialPreview 最大组数"),
			LOCTEXT("TaskRuntimePartialPreviewMaxGroupsHint", "控制 partial preview 缓存允许保留的分组数量。"),
			1,
			100000,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
			TEXT("runtime.task_runtime.cache.partial_preview.max_step_entries"),
			DeveloperTaskRuntimeCategory,
			LOCTEXT("TaskRuntimePartialPreviewMaxStepEntriesLabel", "PartialPreview 最大步骤条目"),
			LOCTEXT("TaskRuntimePartialPreviewMaxStepEntriesHint", "控制 partial preview 缓存允许保留的步骤条目数量。"),
			1,
			100000,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
			TEXT("runtime.task_runtime.cache.partial_preview.max_bytes"),
			DeveloperTaskRuntimeCategory,
			LOCTEXT("TaskRuntimePartialPreviewMaxBytesLabel", "PartialPreview 最大字节数"),
			LOCTEXT("TaskRuntimePartialPreviewMaxBytesHint", "控制 partial preview 缓存允许占用的最大字节数。"),
			1,
			2147483647,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
			TEXT("runtime.task_runtime.cache.call_function_fact.ttl_seconds"),
			DeveloperTaskRuntimeCategory,
			LOCTEXT("TaskRuntimeCallFunctionFactTtlLabel", "CallFunctionFact 缓存 TTL"),
			LOCTEXT("TaskRuntimeCallFunctionFactTtlHint", "控制函数调用事实缓存保留秒数。"),
			0.0,
			3600.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
			TEXT("runtime.task_runtime.cache.call_function_fact.max_entries"),
			DeveloperTaskRuntimeCategory,
			LOCTEXT("TaskRuntimeCallFunctionFactMaxEntriesLabel", "CallFunctionFact 最大条目"),
			LOCTEXT("TaskRuntimeCallFunctionFactMaxEntriesHint", "控制函数调用事实缓存允许保留的最大条目数。"),
			1,
			1000000,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
			TEXT("runtime.task_runtime.cache.call_function_fact.max_bytes"),
			DeveloperTaskRuntimeCategory,
			LOCTEXT("TaskRuntimeCallFunctionFactMaxBytesLabel", "CallFunctionFact 最大字节数"),
			LOCTEXT("TaskRuntimeCallFunctionFactMaxBytesHint", "控制函数调用事实缓存允许占用的最大字节数。"),
			1,
			2147483647,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
			TEXT("runtime.task_runtime.cache.graph_write_plan.ttl_seconds"),
			DeveloperTaskRuntimeCategory,
			LOCTEXT("TaskRuntimeGraphWritePlanTtlLabel", "GraphWritePlan 缓存 TTL"),
			LOCTEXT("TaskRuntimeGraphWritePlanTtlHint", "控制 GraphWrite TaskPlan 缓存保留秒数。"),
			0.0,
			3600.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
			TEXT("runtime.task_runtime.cache.graph_write_plan.max_entries"),
			DeveloperTaskRuntimeCategory,
			LOCTEXT("TaskRuntimeGraphWritePlanMaxEntriesLabel", "GraphWritePlan 最大条目"),
			LOCTEXT("TaskRuntimeGraphWritePlanMaxEntriesHint", "控制 GraphWrite TaskPlan 缓存允许保留的最大条目数。"),
			1,
			100000,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
			TEXT("runtime.task_runtime.cache.graph_write_plan.max_bytes"),
			DeveloperTaskRuntimeCategory,
			LOCTEXT("TaskRuntimeGraphWritePlanMaxBytesLabel", "GraphWritePlan 最大字节数"),
			LOCTEXT("TaskRuntimeGraphWritePlanMaxBytesHint", "控制 GraphWrite TaskPlan 缓存允许占用的最大字节数。"),
			1,
			2147483647,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
			TEXT("runtime.task_runtime.cache.prune_on_access_min_interval_seconds"),
			DeveloperTaskRuntimeCategory,
			LOCTEXT("TaskRuntimeCachePruneIntervalLabel", "缓存裁剪最小间隔"),
			LOCTEXT("TaskRuntimeCachePruneIntervalHint", "控制 TaskRuntime 缓存访问时触发裁剪的最小间隔秒数。"),
			0.0,
			3600.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("runtime.task_runtime.execution_policy.should_compile"),
			DeveloperTaskRuntimeCategory,
			LOCTEXT("TaskRuntimeExecutionPolicyShouldCompileLabel", "TaskRuntime 默认编译"),
			LOCTEXT("TaskRuntimeExecutionPolicyShouldCompileHint", "控制 TaskPlan 未覆盖时是否默认执行编译后操作。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("runtime.task_runtime.execution_policy.should_save"),
			DeveloperTaskRuntimeCategory,
			LOCTEXT("TaskRuntimeExecutionPolicyShouldSaveLabel", "TaskRuntime 默认保存"),
			LOCTEXT("TaskRuntimeExecutionPolicyShouldSaveHint", "控制 TaskPlan 未覆盖时是否默认执行保存后操作。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeChoiceRow(
			TEXT("runtime.task_runtime.execution_policy.dry_run_mode"),
			DeveloperTaskRuntimeCategory,
			LOCTEXT("TaskRuntimeExecutionPolicyDryRunModeLabel", "TaskRuntime DryRun 模式"),
			LOCTEXT("TaskRuntimeExecutionPolicyDryRunModeHint", "控制 TaskPlan 未覆盖时使用的默认 dry_run_mode。"),
			{
				{ TEXT("full"), LOCTEXT("TaskRuntimeDryRunModeFull", "完整预演") },
				{ TEXT("quick"), LOCTEXT("TaskRuntimeDryRunModeQuick", "快速预演") },
				{ TEXT("none"), LOCTEXT("TaskRuntimeDryRunModeNone", "实际写入") }
			},
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("review.evidence_required"),
			DeveloperReviewCategory,
			LOCTEXT("ReviewEvidenceRequiredLabel", "要求 Review Evidence"),
			LOCTEXT("ReviewEvidenceRequiredHint", "控制 Review 写入和回滚流程是否要求 evidence。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeStringRow(
			TEXT("review.artifact.snapshot_root"),
			DeveloperReviewCategory,
			LOCTEXT("ReviewArtifactSnapshotRootLabel", "Review 快照目录"),
			LOCTEXT("ReviewArtifactSnapshotRootHint", "控制 Review 语义快照写入目录；相对路径基于项目目录解析。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeStringRow(
			TEXT("review.debug_bundle.root_dir"),
			DeveloperReviewCategory,
			LOCTEXT("ReviewDebugBundleRootDirLabel", "DebugBundle 根目录"),
			LOCTEXT("ReviewDebugBundleRootDirHint", "控制 Review DebugBundle 根目录；相对路径基于项目目录解析。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeStringRow(
			TEXT("review.debug_bundle.sub_dir"),
			DeveloperReviewCategory,
			LOCTEXT("ReviewDebugBundleSubDirLabel", "DebugBundle 子目录"),
			LOCTEXT("ReviewDebugBundleSubDirHint", "控制 Review DebugBundle 在根目录下使用的子目录名称。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeStringRow(
			TEXT("review.debug_bundle.filename_pattern"),
			DeveloperReviewCategory,
			LOCTEXT("ReviewDebugBundleFilenamePatternLabel", "DebugBundle 文件名模式"),
			LOCTEXT("ReviewDebugBundleFilenamePatternHint", "控制 Review DebugBundle 导出的文件名格式。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("review.debug_bundle.enforce_root_path"),
			DeveloperReviewCategory,
			LOCTEXT("ReviewDebugBundleEnforceRootPathLabel", "限制 DebugBundle 根路径"),
			LOCTEXT("ReviewDebugBundleEnforceRootPathHint", "开启后，DebugBundle 输出必须位于配置的根目录内。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
			TEXT("review.performance.trace_warning_ms"),
			DeveloperReviewCategory,
			LOCTEXT("ReviewPerformanceTraceWarningMsLabel", "Review 性能日志阈值"),
			LOCTEXT("ReviewPerformanceTraceWarningMsHint", "控制 Review 性能计时超过多少毫秒后写入 Warning 日志；0 表示所有计时都按 Warning 记录。"),
			0,
			60000,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
			TEXT("review.performance.main_window_page_construct_warning_ms"),
			DeveloperReviewCategory,
			LOCTEXT("ReviewPerformancePageConstructWarningMsLabel", "页面构造日志阈值"),
			LOCTEXT("ReviewPerformancePageConstructWarningMsHint", "控制 BlueprintHelper 主窗口懒构造页面超过多少毫秒后写入 Warning 日志。"),
			0,
			60000,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
			TEXT("review.performance.pending_load_page_size"),
			DeveloperReviewCategory,
			LOCTEXT("ReviewPerformancePendingLoadPageSizeLabel", "Pending 分页大小"),
			LOCTEXT("ReviewPerformancePendingLoadPageSizeHint", "控制 Review 面板每次从 pending index 加载多少条可见变更。数值越大，滚动次数越少，但单次 GameThread 应用成本越高。"),
			1,
			1000,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
			TEXT("review.performance.pending_load_scroll_prefetch_rows"),
			DeveloperReviewCategory,
			LOCTEXT("ReviewPerformancePendingLoadScrollPrefetchRowsLabel", "Pending 滚动预加载行数"),
			LOCTEXT("ReviewPerformancePendingLoadScrollPrefetchRowsHint", "控制 Review 变更树距离底部还剩多少行时提前加载下一页。设为 0 表示滚动到当前页末尾才加载。"),
			0,
			500,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
			TEXT("review.performance.pending_load_validity_candidate_budget"),
			DeveloperReviewCategory,
			LOCTEXT("ReviewPerformancePendingLoadValidityCandidateBudgetLabel", "Pending 校验候选数量"),
			LOCTEXT("ReviewPerformancePendingLoadValidityCandidateBudgetHint", "控制一次 pending load 最多向低速有效性扫描队列提交多少个 ReviewEvent 校验候选。"),
			0,
			100000,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("review.performance.validity_sweep_enabled"),
			DeveloperReviewCategory,
			LOCTEXT("ReviewPerformanceValiditySweepEnabledLabel", "启用低速有效性扫描"),
			LOCTEXT("ReviewPerformanceValiditySweepEnabledHint", "开启后，BlueprintHelperWidget 打开时会低速检查 pending ReviewEvent 对应的真实资产、变量、函数和其他锚点是否仍然存在。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
			TEXT("review.performance.validity_sweep_max_record_hydrations_per_worker_batch"),
			DeveloperReviewCategory,
			LOCTEXT("ReviewPerformanceValiditySweepWorkerBatchLabel", "每批读取 ReviewRecord 数"),
			LOCTEXT("ReviewPerformanceValiditySweepWorkerBatchHint", "控制低速有效性扫描 worker 每批最多读取多少个 ReviewRecord JSON 来生成纯数据候选。"),
			0,
			10000,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
			TEXT("review.performance.validity_sweep_max_game_thread_targets_per_frame"),
			DeveloperReviewCategory,
			LOCTEXT("ReviewPerformanceValiditySweepTargetsPerFrameLabel", "每帧校验目标数"),
			LOCTEXT("ReviewPerformanceValiditySweepTargetsPerFrameHint", "控制低速有效性扫描每帧最多在 GameThread 校验多少个真实资产锚点。"),
			0,
			1000,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
			TEXT("review.performance.validity_sweep_max_game_thread_ms_per_frame"),
			DeveloperReviewCategory,
			LOCTEXT("ReviewPerformanceValiditySweepMsPerFrameLabel", "每帧校验毫秒数"),
			LOCTEXT("ReviewPerformanceValiditySweepMsPerFrameHint", "控制低速有效性扫描每帧最多占用多少毫秒 GameThread 时间。"),
			0.0,
			100.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
			TEXT("review.performance.validity_sweep_max_invalid_purges_per_batch"),
			DeveloperReviewCategory,
			LOCTEXT("ReviewPerformanceValiditySweepInvalidPurgeBatchLabel", "每批清理无效项数"),
			LOCTEXT("ReviewPerformanceValiditySweepInvalidPurgeBatchHint", "控制低速有效性扫描一次最多向 ReviewStore 提交多少个无效 ReviewEvent target 清理结果。"),
			0,
			10000,
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
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
			TEXT("tool_clusters.graph_write.action_resolution.max_candidates"),
			DeveloperToolClusterCategory,
			LOCTEXT("GraphWriteActionResolutionMaxCandidatesLabel", "ActionResolution 最大候选"),
			LOCTEXT("GraphWriteActionResolutionMaxCandidatesHint", "控制 GraphWrite 动作解析保留的最大候选数量。"),
			1,
			1000,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeStringRow(
			TEXT("tool_clusters.graph_write.action_resolution.default_search_mode"),
			DeveloperToolClusterCategory,
			LOCTEXT("GraphWriteActionResolutionDefaultSearchModeLabel", "ActionResolution 默认搜索模式"),
			LOCTEXT("GraphWriteActionResolutionDefaultSearchModeHint", "控制 GraphWrite 动作解析未指定 search_mode 时使用的默认值。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeStringRow(
			TEXT("tool_clusters.graph_write.action_resolution.default_ambiguity_policy"),
			DeveloperToolClusterCategory,
			LOCTEXT("GraphWriteActionResolutionDefaultAmbiguityPolicyLabel", "ActionResolution 歧义策略"),
			LOCTEXT("GraphWriteActionResolutionDefaultAmbiguityPolicyHint", "控制 GraphWrite 动作解析未指定 ambiguity_policy 时使用的默认值。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
			TEXT("tool_clusters.graph_write.action_resolution.auto_search.max_candidates_per_statement"),
			DeveloperToolClusterCategory,
			LOCTEXT("GraphWriteActionResolutionAutoSearchCandidatesLabel", "AutoSearch 每语句候选"),
			LOCTEXT("GraphWriteActionResolutionAutoSearchCandidatesHint", "控制 GraphWrite AutoSearch Preview 每条语句返回的候选上限。"),
			1,
			10,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
			TEXT("tool_clusters.graph_write.action_resolution.auto_search.max_statements"),
			DeveloperToolClusterCategory,
			LOCTEXT("GraphWriteActionResolutionAutoSearchStatementsLabel", "AutoSearch 语句预算"),
			LOCTEXT("GraphWriteActionResolutionAutoSearchStatementsHint", "控制 GraphWrite AutoSearch Preview 单次最多处理的语句数。"),
			1,
			64,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
			TEXT("tool_clusters.graph_write.action_resolution.auto_search.max_total_ms"),
			DeveloperToolClusterCategory,
			LOCTEXT("GraphWriteActionResolutionAutoSearchMsLabel", "AutoSearch 时间预算"),
			LOCTEXT("GraphWriteActionResolutionAutoSearchMsHint", "控制 GraphWrite AutoSearch Preview 单次搜索的毫秒上限。"),
			1,
			1000,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeStringRow(
			TEXT("tool_clusters.graph_write.action_resolution.auto_search.detail_level"),
			DeveloperToolClusterCategory,
			LOCTEXT("GraphWriteActionResolutionAutoSearchDetailLabel", "AutoSearch 详情级别"),
			LOCTEXT("GraphWriteActionResolutionAutoSearchDetailHint", "控制 GraphWrite AutoSearch 候选输出使用 short 或 diagnostic。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeStringRow(
			TEXT("graph_layout.rules_source"),
			DeveloperGraphLayoutCategory,
			LOCTEXT("GraphLayoutRulesSourceLabel", "GraphLayout 规则来源"),
			LOCTEXT("GraphLayoutRulesSourceHint", "GraphLayout 规则文件路径；允许项目配置目录下的相对路径，或受信任目录内的绝对路径。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeVector2Row(
			TEXT("ui.review_panel.main_split_ratio"),
			DeveloperUiCategory,
			LOCTEXT("ReviewPanelMainSplitRatioLabel", "ReviewPanel 主分割比例"),
			LOCTEXT("ReviewPanelMainSplitRatioHint", "控制 ReviewPanel 左右主区域分割比例，格式为 X,Y。"),
			0.0,
			1.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeVector2Row(
			TEXT("ui.review_panel.component_blueprint_split"),
			DeveloperUiCategory,
			LOCTEXT("ReviewPanelComponentBlueprintSplitLabel", "组件/蓝图分割比例"),
			LOCTEXT("ReviewPanelComponentBlueprintSplitHint", "控制 ReviewPanel 组件视图与蓝图视图分割比例，格式为 X,Y。"),
			0.0,
			1.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeVector2Row(
			TEXT("ui.review_panel.main_graph_ratio"),
			DeveloperUiCategory,
			LOCTEXT("ReviewPanelMainGraphRatioLabel", "主图区域比例"),
			LOCTEXT("ReviewPanelMainGraphRatioHint", "控制 ReviewPanel 主图区域布局比例，格式为 X,Y。"),
			0.0,
			1.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeVector2Row(
			TEXT("ui.review_panel.right_bottom_ratio"),
			DeveloperUiCategory,
			LOCTEXT("ReviewPanelRightBottomRatioLabel", "右下区域比例"),
			LOCTEXT("ReviewPanelRightBottomRatioHint", "控制 ReviewPanel 右下区域分割比例，格式为 X,Y。"),
			0.0,
			1.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeVector2Row(
			TEXT("ui.review_panel.root_row_padding"),
			DeveloperUiCategory,
			LOCTEXT("ReviewPanelRootRowPaddingLabel", "Root 行内边距"),
			LOCTEXT("ReviewPanelRootRowPaddingHint", "控制 ReviewPanel 根行的水平和垂直内边距，格式为 X,Y。"),
			0.0,
			128.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
			TEXT("ui.review_panel.row_content_padding"),
			DeveloperUiCategory,
			LOCTEXT("ReviewPanelRowContentPaddingLabel", "行内容内边距"),
			LOCTEXT("ReviewPanelRowContentPaddingHint", "控制 ReviewPanel 行内容与边界之间的内边距。"),
			0.0,
			128.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
			TEXT("ui.review_panel.flash_tick_decay"),
			DeveloperUiCategory,
			LOCTEXT("ReviewPanelFlashTickDecayLabel", "Flash 衰减"),
			LOCTEXT("ReviewPanelFlashTickDecayHint", "控制 ReviewPanel 行高亮 flash 衰减速度。"),
			0.0,
			60.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("ui.review_panel.overlay_filter_current_asset_only"),
			DeveloperUiCategory,
			LOCTEXT("ReviewPanelOverlayFilterCurrentAssetOnlyLabel", "Overlay 仅当前资产"),
			LOCTEXT("ReviewPanelOverlayFilterCurrentAssetOnlyHint", "控制 Review overlay 是否只显示当前资产相关的变更。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeStringRow(
			TEXT("ui.main_window.default_tab"),
			DeveloperUiCategory,
			LOCTEXT("MainWindowDefaultTabLabel", "主窗口默认 Tab"),
			LOCTEXT("MainWindowDefaultTabHint", "控制 BlueprintHelper 主窗口初次打开时选中的默认 Tab。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
			TEXT("ui.main_window.tab_bar_padding"),
			DeveloperUiCategory,
			LOCTEXT("MainWindowTabBarPaddingLabel", "Tab 栏内边距"),
			LOCTEXT("MainWindowTabBarPaddingHint", "控制主窗口 Tab 栏整体内边距。"),
			0.0,
			64.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeMarginRow(
			TEXT("ui.main_window.tab_button_spacing"),
			DeveloperUiCategory,
			LOCTEXT("MainWindowTabButtonSpacingLabel", "Tab 按钮间距"),
			LOCTEXT("MainWindowTabButtonSpacingHint", "控制主窗口 Tab 按钮之间的间距，格式为左,上,右,下。"),
			0.0,
			64.0,
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
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeStringRow(
			TEXT("ui.main_window.cleanup_button_label"),
			DeveloperUiCategory,
			LOCTEXT("MainWindowCleanupButtonLabelLabel", "清理按钮文本"),
			LOCTEXT("MainWindowCleanupButtonLabelHint", "控制主窗口清理 Review 数据按钮显示的文本。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
			TEXT("ui.main_window.cleanup_button_margin_left"),
			DeveloperUiCategory,
			LOCTEXT("MainWindowCleanupButtonMarginLeftLabel", "清理按钮左边距"),
			LOCTEXT("MainWindowCleanupButtonMarginLeftHint", "控制清理 Review 数据按钮左侧留白。"),
			0.0,
			128.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("ui.notifications.cleanup_use_throbber"),
			DeveloperUiCategory,
			LOCTEXT("NotificationCleanupUseThrobberLabel", "清理通知显示加载动画"),
			LOCTEXT("NotificationCleanupUseThrobberHint", "控制清理 Review 数据通知是否显示加载动画。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("ui.notifications.cleanup_use_success_fail_icons"),
			DeveloperUiCategory,
			LOCTEXT("NotificationCleanupUseSuccessFailIconsLabel", "清理通知显示结果图标"),
			LOCTEXT("NotificationCleanupUseSuccessFailIconsHint", "控制清理 Review 数据通知是否显示成功或失败图标。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("ui.notifications.cleanup_fire_and_forget"),
			DeveloperUiCategory,
			LOCTEXT("NotificationCleanupFireAndForgetLabel", "清理通知异步关闭"),
			LOCTEXT("NotificationCleanupFireAndForgetHint", "控制清理 Review 数据通知是否按异步完成方式关闭。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
			TEXT("ui.notifications.cleanup_fade_out_seconds"),
			DeveloperUiCategory,
			LOCTEXT("NotificationCleanupFadeOutSecondsLabel", "清理通知淡出秒数"),
			LOCTEXT("NotificationCleanupFadeOutSecondsHint", "控制清理 Review 数据通知淡出动画时长。"),
			0.0,
			60.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
			TEXT("ui.notifications.cleanup_expire_seconds"),
			DeveloperUiCategory,
			LOCTEXT("NotificationCleanupExpireSecondsLabel", "清理通知过期秒数"),
			LOCTEXT("NotificationCleanupExpireSecondsHint", "控制清理 Review 数据通知自动过期时间。"),
			0.0,
			600.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeVector2Row(
			TEXT("ui.layout_rule_editor.canvas_desired_size"),
			DeveloperLayoutRuleEditorCategory,
			LOCTEXT("LayoutRuleEditorCanvasDesiredSizeLabel", "LayoutRule 画布尺寸"),
			LOCTEXT("LayoutRuleEditorCanvasDesiredSizeHint", "控制 Layout Rule Editor 画布期望尺寸，格式为 X,Y。"),
			1.0,
			8192.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeVector2Row(
			TEXT("ui.layout_rule_editor.node_size"),
			DeveloperLayoutRuleEditorCategory,
			LOCTEXT("LayoutRuleEditorNodeSizeLabel", "LayoutRule 节点尺寸"),
			LOCTEXT("LayoutRuleEditorNodeSizeHint", "控制 Layout Rule Editor 预览节点尺寸，格式为 X,Y。"),
			1.0,
			2048.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
			TEXT("ui.layout_rule_editor.canvas_rule_scale"),
			DeveloperLayoutRuleEditorCategory,
			LOCTEXT("LayoutRuleEditorCanvasRuleScaleLabel", "LayoutRule 缩放"),
			LOCTEXT("LayoutRuleEditorCanvasRuleScaleHint", "控制 Layout Rule Editor 规则预览缩放比例。"),
			0.01,
			10.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeStringRow(
			TEXT("ui.layout_rule_editor.default_rule_id"),
			DeveloperLayoutRuleEditorCategory,
			LOCTEXT("LayoutRuleEditorDefaultRuleIdLabel", "LayoutRule 默认 ID"),
			LOCTEXT("LayoutRuleEditorDefaultRuleIdHint", "控制 Layout Rule Editor 新规则使用的默认规则 ID。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeStringRow(
			TEXT("ui.layout_rule_editor.default_rule_display_name"),
			DeveloperLayoutRuleEditorCategory,
			LOCTEXT("LayoutRuleEditorDefaultRuleDisplayNameLabel", "LayoutRule 默认名称"),
			LOCTEXT("LayoutRuleEditorDefaultRuleDisplayNameHint", "控制 Layout Rule Editor 新规则显示名称的默认值。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
			TEXT("ui.layout_rule_editor.exec_column_spacing"),
			DeveloperLayoutRuleEditorCategory,
			LOCTEXT("LayoutRuleEditorExecColumnSpacingLabel", "Exec 列间距"),
			LOCTEXT("LayoutRuleEditorExecColumnSpacingHint", "控制 Layout Rule Editor 中执行链列间距。"),
			0.0,
			4096.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
			TEXT("ui.layout_rule_editor.exec_row_spacing"),
			DeveloperLayoutRuleEditorCategory,
			LOCTEXT("LayoutRuleEditorExecRowSpacingLabel", "Exec 行间距"),
			LOCTEXT("LayoutRuleEditorExecRowSpacingHint", "控制 Layout Rule Editor 中执行链行间距。"),
			0.0,
			4096.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
			TEXT("ui.layout_rule_editor.branch_row_spacing"),
			DeveloperLayoutRuleEditorCategory,
			LOCTEXT("LayoutRuleEditorBranchRowSpacingLabel", "Branch 行间距"),
			LOCTEXT("LayoutRuleEditorBranchRowSpacingHint", "控制 Layout Rule Editor 中分支行间距。"),
			0.0,
			4096.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
			TEXT("ui.layout_rule_editor.pure_input_offset_x"),
			DeveloperLayoutRuleEditorCategory,
			LOCTEXT("LayoutRuleEditorPureInputOffsetXLabel", "Pure 输入 X 偏移"),
			LOCTEXT("LayoutRuleEditorPureInputOffsetXHint", "控制纯节点输入列相对主执行链的 X 偏移。"),
			0.0,
			4096.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
			TEXT("ui.layout_rule_editor.variable_input_offset_x"),
			DeveloperLayoutRuleEditorCategory,
			LOCTEXT("LayoutRuleEditorVariableInputOffsetXLabel", "变量输入 X 偏移"),
			LOCTEXT("LayoutRuleEditorVariableInputOffsetXHint", "控制变量输入列相对主执行链的 X 偏移。"),
			0.0,
			4096.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
			TEXT("ui.layout_rule_editor.input_pin_row_spacing"),
			DeveloperLayoutRuleEditorCategory,
			LOCTEXT("LayoutRuleEditorInputPinRowSpacingLabel", "输入 Pin 行间距"),
			LOCTEXT("LayoutRuleEditorInputPinRowSpacingHint", "控制 Layout Rule Editor 中输入 Pin 行间距。"),
			0.0,
			512.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
			TEXT("ui.layout_rule_editor.max_ms_per_frame"),
			DeveloperLayoutRuleEditorCategory,
			LOCTEXT("LayoutRuleEditorMaxMsPerFrameLabel", "每帧最大毫秒数"),
			LOCTEXT("LayoutRuleEditorMaxMsPerFrameHint", "控制 Layout Rule Editor 应用布局时单帧最多占用时间。"),
			0.0,
			1000.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
			TEXT("ui.layout_rule_editor.max_nodes_per_frame"),
			DeveloperLayoutRuleEditorCategory,
			LOCTEXT("LayoutRuleEditorMaxNodesPerFrameLabel", "每帧最大节点数"),
			LOCTEXT("LayoutRuleEditorMaxNodesPerFrameHint", "控制 Layout Rule Editor 应用布局时单帧最多移动节点数。"),
			1,
			100000,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("ui.layout_rule_editor.move_generated_nodes"),
			DeveloperLayoutRuleEditorCategory,
			LOCTEXT("LayoutRuleEditorMoveGeneratedNodesLabel", "移动生成节点"),
			LOCTEXT("LayoutRuleEditorMoveGeneratedNodesHint", "控制 Layout Rule Editor 是否移动本轮生成的节点。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("ui.layout_rule_editor.move_existing_nodes"),
			DeveloperLayoutRuleEditorCategory,
			LOCTEXT("LayoutRuleEditorMoveExistingNodesLabel", "移动已有节点"),
			LOCTEXT("LayoutRuleEditorMoveExistingNodesHint", "控制 Layout Rule Editor 是否移动图表中已有节点。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("ui.layout_rule_editor.mark_dirty_after_apply"),
			DeveloperLayoutRuleEditorCategory,
			LOCTEXT("LayoutRuleEditorMarkDirtyAfterApplyLabel", "应用后标记 Dirty"),
			LOCTEXT("LayoutRuleEditorMarkDirtyAfterApplyHint", "控制 Layout Rule Editor 应用布局后是否标记资产已修改。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
			TEXT("ui.layout_rule_editor.save_after_apply"),
			DeveloperLayoutRuleEditorCategory,
			LOCTEXT("LayoutRuleEditorSaveAfterApplyLabel", "应用后保存"),
			LOCTEXT("LayoutRuleEditorSaveAfterApplyHint", "控制 Layout Rule Editor 应用布局后是否立即保存资产。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeVector2Row(
			TEXT("ui.layout_rule_editor.side_splitter_ratio"),
			DeveloperLayoutRuleEditorCategory,
			LOCTEXT("LayoutRuleEditorSideSplitterRatioLabel", "LayoutRule 侧栏比例"),
			LOCTEXT("LayoutRuleEditorSideSplitterRatioHint", "控制 Layout Rule Editor 左右区域分割比例，格式为 X,Y。"),
			0.0,
			1.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
			TEXT("ui.task_spec_workbench.top_padding"),
			DeveloperTaskSpecWorkbenchCategory,
			LOCTEXT("WorkbenchTopPaddingLabel", "Workbench 顶部间距"),
			LOCTEXT("WorkbenchTopPaddingHint", "控制 TaskSpec Workbench 顶部工具条间距。"),
			0.0,
			128.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeMarginRow(
			TEXT("ui.task_spec_workbench.button_spacing"),
			DeveloperTaskSpecWorkbenchCategory,
			LOCTEXT("WorkbenchButtonSpacingLabel", "Workbench 按钮间距"),
			LOCTEXT("WorkbenchButtonSpacingHint", "控制 TaskSpec Workbench 按钮之间的间距，格式为左,上,右,下。"),
			0.0,
			128.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeVector2Row(
			TEXT("ui.task_spec_workbench.main_split_ratio"),
			DeveloperTaskSpecWorkbenchCategory,
			LOCTEXT("WorkbenchMainSplitRatioLabel", "Workbench 主分割比例"),
			LOCTEXT("WorkbenchMainSplitRatioHint", "控制 TaskSpec Workbench 主区域分割比例，格式为 X,Y。"),
			0.0,
			1.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeVector2Row(
			TEXT("ui.task_spec_workbench.left_split_ratio"),
			DeveloperTaskSpecWorkbenchCategory,
			LOCTEXT("WorkbenchLeftSplitRatioLabel", "Workbench 左侧分割比例"),
			LOCTEXT("WorkbenchLeftSplitRatioHint", "控制 TaskSpec Workbench 左侧区域分割比例，格式为 X,Y。"),
			0.0,
			1.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
			TEXT("ui.task_spec_workbench.preview_width"),
			DeveloperTaskSpecWorkbenchCategory,
			LOCTEXT("WorkbenchPreviewWidthLabel", "Workbench 预览宽度"),
			LOCTEXT("WorkbenchPreviewWidthHint", "控制 TaskSpec Workbench 预览列宽度。"),
			1.0,
			4096.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
			TEXT("ui.task_spec_workbench.preview_min_height"),
			DeveloperTaskSpecWorkbenchCategory,
			LOCTEXT("WorkbenchPreviewMinHeightLabel", "Workbench 预览最小高度"),
			LOCTEXT("WorkbenchPreviewMinHeightHint", "控制 TaskSpec Workbench 预览区域最小高度。"),
			1.0,
			4096.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeNumberRow(
			TEXT("ui.task_spec_workbench.preview_container_padding"),
			DeveloperTaskSpecWorkbenchCategory,
			LOCTEXT("WorkbenchPreviewContainerPaddingLabel", "Workbench 预览内边距"),
			LOCTEXT("WorkbenchPreviewContainerPaddingHint", "控制 TaskSpec Workbench 预览容器内边距。"),
			0.0,
			128.0,
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeColorArrayRow(
			TEXT("ui.task_spec_workbench.block_colors.default"),
			DeveloperTaskSpecWorkbenchCategory,
			LOCTEXT("WorkbenchDefaultBlockColorLabel", "Workbench 默认块颜色"),
			LOCTEXT("WorkbenchDefaultBlockColorHint", "TaskSpec Workbench 默认块的 RGBA 数组，格式：[R,G,B,A]。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeColorArrayRow(
			TEXT("ui.task_spec_workbench.block_colors.graph_logic"),
			DeveloperTaskSpecWorkbenchCategory,
			LOCTEXT("WorkbenchGraphLogicBlockColorLabel", "Workbench 图逻辑块颜色"),
			LOCTEXT("WorkbenchGraphLogicBlockColorHint", "TaskSpec Workbench 图逻辑块的 RGBA 数组，格式：[R,G,B,A]。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeColorArrayRow(
			TEXT("ui.task_spec_workbench.block_colors.diagnostic"),
			DeveloperTaskSpecWorkbenchCategory,
			LOCTEXT("WorkbenchDiagnosticBlockColorLabel", "Workbench 诊断块颜色"),
			LOCTEXT("WorkbenchDiagnosticBlockColorHint", "TaskSpec Workbench 诊断块的 RGBA 数组，格式：[R,G,B,A]。"),
			true));
		Rows.Add(UBlueprintHelperSettingsUIUtils::MakeColorArrayRow(
			TEXT("ui.task_spec_workbench.block_colors.selected"),
			DeveloperTaskSpecWorkbenchCategory,
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
