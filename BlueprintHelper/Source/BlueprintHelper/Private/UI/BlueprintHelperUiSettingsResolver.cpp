// BlueprintHelper UI runtime settings resolver implementation.

#include "UI/BlueprintHelperUiSettingsResolver.h"

#include "Dom/JsonValue.h"
#include "Systems/Config/BlueprintHelperRuntimeSettingResolver.h"

namespace BlueprintHelperUiSettingsResolverLocal
{
	FLinearColor ResolveColor(const FString& DotPath, const FLinearColor& DefaultValue)
	{
		const TSharedPtr<FJsonValue> Value = FBlueprintHelperRuntimeSettingResolver::GetJsonValue(DotPath);
		if (!Value.IsValid() || Value->Type != EJson::Array)
		{
			return DefaultValue;
		}

		const TArray<TSharedPtr<FJsonValue>>& Components = Value->AsArray();
		if (Components.Num() < 3)
		{
			return DefaultValue;
		}

		float ParsedComponents[4] = {
			DefaultValue.R,
			DefaultValue.G,
			DefaultValue.B,
			DefaultValue.A
		};
		const int32 ComponentCount = FMath::Min(Components.Num(), 4);
		for (int32 Index = 0; Index < ComponentCount; ++Index)
		{
			if (!Components[Index].IsValid() || Components[Index]->Type != EJson::Number)
			{
				return DefaultValue;
			}
			ParsedComponents[Index] = static_cast<float>(Components[Index]->AsNumber());
		}

		return FLinearColor(
			ParsedComponents[0],
			ParsedComponents[1],
			ParsedComponents[2],
			ParsedComponents[3]);
	}
}

FBlueprintHelperMainWindowSettings FBlueprintHelperUiSettingsResolver::LoadMainWindowSettings()
{
	FBlueprintHelperMainWindowSettings Settings;
	Settings.DefaultTab = FBlueprintHelperRuntimeSettingResolver::GetString(
		TEXT("ui.main_window.default_tab"),
		Settings.DefaultTab);
	Settings.TabBarPadding = static_cast<float>(FBlueprintHelperRuntimeSettingResolver::GetDouble(
		TEXT("ui.main_window.tab_bar_padding"),
		Settings.TabBarPadding));
	Settings.TabButtonSpacing = FBlueprintHelperRuntimeSettingResolver::GetMargin(
		TEXT("ui.main_window.tab_button_spacing"),
		Settings.TabButtonSpacing);
	Settings.ActiveTabColor = BlueprintHelperUiSettingsResolverLocal::ResolveColor(
		TEXT("ui.main_window.active_tab_color"),
		Settings.ActiveTabColor);
	Settings.InactiveTabColor = BlueprintHelperUiSettingsResolverLocal::ResolveColor(
		TEXT("ui.main_window.inactive_tab_color"),
		Settings.InactiveTabColor);
	Settings.CleanupButtonLabel = FText::FromString(FBlueprintHelperRuntimeSettingResolver::GetString(
		TEXT("ui.main_window.cleanup_button_label"),
		Settings.CleanupButtonLabel.ToString()));
	Settings.CleanupButtonMarginLeft = static_cast<float>(FBlueprintHelperRuntimeSettingResolver::GetDouble(
		TEXT("ui.main_window.cleanup_button_margin_left"),
		Settings.CleanupButtonMarginLeft));
	return Settings;
}

FBlueprintHelperNotificationSettings FBlueprintHelperUiSettingsResolver::LoadNotificationSettings()
{
	FBlueprintHelperNotificationSettings Settings;
	Settings.bCleanupUseThrobber = FBlueprintHelperRuntimeSettingResolver::GetBool(
		TEXT("ui.notifications.cleanup_use_throbber"),
		Settings.bCleanupUseThrobber);
	Settings.bCleanupUseSuccessFailIcons = FBlueprintHelperRuntimeSettingResolver::GetBool(
		TEXT("ui.notifications.cleanup_use_success_fail_icons"),
		Settings.bCleanupUseSuccessFailIcons);
	Settings.bCleanupFireAndForget = FBlueprintHelperRuntimeSettingResolver::GetBool(
		TEXT("ui.notifications.cleanup_fire_and_forget"),
		Settings.bCleanupFireAndForget);
	Settings.CleanupFadeOutSeconds = static_cast<float>(FBlueprintHelperRuntimeSettingResolver::GetDouble(
		TEXT("ui.notifications.cleanup_fade_out_seconds"),
		Settings.CleanupFadeOutSeconds));
	Settings.CleanupExpireSeconds = static_cast<float>(FBlueprintHelperRuntimeSettingResolver::GetDouble(
		TEXT("ui.notifications.cleanup_expire_seconds"),
		Settings.CleanupExpireSeconds));
	return Settings;
}

FBlueprintHelperTaskSpecWorkbenchSettings FBlueprintHelperUiSettingsResolver::LoadTaskSpecWorkbenchSettings()
{
	FBlueprintHelperTaskSpecWorkbenchSettings Settings;
	Settings.TopPadding = static_cast<float>(FBlueprintHelperRuntimeSettingResolver::GetDouble(
		TEXT("ui.task_spec_workbench.top_padding"),
		Settings.TopPadding));
	Settings.ButtonSpacing = FBlueprintHelperRuntimeSettingResolver::GetMargin(
		TEXT("ui.task_spec_workbench.button_spacing"),
		Settings.ButtonSpacing);
	Settings.MainSplitRatio = FBlueprintHelperRuntimeSettingResolver::GetVector2(
		TEXT("ui.task_spec_workbench.main_split_ratio"),
		Settings.MainSplitRatio);
	Settings.LeftSplitRatio = FBlueprintHelperRuntimeSettingResolver::GetVector2(
		TEXT("ui.task_spec_workbench.left_split_ratio"),
		Settings.LeftSplitRatio);
	Settings.PreviewWidth = static_cast<float>(FBlueprintHelperRuntimeSettingResolver::GetDouble(
		TEXT("ui.task_spec_workbench.preview_width"),
		Settings.PreviewWidth));
	Settings.PreviewMinHeight = static_cast<float>(FBlueprintHelperRuntimeSettingResolver::GetDouble(
		TEXT("ui.task_spec_workbench.preview_min_height"),
		Settings.PreviewMinHeight));
	Settings.PreviewContainerPadding = static_cast<float>(FBlueprintHelperRuntimeSettingResolver::GetDouble(
		TEXT("ui.task_spec_workbench.preview_container_padding"),
		Settings.PreviewContainerPadding));
	Settings.DefaultBlockColor = BlueprintHelperUiSettingsResolverLocal::ResolveColor(
		TEXT("ui.task_spec_workbench.block_colors.default"),
		Settings.DefaultBlockColor);
	Settings.GraphLogicBlockColor = BlueprintHelperUiSettingsResolverLocal::ResolveColor(
		TEXT("ui.task_spec_workbench.block_colors.graph_logic"),
		Settings.GraphLogicBlockColor);
	Settings.DiagnosticBlockColor = BlueprintHelperUiSettingsResolverLocal::ResolveColor(
		TEXT("ui.task_spec_workbench.block_colors.diagnostic"),
		Settings.DiagnosticBlockColor);
	Settings.SelectedBlockColor = BlueprintHelperUiSettingsResolverLocal::ResolveColor(
		TEXT("ui.task_spec_workbench.block_colors.selected"),
		Settings.SelectedBlockColor);
	return Settings;
}

FBlueprintHelperLayoutRuleEditorSettings FBlueprintHelperUiSettingsResolver::LoadLayoutRuleEditorSettings()
{
	FBlueprintHelperLayoutRuleEditorSettings Settings;
	Settings.CanvasDesiredSize = FBlueprintHelperRuntimeSettingResolver::GetVector2(
		TEXT("ui.layout_rule_editor.canvas_desired_size"),
		Settings.CanvasDesiredSize);
	Settings.NodeSize = FBlueprintHelperRuntimeSettingResolver::GetVector2(
		TEXT("ui.layout_rule_editor.node_size"),
		Settings.NodeSize);
	Settings.CanvasRuleScale = static_cast<float>(FBlueprintHelperRuntimeSettingResolver::GetDouble(
		TEXT("ui.layout_rule_editor.canvas_rule_scale"),
		Settings.CanvasRuleScale));
	Settings.DefaultRuleId = FBlueprintHelperRuntimeSettingResolver::GetString(
		TEXT("ui.layout_rule_editor.default_rule_id"),
		Settings.DefaultRuleId);
	Settings.DefaultRuleDisplayName = FBlueprintHelperRuntimeSettingResolver::GetString(
		TEXT("ui.layout_rule_editor.default_rule_display_name"),
		Settings.DefaultRuleDisplayName);
	Settings.ExecColumnSpacing = static_cast<float>(FBlueprintHelperRuntimeSettingResolver::GetDouble(
		TEXT("ui.layout_rule_editor.exec_column_spacing"),
		Settings.ExecColumnSpacing));
	Settings.ExecRowSpacing = static_cast<float>(FBlueprintHelperRuntimeSettingResolver::GetDouble(
		TEXT("ui.layout_rule_editor.exec_row_spacing"),
		Settings.ExecRowSpacing));
	Settings.BranchRowSpacing = static_cast<float>(FBlueprintHelperRuntimeSettingResolver::GetDouble(
		TEXT("ui.layout_rule_editor.branch_row_spacing"),
		Settings.BranchRowSpacing));
	Settings.PureInputOffsetX = static_cast<float>(FBlueprintHelperRuntimeSettingResolver::GetDouble(
		TEXT("ui.layout_rule_editor.pure_input_offset_x"),
		Settings.PureInputOffsetX));
	Settings.VariableInputOffsetX = static_cast<float>(FBlueprintHelperRuntimeSettingResolver::GetDouble(
		TEXT("ui.layout_rule_editor.variable_input_offset_x"),
		Settings.VariableInputOffsetX));
	Settings.InputPinRowSpacing = static_cast<float>(FBlueprintHelperRuntimeSettingResolver::GetDouble(
		TEXT("ui.layout_rule_editor.input_pin_row_spacing"),
		Settings.InputPinRowSpacing));
	Settings.MaxMillisecondsPerFrame = static_cast<float>(FBlueprintHelperRuntimeSettingResolver::GetDouble(
		TEXT("ui.layout_rule_editor.max_ms_per_frame"),
		Settings.MaxMillisecondsPerFrame));
	Settings.MaxNodesPerFrame = FBlueprintHelperRuntimeSettingResolver::GetInt(
		TEXT("ui.layout_rule_editor.max_nodes_per_frame"),
		Settings.MaxNodesPerFrame);
	Settings.bMoveGeneratedNodes = FBlueprintHelperRuntimeSettingResolver::GetBool(
		TEXT("ui.layout_rule_editor.move_generated_nodes"),
		Settings.bMoveGeneratedNodes);
	Settings.bMoveExistingNodes = FBlueprintHelperRuntimeSettingResolver::GetBool(
		TEXT("ui.layout_rule_editor.move_existing_nodes"),
		Settings.bMoveExistingNodes);
	Settings.bMarkDirtyAfterApply = FBlueprintHelperRuntimeSettingResolver::GetBool(
		TEXT("ui.layout_rule_editor.mark_dirty_after_apply"),
		Settings.bMarkDirtyAfterApply);
	Settings.bSaveAfterApply = FBlueprintHelperRuntimeSettingResolver::GetBool(
		TEXT("ui.layout_rule_editor.save_after_apply"),
		Settings.bSaveAfterApply);
	Settings.SideSplitterRatio = FBlueprintHelperRuntimeSettingResolver::GetVector2(
		TEXT("ui.layout_rule_editor.side_splitter_ratio"),
		Settings.SideSplitterRatio);
	return Settings;
}