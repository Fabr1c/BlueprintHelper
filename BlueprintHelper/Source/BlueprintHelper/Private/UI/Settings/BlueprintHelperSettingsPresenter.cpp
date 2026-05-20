// BlueprintHelper settings presenter implementation.

#include "UI/Settings/BlueprintHelperSettingsPresenter.h"

#include "Systems/Config/BlueprintHelperSafetyProfileResolver.h"

#define LOCTEXT_NAMESPACE "BlueprintHelperSettingsPresenter"

namespace
{
static bool IsRuntimeConsumedSetting(const FString& DotPath)
{
	static const TSet<FString> ExactConsumedPaths = {
		TEXT("ui.review_panel.diff_frame_outer_padding"),
		TEXT("ui.review_panel.diff_action_padding"),
		TEXT("ui.review_panel.diff_action_spacing"),
		TEXT("ui.review_panel.surface_overlay_fill_alpha"),
		TEXT("ui.review_panel.surface_overlay_selected_fill_alpha"),
		TEXT("ui.review_panel.surface_geometry_padding"),
		TEXT("ui.review_panel.debug_max_messages"),
		TEXT("review.debug_bundle.retention"),
		TEXT("debug.export_profile"),
		TEXT("debug.contains_full_settings"),
		TEXT("profiles.default.safety_profile"),
		TEXT("safety.preview_required"),
		TEXT("safety.write_approval_required"),
		TEXT("safety.approval_bypass"),
		TEXT("tool_clusters.signature.reference_context_max_results"),
		TEXT("tool_clusters.signature.reference_context_search_scope"),
		TEXT("tool_clusters.signature.reference_context_resolution_policy"),
		TEXT("tool_clusters.signature.reference_context_detail"),
		TEXT("tool_clusters.read_context.max_output_rows"),
		TEXT("tool_clusters.read_context.max_output_bytes"),
		TEXT("tool_clusters.component.default_attach_rule"),
		TEXT("tool_clusters.component.default_name_collision_policy"),
		TEXT("tool_clusters.component.default_property_mode"),
		TEXT("tool_clusters.component.dry_run"),
		TEXT("tool_clusters.blueprint_variables.dry_run"),
		TEXT("tool_clusters.blueprint_variables.read_member_defaults_scope"),
		TEXT("tool_clusters.blueprint_variables.asset_path_fallback"),
		TEXT("tool_clusters.graph_write.strict"),
		TEXT("tool_clusters.graph_write.create_missing_variables"),
		TEXT("tool_clusters.graph_write.reconstruct_existing_nodes"),
		TEXT("tool_clusters.graph_write.compile"),
		TEXT("tool_clusters.graph_write.save"),
		TEXT("tool_clusters.graph_write.layout"),
		TEXT("tool_clusters.graph_write.dry_run"),
		TEXT("graph_layout.rules_source")
	};

	if (ExactConsumedPaths.Contains(DotPath))
	{
		return true;
	}

	return DotPath.StartsWith(TEXT("tool_clusters.asset_factory.")) ||
		DotPath.StartsWith(TEXT("tool_clusters.class_settings.")) ||
		DotPath.StartsWith(TEXT("tool_clusters.object_property.")) ||
		DotPath.StartsWith(TEXT("tool_clusters.data_table.")) ||
		DotPath.StartsWith(TEXT("tool_clusters.umg_widget."));
}

static bool ShouldShowDeveloperSettings()
{
	return FBlueprintHelperSafetyProfileResolver::IsAutoRepair();
}

static FString ReadSettingValueOrDefault(const FString& DotPath, FString& OutDefaultValue, bool& bOutHasProjectOverride)
{
	FString CurrentValue;
	FString Error;
	bOutHasProjectOverride = false;
	FBlueprintHelperSettingStore::GetSettingValue(DotPath, CurrentValue, OutDefaultValue, bOutHasProjectOverride, Error);
	return CurrentValue;
}

static FBlueprintHelperSettingRowViewModel MakeBaseRow(
	const FString& DotPath,
	const FText& Category,
	const FText& Label,
	const FText& Hint,
	EBlueprintHelperSettingValueType ValueType,
	bool bDeveloperOnly = false)
{
	FString DefaultValue;
	bool bHasProjectOverride = false;
	const FString CurrentValue = ReadSettingValueOrDefault(DotPath, DefaultValue, bHasProjectOverride);

	FBlueprintHelperSettingRowViewModel Row;
	Row.DotPath = DotPath;
	Row.CategoryLabel = Category;
	Row.DisplayLabel = Label;
	Row.OverlapHint = Hint;
	Row.ValueType = ValueType;
	Row.CurrentValue = CurrentValue;
	Row.DefaultValue = DefaultValue;
	Row.bModified = bHasProjectOverride && CurrentValue != DefaultValue;
	Row.bDeveloperOnly = bDeveloperOnly;
	Row.bRuntimeConsumed = IsRuntimeConsumedSetting(DotPath);
	Row.AccessStatusText = bDeveloperOnly ? TEXT("Developer only") : TEXT("User editable");
	Row.ConsumerStatusText = Row.bRuntimeConsumed ? TEXT("Runtime consumed") : TEXT("Not yet consumed");
	return Row;
}

static FBlueprintHelperSettingRowViewModel MakeNumberRow(
	const FString& DotPath,
	const FText& Category,
	const FText& Label,
	const FText& Hint,
	double MinValue,
	double MaxValue,
	bool bDeveloperOnly = false)
{
	FBlueprintHelperSettingRowViewModel Row = MakeBaseRow(DotPath, Category, Label, Hint, EBlueprintHelperSettingValueType::Number, bDeveloperOnly);
	Row.MinValue = MinValue;
	Row.MaxValue = MaxValue;
	Row.bHasMinValue = true;
	Row.bHasMaxValue = true;
	return Row;
}

static FBlueprintHelperSettingRowViewModel MakeIntegerRow(
	const FString& DotPath,
	const FText& Category,
	const FText& Label,
	const FText& Hint,
	int32 MinValue,
	int32 MaxValue,
	bool bDeveloperOnly = false)
{
	FBlueprintHelperSettingRowViewModel Row = MakeBaseRow(DotPath, Category, Label, Hint, EBlueprintHelperSettingValueType::Integer, bDeveloperOnly);
	Row.MinValue = MinValue;
	Row.MaxValue = MaxValue;
	Row.bHasMinValue = true;
	Row.bHasMaxValue = true;
	return Row;
}

static FBlueprintHelperSettingRowViewModel MakeBooleanRow(
	const FString& DotPath,
	const FText& Category,
	const FText& Label,
	const FText& Hint,
	bool bDeveloperOnly = false)
{
	return MakeBaseRow(DotPath, Category, Label, Hint, EBlueprintHelperSettingValueType::Boolean, bDeveloperOnly);
}

static FBlueprintHelperSettingRowViewModel MakeChoiceRow(
	const FString& DotPath,
	const FText& Category,
	const FText& Label,
	const FText& Hint,
	TArray<FBlueprintHelperSettingChoiceViewModel> Choices,
	bool bDeveloperOnly = false)
{
	FBlueprintHelperSettingRowViewModel Row = MakeBaseRow(DotPath, Category, Label, Hint, EBlueprintHelperSettingValueType::Choice, bDeveloperOnly);
	Row.Choices = MoveTemp(Choices);
	return Row;
}

static FBlueprintHelperSettingRowViewModel MakeVector2Row(
	const FString& DotPath,
	const FText& Category,
	const FText& Label,
	const FText& Hint,
	double MinValue,
	double MaxValue,
	bool bDeveloperOnly = false)
{
	FBlueprintHelperSettingRowViewModel Row = MakeBaseRow(DotPath, Category, Label, Hint, EBlueprintHelperSettingValueType::Vector2, bDeveloperOnly);
	Row.MinValue = MinValue;
	Row.MaxValue = MaxValue;
	Row.bHasMinValue = true;
	Row.bHasMaxValue = true;
	return Row;
}

static FBlueprintHelperSettingRowViewModel MakeMarginRow(
	const FString& DotPath,
	const FText& Category,
	const FText& Label,
	const FText& Hint,
	double MinValue,
	double MaxValue,
	bool bDeveloperOnly = false)
{
	FBlueprintHelperSettingRowViewModel Row = MakeBaseRow(DotPath, Category, Label, Hint, EBlueprintHelperSettingValueType::Margin, bDeveloperOnly);
	Row.MinValue = MinValue;
	Row.MaxValue = MaxValue;
	Row.bHasMinValue = true;
	Row.bHasMaxValue = true;
	return Row;
}

static FBlueprintHelperSettingRowViewModel MakeStringRow(
	const FString& DotPath,
	const FText& Category,
	const FText& Label,
	const FText& Hint,
	bool bDeveloperOnly = false)
{
	return MakeBaseRow(DotPath, Category, Label, Hint, EBlueprintHelperSettingValueType::String, bDeveloperOnly);
}

static FBlueprintHelperSettingRowViewModel MakeColorArrayRow(
	const FString& DotPath,
	const FText& Category,
	const FText& Label,
	const FText& Hint,
	bool bDeveloperOnly = false)
{
	FBlueprintHelperSettingRowViewModel Row = MakeBaseRow(DotPath, Category, Label, Hint, EBlueprintHelperSettingValueType::ColorArray, bDeveloperOnly);
	Row.MinValue = 0.0;
	Row.MaxValue = 1.0;
	Row.bHasMinValue = true;
	Row.bHasMaxValue = true;
	return Row;
}

static bool ParseNumberList(const FString& Input, int32 ExpectedCount, TArray<double>& OutValues)
{
	FString NormalizedInput = Input.TrimStartAndEnd();
	if (NormalizedInput.StartsWith(TEXT("[")) && NormalizedInput.EndsWith(TEXT("]")))
	{
		NormalizedInput = NormalizedInput.Mid(1, NormalizedInput.Len() - 2).TrimStartAndEnd();
	}

	TArray<FString> Parts;
	NormalizedInput.ParseIntoArray(Parts, TEXT(","), true);
	if (Parts.Num() != ExpectedCount)
	{
		return false;
	}

	for (const FString& Part : Parts)
	{
		double Parsed = 0.0;
		if (!LexTryParseString(Parsed, *Part.TrimStartAndEnd()))
		{
			return false;
		}
		OutValues.Add(Parsed);
	}
	return true;
}

static FString NumberListToJsonArray(const TArray<double>& Values)
{
	TArray<FString> Parts;
	for (double Value : Values)
	{
		Parts.Add(FString::SanitizeFloat(Value));
	}
	return FString::Printf(TEXT("[%s]"), *FString::Join(Parts, TEXT(",")));
}
}

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
	const FText DeveloperToolClusterCategory = LOCTEXT("SettingsCategoryDeveloperToolCluster", "Developer ToolCluster");
	const FText SafetyCategory = LOCTEXT("SettingsCategorySafety", "Safety");
	const FText DeveloperUiCategory = LOCTEXT("SettingsCategoryDeveloperUi", "Developer UI");
	const FText DeveloperGraphLayoutCategory = LOCTEXT("SettingsCategoryDeveloperGraphLayout", "Developer GraphLayout");

	Rows.Add(MakeNumberRow(
		TEXT("ui.review_panel.diff_frame_outer_padding"),
		ReviewVisualCategory,
		LOCTEXT("DiffFrameOuterPaddingLabel", "Diff 外边距"),
		LOCTEXT("DiffFrameOuterPaddingHint", "调整 Diff 框与被标记区域之间的外扩距离。只影响 Review 可视化，不改变资产。"),
		0.0,
		64.0));
	Rows.Add(MakeNumberRow(
		TEXT("ui.review_panel.diff_action_padding"),
		ReviewVisualCategory,
		LOCTEXT("DiffActionPaddingLabel", "操作按钮内边距"),
		LOCTEXT("DiffActionPaddingHint", "调整 Accept / Reject 按钮区域的内边距。"),
		0.0,
		64.0));
	Rows.Add(MakeMarginRow(
		TEXT("ui.review_panel.diff_action_spacing"),
		ReviewVisualCategory,
		LOCTEXT("DiffActionSpacingLabel", "操作按钮间距"),
		LOCTEXT("DiffActionSpacingHint", "调整 Accept / Reject 按钮之间的间距，格式为左,上,右,下。"),
		0.0,
		64.0));
	Rows.Add(MakeNumberRow(
		TEXT("ui.review_panel.surface_overlay_fill_alpha"),
		ReviewVisualCategory,
		LOCTEXT("SurfaceOverlayFillAlphaLabel", "Diff 填充透明度"),
		LOCTEXT("SurfaceOverlayFillAlphaHint", "控制普通 Diff 区域背景填充透明度，取值范围 0 到 1。"),
		0.0,
		1.0));
	Rows.Add(MakeNumberRow(
		TEXT("ui.review_panel.surface_overlay_selected_fill_alpha"),
		ReviewVisualCategory,
		LOCTEXT("SurfaceOverlaySelectedFillAlphaLabel", "选中 Diff 填充透明度"),
		LOCTEXT("SurfaceOverlaySelectedFillAlphaHint", "控制选中 Diff 区域背景填充透明度，取值范围 0 到 1。"),
		0.0,
		1.0));
	Rows.Add(MakeVector2Row(
		TEXT("ui.review_panel.surface_geometry_padding"),
		ReviewVisualCategory,
		LOCTEXT("SurfaceGeometryPaddingLabel", "Diff 几何外扩"),
		LOCTEXT("SurfaceGeometryPaddingHint", "调整 Surface 几何匹配后的 Diff 绘制外扩量，格式为 X,Y。"),
		0.0,
		128.0));
	Rows.Add(MakeIntegerRow(
		TEXT("ui.review_panel.debug_max_messages"),
		ReviewDebugCategory,
		LOCTEXT("DebugMaxMessagesLabel", "Debug 最大消息数"),
		LOCTEXT("DebugMaxMessagesHint", "控制 Review Debug 面板保留的最近消息数量。"),
		10,
		5000));
	Rows.Add(MakeChoiceRow(
		TEXT("review.debug_bundle.retention"),
		ReviewDebugCategory,
		LOCTEXT("DebugBundleRetentionLabel", "DebugBundle 保留策略"),
		LOCTEXT("DebugBundleRetentionHint", "控制 Review DebugBundle 的保留方式。"),
		{
			{ TEXT("standard"), LOCTEXT("DebugBundleRetentionStandard", "标准") },
			{ TEXT("aggressive"), LOCTEXT("DebugBundleRetentionAggressive", "积极清理") },
			{ TEXT("keep_all"), LOCTEXT("DebugBundleRetentionKeepAll", "保留全部") }
		}));
	Rows.Add(MakeChoiceRow(
		TEXT("debug.export_profile"),
		DebugExportCategory,
		LOCTEXT("DebugExportProfileLabel", "Debug 导出级别"),
		LOCTEXT("DebugExportProfileHint", "控制 DebugBundle 导出的详细程度。"),
		{
			{ TEXT("minimal"), LOCTEXT("DebugExportProfileMinimal", "最小") },
			{ TEXT("standard"), LOCTEXT("DebugExportProfileStandard", "标准") },
			{ TEXT("full"), LOCTEXT("DebugExportProfileFull", "完整") }
		}));
	Rows.Add(MakeBooleanRow(
		TEXT("debug.contains_full_settings"),
		DebugExportCategory,
		LOCTEXT("DebugContainsFullSettingsLabel", "导出完整设置"),
		LOCTEXT("DebugContainsFullSettingsHint", "控制 DebugBundle 是否包含完整设置快照。")));
	Rows.Add(MakeChoiceRow(
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
	Rows.Add(MakeBooleanRow(
		TEXT("safety.preview_required"),
		SafetyCategory,
		LOCTEXT("SafetyPreviewRequiredLabel", "需要 Preview"),
		LOCTEXT("SafetyPreviewRequiredHint", "开启后，写入执行前必须先通过 Preview。"),
		true));
	Rows.Add(MakeBooleanRow(
		TEXT("safety.write_approval_required"),
		SafetyCategory,
		LOCTEXT("SafetyWriteApprovalRequiredLabel", "需要写入批准"),
		LOCTEXT("SafetyWriteApprovalRequiredHint", "开启后，写入执行前需要弹窗批准。"),
		true));
	Rows.Add(MakeBooleanRow(
		TEXT("safety.approval_bypass"),
		SafetyCategory,
		LOCTEXT("SafetyApprovalBypassLabel", "跳过批准弹窗"),
		LOCTEXT("SafetyApprovalBypassHint", "允许受信任的 AutoRepair 流程跳过写请求弹窗。"),
		true));
	Rows.Add(MakeIntegerRow(
		TEXT("tool_clusters.signature.reference_context_max_results"),
		ToolOutputCategory,
		LOCTEXT("SignatureReferenceContextMaxResultsLabel", "签名引用最大结果数"),
		LOCTEXT("SignatureReferenceContextMaxResultsHint", "控制签名引用上下文最多返回多少条结果。"),
		0,
		1000));
	Rows.Add(MakeIntegerRow(
		TEXT("tool_clusters.read_context.max_output_rows"),
		ToolOutputCategory,
		LOCTEXT("ReadContextMaxOutputRowsLabel", "Read Context 最大行数"),
		LOCTEXT("ReadContextMaxOutputRowsHint", "控制 Read Context 输出最大行数，0 表示不限制。"),
		0,
		100000));
	Rows.Add(MakeIntegerRow(
		TEXT("tool_clusters.read_context.max_output_bytes"),
		ToolOutputCategory,
		LOCTEXT("ReadContextMaxOutputBytesLabel", "Read Context 最大字节数"),
		LOCTEXT("ReadContextMaxOutputBytesHint", "控制 Read Context 输出最大字节数，0 表示不限制。"),
		0,
		104857600));

	if (ShouldShowDeveloperSettings())
	{
		Rows.Add(MakeStringRow(
			TEXT("tool_clusters.asset_factory.default_parent_class"),
			DeveloperToolClusterCategory,
			LOCTEXT("AssetFactoryDefaultParentClassLabel", "AssetFactory parent class"),
			LOCTEXT("AssetFactoryDefaultParentClassHint", "Developer-only default parent_class for asset factory requests."),
			true));
		Rows.Add(MakeStringRow(
			TEXT("tool_clusters.asset_factory.default_value_type"),
			DeveloperToolClusterCategory,
			LOCTEXT("AssetFactoryDefaultValueTypeLabel", "AssetFactory value type"),
			LOCTEXT("AssetFactoryDefaultValueTypeHint", "Developer-only default value_type for asset factory requests."),
			true));
		Rows.Add(MakeStringRow(
			TEXT("tool_clusters.asset_factory.default_collision_policy"),
			DeveloperToolClusterCategory,
			LOCTEXT("AssetFactoryDefaultCollisionPolicyLabel", "AssetFactory collision policy"),
			LOCTEXT("AssetFactoryDefaultCollisionPolicyHint", "Developer-only default collision policy for asset factory requests."),
			true));
		Rows.Add(MakeBooleanRow(
			TEXT("tool_clusters.asset_factory.dry_run"),
			DeveloperToolClusterCategory,
			LOCTEXT("AssetFactoryDryRunLabel", "AssetFactory dry run"),
			LOCTEXT("AssetFactoryDryRunHint", "Developer-only default dry_run for asset factory requests."),
			true));
		Rows.Add(MakeStringRow(
			TEXT("tool_clusters.signature.reference_context_search_scope"),
			DeveloperToolClusterCategory,
			LOCTEXT("SignatureReferenceContextSearchScopeLabel", "Signature search scope"),
			LOCTEXT("SignatureReferenceContextSearchScopeHint", "Developer-only default for signature reference-context search_scope."),
			true));
		Rows.Add(MakeStringRow(
			TEXT("tool_clusters.signature.reference_context_resolution_policy"),
			DeveloperToolClusterCategory,
			LOCTEXT("SignatureReferenceContextResolutionPolicyLabel", "Signature resolution policy"),
			LOCTEXT("SignatureReferenceContextResolutionPolicyHint", "Developer-only default for signature reference-context resolution_policy."),
			true));
		Rows.Add(MakeStringRow(
			TEXT("tool_clusters.signature.reference_context_detail"),
			DeveloperToolClusterCategory,
			LOCTEXT("SignatureReferenceContextDetailLabel", "Signature detail"),
			LOCTEXT("SignatureReferenceContextDetailHint", "Developer-only default for signature reference-context detail."),
			true));
		Rows.Add(MakeStringRow(
			TEXT("tool_clusters.component.default_attach_rule"),
			DeveloperToolClusterCategory,
			LOCTEXT("ComponentDefaultAttachRuleLabel", "Component attach rule"),
			LOCTEXT("ComponentDefaultAttachRuleHint", "Developer-only default for component attach_rule."),
			true));
		Rows.Add(MakeStringRow(
			TEXT("tool_clusters.component.default_name_collision_policy"),
			DeveloperToolClusterCategory,
			LOCTEXT("ComponentDefaultNameCollisionPolicyLabel", "Component collision policy"),
			LOCTEXT("ComponentDefaultNameCollisionPolicyHint", "Developer-only default for component name_collision_policy."),
			true));
		Rows.Add(MakeStringRow(
			TEXT("tool_clusters.component.default_property_mode"),
			DeveloperToolClusterCategory,
			LOCTEXT("ComponentDefaultPropertyModeLabel", "Component property mode"),
			LOCTEXT("ComponentDefaultPropertyModeHint", "Developer-only default for component property_mode."),
			true));
		Rows.Add(MakeBooleanRow(
			TEXT("tool_clusters.component.dry_run"),
			DeveloperToolClusterCategory,
			LOCTEXT("ComponentDryRunLabel", "Component dry run"),
			LOCTEXT("ComponentDryRunHint", "Developer-only default for component write dry_run."),
			true));
		Rows.Add(MakeBooleanRow(
			TEXT("tool_clusters.class_settings.dry_run"),
			DeveloperToolClusterCategory,
			LOCTEXT("ClassSettingsDryRunLabel", "Class settings dry run"),
			LOCTEXT("ClassSettingsDryRunHint", "Developer-only default dry_run for class settings requests."),
			true));
		Rows.Add(MakeBooleanRow(
			TEXT("tool_clusters.class_settings.validation_should_compile"),
			DeveloperToolClusterCategory,
			LOCTEXT("ClassSettingsValidationCompileLabel", "Class settings compile"),
			LOCTEXT("ClassSettingsValidationCompileHint", "Developer-only validation compile default for class settings requests."),
			true));
		Rows.Add(MakeBooleanRow(
			TEXT("tool_clusters.class_settings.validation_should_save"),
			DeveloperToolClusterCategory,
			LOCTEXT("ClassSettingsValidationSaveLabel", "Class settings save"),
			LOCTEXT("ClassSettingsValidationSaveHint", "Developer-only validation save default for class settings requests."),
			true));
		Rows.Add(MakeBooleanRow(
			TEXT("tool_clusters.blueprint_variables.dry_run"),
			DeveloperToolClusterCategory,
			LOCTEXT("BlueprintVariablesDryRunLabel", "Blueprint variables dry run"),
			LOCTEXT("BlueprintVariablesDryRunHint", "Developer-only default for blueprint variable write dry_run."),
			true));
		Rows.Add(MakeStringRow(
			TEXT("tool_clusters.blueprint_variables.read_member_defaults_scope"),
			DeveloperToolClusterCategory,
			LOCTEXT("BlueprintVariablesReadMemberDefaultsScopeLabel", "Member defaults scope"),
			LOCTEXT("BlueprintVariablesReadMemberDefaultsScopeHint", "Developer-only read scope label for member-default reads."),
			true));
		Rows.Add(MakeStringRow(
			TEXT("tool_clusters.blueprint_variables.asset_path_fallback"),
			DeveloperToolClusterCategory,
			LOCTEXT("BlueprintVariablesAssetPathFallbackLabel", "Asset path fallback"),
			LOCTEXT("BlueprintVariablesAssetPathFallbackHint", "Developer-only fallback target label when asset_path is omitted for read rows."),
			true));
		Rows.Add(MakeBooleanRow(
			TEXT("tool_clusters.object_property.dry_run"),
			DeveloperToolClusterCategory,
			LOCTEXT("ObjectPropertyDryRunLabel", "Object property dry run"),
			LOCTEXT("ObjectPropertyDryRunHint", "Developer-only default dry_run for object property requests."),
			true));
		Rows.Add(MakeBooleanRow(
			TEXT("tool_clusters.data_table.dry_run"),
			DeveloperToolClusterCategory,
			LOCTEXT("DataTableDryRunLabel", "DataTable dry run"),
			LOCTEXT("DataTableDryRunHint", "Developer-only default dry_run for data table requests."),
			true));
		Rows.Add(MakeBooleanRow(
			TEXT("tool_clusters.data_table.write_requires_row_struct"),
			DeveloperToolClusterCategory,
			LOCTEXT("DataTableWriteRequiresRowStructLabel", "DataTable row struct required"),
			LOCTEXT("DataTableWriteRequiresRowStructHint", "Developer-only row-struct requirement default for data table writes."),
			true));
		Rows.Add(MakeBooleanRow(
			TEXT("tool_clusters.umg_widget.dry_run"),
			DeveloperToolClusterCategory,
			LOCTEXT("UmgWidgetDryRunLabel", "UMG widget dry run"),
			LOCTEXT("UmgWidgetDryRunHint", "Developer-only default dry_run for UMG widget requests."),
			true));
		Rows.Add(MakeBooleanRow(
			TEXT("tool_clusters.umg_widget.asset_path_required"),
			DeveloperToolClusterCategory,
			LOCTEXT("UmgWidgetAssetPathRequiredLabel", "UMG asset path required"),
			LOCTEXT("UmgWidgetAssetPathRequiredHint", "Developer-only asset path requirement default for UMG widget requests."),
			true));
		Rows.Add(MakeBooleanRow(
			TEXT("tool_clusters.graph_write.strict"),
			DeveloperToolClusterCategory,
			LOCTEXT("GraphWriteStrictLabel", "GraphWrite strict"),
			LOCTEXT("GraphWriteStrictHint", "Developer-only default for graph_write strict mode."),
			true));
		Rows.Add(MakeBooleanRow(
			TEXT("tool_clusters.graph_write.create_missing_variables"),
			DeveloperToolClusterCategory,
			LOCTEXT("GraphWriteCreateMissingVariablesLabel", "Create missing variables"),
			LOCTEXT("GraphWriteCreateMissingVariablesHint", "Developer-only graph_write default for variable creation."),
			true));
		Rows.Add(MakeBooleanRow(
			TEXT("tool_clusters.graph_write.reconstruct_existing_nodes"),
			DeveloperToolClusterCategory,
			LOCTEXT("GraphWriteReconstructExistingNodesLabel", "Reconstruct existing nodes"),
			LOCTEXT("GraphWriteReconstructExistingNodesHint", "Developer-only graph_write default for reconstruct_existing_nodes."),
			true));
		Rows.Add(MakeBooleanRow(
			TEXT("tool_clusters.graph_write.compile"),
			DeveloperToolClusterCategory,
			LOCTEXT("GraphWriteCompileLabel", "GraphWrite compile"),
			LOCTEXT("GraphWriteCompileHint", "Developer-only graph_write validation compile default."),
			true));
		Rows.Add(MakeBooleanRow(
			TEXT("tool_clusters.graph_write.save"),
			DeveloperToolClusterCategory,
			LOCTEXT("GraphWriteSaveLabel", "GraphWrite save"),
			LOCTEXT("GraphWriteSaveHint", "Developer-only graph_write validation save default."),
			true));
		Rows.Add(MakeStringRow(
			TEXT("tool_clusters.graph_write.layout"),
			DeveloperToolClusterCategory,
			LOCTEXT("GraphWriteLayoutLabel", "GraphWrite layout"),
			LOCTEXT("GraphWriteLayoutHint", "Developer-only graph_write layout policy label."),
			true));
		Rows.Add(MakeStringRow(
			TEXT("graph_layout.rules_source"),
			DeveloperGraphLayoutCategory,
			LOCTEXT("GraphLayoutRulesSourceLabel", "GraphLayout rules source"),
			LOCTEXT("GraphLayoutRulesSourceHint", "Developer-only relative or safe absolute path for graph layout rules."),
			true));
		Rows.Add(MakeColorArrayRow(
			TEXT("ui.main_window.active_tab_color"),
			DeveloperUiCategory,
			LOCTEXT("MainWindowActiveTabColorLabel", "Active tab color"),
			LOCTEXT("MainWindowActiveTabColorHint", "Developer-only RGBA array. Format: [R,G,B,A]."),
			true));
		Rows.Add(MakeColorArrayRow(
			TEXT("ui.main_window.inactive_tab_color"),
			DeveloperUiCategory,
			LOCTEXT("MainWindowInactiveTabColorLabel", "Inactive tab color"),
			LOCTEXT("MainWindowInactiveTabColorHint", "Developer-only RGBA array. Format: [R,G,B,A]."),
			true));
		Rows.Add(MakeColorArrayRow(
			TEXT("ui.task_spec_workbench.block_colors.default"),
			DeveloperUiCategory,
			LOCTEXT("WorkbenchDefaultBlockColorLabel", "Workbench default block color"),
			LOCTEXT("WorkbenchDefaultBlockColorHint", "Developer-only RGBA array. Format: [R,G,B,A]."),
			true));
		Rows.Add(MakeColorArrayRow(
			TEXT("ui.task_spec_workbench.block_colors.graph_logic"),
			DeveloperUiCategory,
			LOCTEXT("WorkbenchGraphLogicBlockColorLabel", "Workbench graph logic color"),
			LOCTEXT("WorkbenchGraphLogicBlockColorHint", "Developer-only RGBA array. Format: [R,G,B,A]."),
			true));
		Rows.Add(MakeColorArrayRow(
			TEXT("ui.task_spec_workbench.block_colors.diagnostic"),
			DeveloperUiCategory,
			LOCTEXT("WorkbenchDiagnosticBlockColorLabel", "Workbench diagnostic color"),
			LOCTEXT("WorkbenchDiagnosticBlockColorHint", "Developer-only RGBA array. Format: [R,G,B,A]."),
			true));
		Rows.Add(MakeColorArrayRow(
			TEXT("ui.task_spec_workbench.block_colors.selected"),
			DeveloperUiCategory,
			LOCTEXT("WorkbenchSelectedBlockColorLabel", "Workbench selected color"),
			LOCTEXT("WorkbenchSelectedBlockColorHint", "Developer-only RGBA array. Format: [R,G,B,A]."),
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
		if (!ParseNumberList(NewValue, 2, Values))
		{
			OutErrorText = LOCTEXT("SettingErrorVector2", "请输入两个数字，格式为 X,Y。");
			return false;
		}
		OutNormalizedValue = NumberListToJsonArray(Values);
		return true;
	}
	case EBlueprintHelperSettingValueType::Margin:
	{
		TArray<double> Values;
		if (!ParseNumberList(NewValue, 4, Values))
		{
			OutErrorText = LOCTEXT("SettingErrorMargin", "请输入四个数字，格式为左,上,右,下。");
			return false;
		}
		OutNormalizedValue = NumberListToJsonArray(Values);
		return true;
	}
	case EBlueprintHelperSettingValueType::ColorArray:
	{
		TArray<double> Values;
		if (!ParseNumberList(NewValue, 4, Values))
		{
			OutErrorText = LOCTEXT("SettingErrorColorArray", "Enter four color values as [R,G,B,A].");
			return false;
		}
		for (double Value : Values)
		{
			if (Value < 0.0 || Value > 1.0)
			{
				OutErrorText = LOCTEXT("SettingErrorColorRange", "Color values must be between 0 and 1.");
				return false;
			}
		}
		OutNormalizedValue = NumberListToJsonArray(Values);
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
