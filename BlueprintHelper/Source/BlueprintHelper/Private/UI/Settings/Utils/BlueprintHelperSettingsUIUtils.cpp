// BlueprintHelper Settings UI utility functions implementation.

#include "UI/Settings/Utils/BlueprintHelperSettingsUIUtils.h"

#include "Systems/Config/BlueprintHelperRuntimeSettingResolver.h"
#include "Systems/Config/BlueprintHelperSafetyProfileResolver.h"
#include "Systems/Config/BlueprintHelperSettingStore.h"

// === From BlueprintHelperSettingsPresenter.cpp ===

bool UBlueprintHelperSettingsUIUtils::IsRuntimeConsumedSetting(const FString& DotPath)
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

bool UBlueprintHelperSettingsUIUtils::ShouldShowDeveloperSettings()
{
	return FBlueprintHelperSafetyProfileResolver::IsAutoRepair();
}

FString UBlueprintHelperSettingsUIUtils::ReadSettingValueOrDefault(const FString& DotPath, FString& OutDefaultValue, bool& bOutHasProjectOverride)
{
	FString CurrentValue;
	FString Error;
	bOutHasProjectOverride = false;
	FBlueprintHelperSettingStore::GetSettingValue(DotPath, CurrentValue, OutDefaultValue, bOutHasProjectOverride, Error);
	return CurrentValue;
}

FBlueprintHelperSettingRowViewModel UBlueprintHelperSettingsUIUtils::MakeBaseRow(
	const FString& DotPath,
	const FText& Category,
	const FText& Label,
	const FText& Hint,
	EBlueprintHelperSettingValueType ValueType,
	bool bDeveloperOnly)
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

FBlueprintHelperSettingRowViewModel UBlueprintHelperSettingsUIUtils::MakeNumberRow(
	const FString& DotPath,
	const FText& Category,
	const FText& Label,
	const FText& Hint,
	double MinValue,
	double MaxValue,
	bool bDeveloperOnly)
{
	FBlueprintHelperSettingRowViewModel Row = UBlueprintHelperSettingsUIUtils::MakeBaseRow(DotPath, Category, Label, Hint, EBlueprintHelperSettingValueType::Number, bDeveloperOnly);
	Row.MinValue = MinValue;
	Row.MaxValue = MaxValue;
	Row.bHasMinValue = true;
	Row.bHasMaxValue = true;
	return Row;
}

FBlueprintHelperSettingRowViewModel UBlueprintHelperSettingsUIUtils::MakeIntegerRow(
	const FString& DotPath,
	const FText& Category,
	const FText& Label,
	const FText& Hint,
	int32 MinValue,
	int32 MaxValue,
	bool bDeveloperOnly)
{
	FBlueprintHelperSettingRowViewModel Row = UBlueprintHelperSettingsUIUtils::MakeBaseRow(DotPath, Category, Label, Hint, EBlueprintHelperSettingValueType::Integer, bDeveloperOnly);
	Row.MinValue = MinValue;
	Row.MaxValue = MaxValue;
	Row.bHasMinValue = true;
	Row.bHasMaxValue = true;
	return Row;
}

FBlueprintHelperSettingRowViewModel UBlueprintHelperSettingsUIUtils::MakeBooleanRow(
	const FString& DotPath,
	const FText& Category,
	const FText& Label,
	const FText& Hint,
	bool bDeveloperOnly)
{
	return UBlueprintHelperSettingsUIUtils::MakeBaseRow(DotPath, Category, Label, Hint, EBlueprintHelperSettingValueType::Boolean, bDeveloperOnly);
}

FBlueprintHelperSettingRowViewModel UBlueprintHelperSettingsUIUtils::MakeChoiceRow(
	const FString& DotPath,
	const FText& Category,
	const FText& Label,
	const FText& Hint,
	TArray<FBlueprintHelperSettingChoiceViewModel> Choices,
	bool bDeveloperOnly)
{
	FBlueprintHelperSettingRowViewModel Row = UBlueprintHelperSettingsUIUtils::MakeBaseRow(DotPath, Category, Label, Hint, EBlueprintHelperSettingValueType::Choice, bDeveloperOnly);
	Row.Choices = MoveTemp(Choices);
	return Row;
}

FBlueprintHelperSettingRowViewModel UBlueprintHelperSettingsUIUtils::MakeVector2Row(
	const FString& DotPath,
	const FText& Category,
	const FText& Label,
	const FText& Hint,
	double MinValue,
	double MaxValue,
	bool bDeveloperOnly)
{
	FBlueprintHelperSettingRowViewModel Row = UBlueprintHelperSettingsUIUtils::MakeBaseRow(DotPath, Category, Label, Hint, EBlueprintHelperSettingValueType::Vector2, bDeveloperOnly);
	Row.MinValue = MinValue;
	Row.MaxValue = MaxValue;
	Row.bHasMinValue = true;
	Row.bHasMaxValue = true;
	return Row;
}

FBlueprintHelperSettingRowViewModel UBlueprintHelperSettingsUIUtils::MakeMarginRow(
	const FString& DotPath,
	const FText& Category,
	const FText& Label,
	const FText& Hint,
	double MinValue,
	double MaxValue,
	bool bDeveloperOnly)
{
	FBlueprintHelperSettingRowViewModel Row = UBlueprintHelperSettingsUIUtils::MakeBaseRow(DotPath, Category, Label, Hint, EBlueprintHelperSettingValueType::Margin, bDeveloperOnly);
	Row.MinValue = MinValue;
	Row.MaxValue = MaxValue;
	Row.bHasMinValue = true;
	Row.bHasMaxValue = true;
	return Row;
}

FBlueprintHelperSettingRowViewModel UBlueprintHelperSettingsUIUtils::MakeStringRow(
	const FString& DotPath,
	const FText& Category,
	const FText& Label,
	const FText& Hint,
	bool bDeveloperOnly)
{
	return UBlueprintHelperSettingsUIUtils::MakeBaseRow(DotPath, Category, Label, Hint, EBlueprintHelperSettingValueType::String, bDeveloperOnly);
}

FBlueprintHelperSettingRowViewModel UBlueprintHelperSettingsUIUtils::MakeColorArrayRow(
	const FString& DotPath,
	const FText& Category,
	const FText& Label,
	const FText& Hint,
	bool bDeveloperOnly)
{
	FBlueprintHelperSettingRowViewModel Row = UBlueprintHelperSettingsUIUtils::MakeBaseRow(DotPath, Category, Label, Hint, EBlueprintHelperSettingValueType::ColorArray, bDeveloperOnly);
	Row.MinValue = 0.0;
	Row.MaxValue = 1.0;
	Row.bHasMinValue = true;
	Row.bHasMaxValue = true;
	return Row;
}

bool UBlueprintHelperSettingsUIUtils::ParseNumberList(const FString& Input, int32 ExpectedCount, TArray<double>& OutValues)
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

FString UBlueprintHelperSettingsUIUtils::NumberListToJsonArray(const TArray<double>& Values)
{
	TArray<FString> Parts;
	for (double Value : Values)
	{
		Parts.Add(FString::SanitizeFloat(Value));
	}
	return FString::Printf(TEXT("[%s]"), *FString::Join(Parts, TEXT(",")));
}
