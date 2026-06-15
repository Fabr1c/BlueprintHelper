#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Systems/Config/BlueprintHelperProjectConfigPaths.h"
#include "Systems/Config/BlueprintHelperSettingStore.h"
#include "Systems/ToolClusters/BlueprintHelperToolClusterConfigResolver.h"
#include "UI/BlueprintHelperUiSettingsResolver.h"
#include "UI/Settings/BlueprintHelperSettingsPresenter.h"
#include "UI/Settings/Utils/BlueprintHelperSettingsUIUtils.h"

namespace
{
struct FScopedBlueprintHelperSettingFileBackup
{
	explicit FScopedBlueprintHelperSettingFileBackup(const FString& InPath)
		: Path(InPath)
	{
		bHadOriginal = FPaths::FileExists(Path) && FFileHelper::LoadFileToString(OriginalText, *Path);
	}

	~FScopedBlueprintHelperSettingFileBackup()
	{
		if (bHadOriginal)
		{
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
			FFileHelper::SaveStringToFile(OriginalText, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		}
		else if (FPaths::FileExists(Path))
		{
			IFileManager::Get().Delete(*Path, false, true);
		}
	}

	bool Write(const FString& JsonText, FString& OutError) const
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		if (!FFileHelper::SaveStringToFile(JsonText, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			OutError = FString::Printf(TEXT("setting_test_write_failed:%s"), *Path);
			return false;
		}
		return true;
	}

	FString Path;
	FString OriginalText;
	bool bHadOriginal = false;
};

bool LoadBlueprintHelperSourceFile(FAutomationTestBase& Test, const FString& RelativePath, FString& OutText)
{
	const FString FullPath = FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("BlueprintHelper/BlueprintHelper/Source/BlueprintHelper"),
		RelativePath);
	if (!FFileHelper::LoadFileToString(OutText, *FullPath))
	{
		Test.AddError(FString::Printf(TEXT("source file not readable: %s"), *FullPath));
		return false;
	}
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSettingStoreUpdateJsonTest,
	"BlueprintHelper.Settings.Store.UpdateJsonPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSettingStoreUpdateJsonTest::RunTest(const FString& Parameters)
{
	FString OutJson;
	FString Error;
	TestTrue(
		TEXT("number path updates"),
		FBlueprintHelperSettingStore::UpdateSettingJsonText(
			TEXT("{}"),
			TEXT("ui.review_panel.diff_action_padding"),
			TEXT("6"),
			OutJson,
			Error));
	TestTrue(TEXT("number value appears"), OutJson.Contains(TEXT("diff_action_padding")) && OutJson.Contains(TEXT("6")));

	FString ArrayJson;
	TestTrue(
		TEXT("array path updates"),
		FBlueprintHelperSettingStore::UpdateSettingJsonText(
			OutJson,
			TEXT("ui.review_panel.diff_action_spacing"),
			TEXT("[0,0,6,0]"),
			ArrayJson,
			Error));
	TestTrue(TEXT("array value appears"), ArrayJson.Contains(TEXT("diff_action_spacing")) && ArrayJson.Contains(TEXT("[")));

	FString BoolJson;
	TestTrue(
		TEXT("bool path updates"),
		FBlueprintHelperSettingStore::UpdateSettingJsonText(
			ArrayJson,
			TEXT("debug.contains_full_settings"),
			TEXT("true"),
			BoolJson,
			Error));
	TestTrue(TEXT("bool value appears"), BoolJson.Contains(TEXT("contains_full_settings")) && BoolJson.Contains(TEXT("true")));

	FString RemovedJson;
	TestTrue(
		TEXT("path removes"),
		FBlueprintHelperSettingStore::RemoveSettingJsonPath(
			BoolJson,
			TEXT("debug.contains_full_settings"),
			RemovedJson,
			Error));
	TestFalse(TEXT("removed value disappears"), RemovedJson.Contains(TEXT("contains_full_settings")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSettingStoreEffectiveMergeTest,
	"BlueprintHelper.Settings.Store.EffectiveMerge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSettingStoreEffectiveMergeTest::RunTest(const FString& Parameters)
{
	FString Error;
	const FScopedBlueprintHelperSettingFileBackup ProjectSettingBackup(FBlueprintHelperProjectConfigPaths::GetProjectSettingPath());
	const FScopedBlueprintHelperSettingFileBackup UserSettingBackup(FBlueprintHelperProjectConfigPaths::GetUserSettingOverridePath());

	TestTrue(
		TEXT("project setting fixture writes"),
		ProjectSettingBackup.Write(
			TEXT("{")
			TEXT("\"ui\":{\"review_panel\":{")
			TEXT("\"debug_max_messages\":123,")
			TEXT("\"surface_geometry_padding\":[20.0,30.0]")
			TEXT("}}")
			TEXT("}"),
			Error));
	TestTrue(
		TEXT("user setting fixture writes"),
		UserSettingBackup.Write(
			TEXT("{")
			TEXT("\"ui\":{\"review_panel\":{\"debug_max_messages\":321}}")
			TEXT("}"),
			Error));

	TSharedPtr<FJsonValue> PriorityValue;
	TestTrue(
		TEXT("user overrides project and default priority"),
		FBlueprintHelperSettingStore::TryGetEffectiveJsonValue(TEXT("ui.review_panel.debug_max_messages"), PriorityValue, Error));
	TestTrue(TEXT("priority value is numeric"), PriorityValue.IsValid() && PriorityValue->Type == EJson::Number);
	TestEqual(TEXT("priority uses user value"), PriorityValue->AsNumber(), 321.0);

	TSharedPtr<FJsonValue> DefaultSiblingValue;
	TestTrue(
		TEXT("project override deep merges without dropping default siblings"),
		FBlueprintHelperSettingStore::TryGetEffectiveJsonValue(TEXT("ui.review_panel.diff_action_padding"), DefaultSiblingValue, Error));
	TestTrue(TEXT("default sibling value is numeric"), DefaultSiblingValue.IsValid() && DefaultSiblingValue->Type == EJson::Number);
	TestEqual(TEXT("default sibling survives partial project override"), DefaultSiblingValue->AsNumber(), 5.0);

	TSharedPtr<FJsonValue> ArrayFirstValue;
	TestTrue(
		TEXT("array index path reads first element"),
		FBlueprintHelperSettingStore::TryGetEffectiveJsonValue(TEXT("ui.review_panel.surface_geometry_padding[0]"), ArrayFirstValue, Error));
	TestTrue(TEXT("array first value is numeric"), ArrayFirstValue.IsValid() && ArrayFirstValue->Type == EJson::Number);
	TestEqual(TEXT("array override replaces default array"), ArrayFirstValue->AsNumber(), 20.0);

	TSharedPtr<FJsonValue> ProjectArraySecondValue;
	TestTrue(
		TEXT("project array index path reads second element"),
		FBlueprintHelperSettingStore::TryGetProjectJsonValue(TEXT("ui.review_panel.surface_geometry_padding[1]"), ProjectArraySecondValue, Error));
	TestTrue(TEXT("project array second value is numeric"), ProjectArraySecondValue.IsValid() && ProjectArraySecondValue->Type == EJson::Number);
	TestEqual(TEXT("project array second value"), ProjectArraySecondValue->AsNumber(), 30.0);

	TSharedPtr<FJsonValue> PreservedDefaultValue;
	TestTrue(
		TEXT("effective object preserves default when project partial override exists"),
		FBlueprintHelperSettingStore::TryGetEffectiveJsonValue(TEXT("ui.review_panel.row_content_padding"), PreservedDefaultValue, Error));
	TestTrue(TEXT("preserved default value is numeric"), PreservedDefaultValue.IsValid() && PreservedDefaultValue->Type == EJson::Number);
	TestEqual(TEXT("preserved default row content padding"), PreservedDefaultValue->AsNumber(), 6.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSettingsGraphWriteLayoutRetiredTest,
	"BlueprintHelper.Settings.GraphWriteLayoutRetired",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSettingsGraphWriteLayoutRetiredTest::RunTest(const FString& Parameters)
{
	FString Error;
	const FScopedBlueprintHelperSettingFileBackup ProjectSettingBackup(FBlueprintHelperProjectConfigPaths::GetProjectSettingPath());
	const FScopedBlueprintHelperSettingFileBackup UserSettingBackup(FBlueprintHelperProjectConfigPaths::GetUserSettingOverridePath());

	TestTrue(TEXT("project setting fixture clears overrides"), ProjectSettingBackup.Write(TEXT("{}"), Error));
	TestTrue(TEXT("user setting fixture clears overrides"), UserSettingBackup.Write(TEXT("{}"), Error));

	TSharedPtr<FJsonValue> RetiredLayoutValue;
	TestFalse(
		TEXT("effective settings no longer expose graph_write.layout"),
		FBlueprintHelperSettingStore::TryGetEffectiveJsonValue(TEXT("tool_clusters.graph_write.layout"), RetiredLayoutValue, Error));

	TSharedPtr<FJsonValue> DryRunValue;
	TestTrue(
		TEXT("effective settings expose graph_write.dry_run"),
		FBlueprintHelperSettingStore::TryGetEffectiveJsonValue(TEXT("tool_clusters.graph_write.dry_run"), DryRunValue, Error));
	TestTrue(TEXT("graph_write.dry_run is boolean"), DryRunValue.IsValid() && DryRunValue->Type == EJson::Boolean);
	TestFalse(TEXT("graph_write.dry_run defaults false"), DryRunValue.IsValid() && DryRunValue->AsBool());

	TSharedPtr<FJsonValue> CliArtifactDirValue;
	TestTrue(
		TEXT("effective settings expose cli artifact output dir"),
		FBlueprintHelperSettingStore::TryGetEffectiveJsonValue(TEXT("cli.artifacts.default_output_dir"), CliArtifactDirValue, Error));
	TestTrue(TEXT("cli artifact dir is a string"), CliArtifactDirValue.IsValid() && CliArtifactDirValue->Type == EJson::String);
	TestEqual(
		TEXT("cli artifact dir default"),
		CliArtifactDirValue.IsValid() ? CliArtifactDirValue->AsString() : FString(),
		FString(TEXT("Saved/BlueprintHelper/Cli")));

	TSharedPtr<FJsonValue> ScreenshotOutputDirValue;
	TestTrue(
		TEXT("effective settings expose screenshot output dir"),
		FBlueprintHelperSettingStore::TryGetEffectiveJsonValue(TEXT("debug.screenshot.output_dir"), ScreenshotOutputDirValue, Error));
	TestTrue(TEXT("screenshot output dir is a string"), ScreenshotOutputDirValue.IsValid() && ScreenshotOutputDirValue->Type == EJson::String);
	TestEqual(
		TEXT("screenshot output dir default"),
		ScreenshotOutputDirValue.IsValid() ? ScreenshotOutputDirValue->AsString() : FString(),
		FString(TEXT("Screenshots")));

	TSharedPtr<FJsonValue> ScreenshotTargetValue;
	TestTrue(
		TEXT("effective settings expose screenshot default target"),
		FBlueprintHelperSettingStore::TryGetEffectiveJsonValue(TEXT("debug.screenshot.default_capture_target"), ScreenshotTargetValue, Error));
	TestTrue(TEXT("screenshot target is a string"), ScreenshotTargetValue.IsValid() && ScreenshotTargetValue->Type == EJson::String);
	TestEqual(
		TEXT("screenshot default target"),
		ScreenshotTargetValue.IsValid() ? ScreenshotTargetValue->AsString() : FString(),
		FString(TEXT("active_window")));

	TSharedPtr<FJsonValue> ScreenshotGraphMaxNodesValue;
	TestTrue(
		TEXT("effective settings expose screenshot graph max nodes per image"),
		FBlueprintHelperSettingStore::TryGetEffectiveJsonValue(TEXT("debug.screenshot.graph_max_nodes_per_image"), ScreenshotGraphMaxNodesValue, Error));
	TestTrue(TEXT("screenshot graph max nodes is numeric"), ScreenshotGraphMaxNodesValue.IsValid() && ScreenshotGraphMaxNodesValue->Type == EJson::Number);
	TestEqual(
		TEXT("screenshot graph max nodes default"),
		ScreenshotGraphMaxNodesValue.IsValid() ? ScreenshotGraphMaxNodesValue->AsNumber() : 0.0,
		8.0);

	TSharedPtr<FJsonValue> ActionResolutionMaxCandidates;
	TestTrue(
		TEXT("effective settings expose graph_write action resolution max candidates"),
		FBlueprintHelperSettingStore::TryGetEffectiveJsonValue(TEXT("tool_clusters.graph_write.action_resolution.max_candidates"), ActionResolutionMaxCandidates, Error));
	TestTrue(TEXT("action resolution max candidates is numeric"), ActionResolutionMaxCandidates.IsValid() && ActionResolutionMaxCandidates->Type == EJson::Number);
	TestEqual(
		TEXT("action resolution max candidates default"),
		ActionResolutionMaxCandidates.IsValid() ? ActionResolutionMaxCandidates->AsNumber() : 0.0,
		8.0);

	TSharedPtr<FJsonValue> AutoSearchMaxCandidates;
	TestTrue(
		TEXT("effective settings expose graph_write autosearch candidate budget"),
		FBlueprintHelperSettingStore::TryGetEffectiveJsonValue(
			TEXT("tool_clusters.graph_write.action_resolution.auto_search.max_candidates_per_statement"),
			AutoSearchMaxCandidates,
			Error));
	TestTrue(TEXT("autosearch candidate budget is numeric"), AutoSearchMaxCandidates.IsValid() && AutoSearchMaxCandidates->Type == EJson::Number);
	TestEqual(
		TEXT("autosearch candidate budget default"),
		AutoSearchMaxCandidates.IsValid() ? AutoSearchMaxCandidates->AsNumber() : 0.0,
		3.0);

	TSharedPtr<FJsonValue> AutoSearchMaxStatements;
	TestTrue(
		TEXT("effective settings expose graph_write autosearch statement budget"),
		FBlueprintHelperSettingStore::TryGetEffectiveJsonValue(
			TEXT("tool_clusters.graph_write.action_resolution.auto_search.max_statements"),
			AutoSearchMaxStatements,
			Error));
	TestTrue(TEXT("autosearch statement budget is numeric"), AutoSearchMaxStatements.IsValid() && AutoSearchMaxStatements->Type == EJson::Number);
	TestEqual(
		TEXT("autosearch statement budget default"),
		AutoSearchMaxStatements.IsValid() ? AutoSearchMaxStatements->AsNumber() : 0.0,
		16.0);

	TSharedPtr<FJsonValue> AutoSearchMaxTotalMs;
	TestTrue(
		TEXT("effective settings expose graph_write autosearch time budget"),
		FBlueprintHelperSettingStore::TryGetEffectiveJsonValue(
			TEXT("tool_clusters.graph_write.action_resolution.auto_search.max_total_ms"),
			AutoSearchMaxTotalMs,
			Error));
	TestTrue(TEXT("autosearch time budget is numeric"), AutoSearchMaxTotalMs.IsValid() && AutoSearchMaxTotalMs->Type == EJson::Number);
	TestEqual(
		TEXT("autosearch time budget default"),
		AutoSearchMaxTotalMs.IsValid() ? AutoSearchMaxTotalMs->AsNumber() : 0.0,
		120.0);

	TSharedPtr<FJsonValue> AgentTaskWorkerMaxAttempts;
	TestTrue(
		TEXT("effective settings expose agent task-worker max attempts"),
		FBlueprintHelperSettingStore::TryGetEffectiveJsonValue(
			TEXT("agent.task_worker.max_attempts"),
			AgentTaskWorkerMaxAttempts,
			Error));
	TestTrue(TEXT("agent task-worker max attempts is numeric"),
		AgentTaskWorkerMaxAttempts.IsValid() && AgentTaskWorkerMaxAttempts->Type == EJson::Number);
	TestEqual(
		TEXT("agent task-worker max attempts default"),
		AgentTaskWorkerMaxAttempts.IsValid() ? AgentTaskWorkerMaxAttempts->AsNumber() : 0.0,
		3.0);

	const FBlueprintHelperGraphWriteToolClusterPolicy Policy =
		FBlueprintHelperToolClusterConfigResolver::LoadGraphWritePolicy();
	TestFalse(TEXT("GraphWrite policy dry_run default remains false"), Policy.bDryRun);

	FString BridgeRouterSource;
	TestTrue(
		TEXT("bridge router source is readable"),
		LoadBlueprintHelperSourceFile(
			*this,
			TEXT("Private/Entry/Bridge/BlueprintHelperBridgeRouter.cpp"),
			BridgeRouterSource));
	TestFalse(
		TEXT("bridge router no longer injects options.layout"),
		BridgeRouterSource.Contains(TEXT("SetStringDefaultIfMissing(Options, TEXT(\"layout\")")));
	TestFalse(
		TEXT("bridge router no longer reads Policy.Layout"),
		BridgeRouterSource.Contains(TEXT("Policy.Layout")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSettingsReviewPerformancePendingPagingTest,
	"BlueprintHelper.Settings.ReviewPerformancePendingPaging",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSettingsReviewPerformancePendingPagingTest::RunTest(const FString& Parameters)
{
	FString Error;
	const FScopedBlueprintHelperSettingFileBackup ProjectSettingBackup(
		FBlueprintHelperProjectConfigPaths::GetProjectSettingPath());
	const FScopedBlueprintHelperSettingFileBackup UserSettingBackup(
		FBlueprintHelperProjectConfigPaths::GetUserSettingOverridePath());

	TestTrue(TEXT("project setting fixture writes paging settings"),
		ProjectSettingBackup.Write(
			TEXT("{")
			TEXT("\"review\":{\"performance\":{")
			TEXT("\"pending_load_page_size\":37,")
			TEXT("\"pending_load_scroll_prefetch_rows\":9")
			TEXT("}}")
			TEXT("}"),
			Error));
	TestTrue(TEXT("user setting fixture clears overrides"), UserSettingBackup.Write(TEXT("{}"), Error));

	const FBlueprintHelperReviewPerformanceSettings Settings =
		FBlueprintHelperUiSettingsResolver::LoadReviewPerformanceSettings();
	TestEqual(TEXT("pending page size comes from settings"), Settings.PendingLoadPageSize, 37);
	TestEqual(TEXT("pending scroll prefetch rows comes from settings"),
		Settings.PendingLoadScrollPrefetchRows,
		9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSettingsReviewPerformanceDefaultsVisibleTest,
	"BlueprintHelper.Settings.ReviewPerformanceDefaultsVisible",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSettingsReviewPerformanceDefaultsVisibleTest::RunTest(const FString& Parameters)
{
	FString Error;
	const FScopedBlueprintHelperSettingFileBackup ProjectSettingBackup(
		FBlueprintHelperProjectConfigPaths::GetProjectSettingPath());
	const FScopedBlueprintHelperSettingFileBackup UserSettingBackup(
		FBlueprintHelperProjectConfigPaths::GetUserSettingOverridePath());

	TestTrue(TEXT("project setting fixture clears overrides"), ProjectSettingBackup.Write(TEXT("{}"), Error));
	TestTrue(TEXT("user setting fixture clears overrides"), UserSettingBackup.Write(TEXT("{}"), Error));

	const TArray<FString> ExpectedPerformanceDefaults = {
		TEXT("review.performance.trace_warning_ms"),
		TEXT("review.performance.main_window_page_construct_warning_ms"),
		TEXT("review.performance.pending_load_page_size"),
		TEXT("review.performance.pending_load_scroll_prefetch_rows"),
		TEXT("review.performance.pending_load_validity_candidate_budget"),
		TEXT("review.performance.validity_sweep_enabled"),
		TEXT("review.performance.validity_sweep_max_record_hydrations_per_worker_batch"),
		TEXT("review.performance.validity_sweep_max_game_thread_targets_per_frame"),
		TEXT("review.performance.validity_sweep_max_game_thread_ms_per_frame"),
		TEXT("review.performance.validity_sweep_max_invalid_purges_per_batch")
	};

	for (const FString& DotPath : ExpectedPerformanceDefaults)
	{
		FString DefaultValue;
		bool bHasProjectOverride = false;
		const FString CurrentValue =
			UBlueprintHelperSettingsUIUtils::ReadSettingValueOrDefault(DotPath, DefaultValue, bHasProjectOverride);
		TestFalse(
			FString::Printf(TEXT("%s has visible current value"), *DotPath),
			CurrentValue.IsEmpty());
		TestFalse(
			FString::Printf(TEXT("%s has visible default value"), *DotPath),
			DefaultValue.IsEmpty());
		TestFalse(
			FString::Printf(TEXT("%s is not project-overridden"), *DotPath),
			bHasProjectOverride);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSettingsDeveloperWidgetCopySourceHygieneTest,
	"BlueprintHelper.Settings.DeveloperWidgetCopySourceHygiene",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSettingsDeveloperWidgetCopySourceHygieneTest::RunTest(const FString& Parameters)
{
	FString SettingRowSource;
	if (!LoadBlueprintHelperSourceFile(
		*this,
		TEXT("Private/UI/Settings/SBlueprintHelperSettingRow.cpp"),
		SettingRowSource))
	{
		return false;
	}

	TestFalse(
		TEXT("color array input hint is not the legacy English copy"),
		SettingRowSource.Contains(TEXT("Format: [R,G,B,A]")));
	TestTrue(
		TEXT("color array input hint keeps the RGBA format visible"),
		SettingRowSource.Contains(TEXT("[R,G,B,A]")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSettingsPresenterDeveloperRowsTest,
	"BlueprintHelper.Settings.Presenter.DeveloperRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSettingsPresenterDeveloperRowsTest::RunTest(const FString& Parameters)
{
	FString Error;
	const FScopedBlueprintHelperSettingFileBackup ProjectSettingBackup(FBlueprintHelperProjectConfigPaths::GetProjectSettingPath());
	const FScopedBlueprintHelperSettingFileBackup UserSettingBackup(FBlueprintHelperProjectConfigPaths::GetUserSettingOverridePath());

	TestTrue(
		TEXT("project setting enables developer rows"),
		ProjectSettingBackup.Write(
			TEXT("{")
			TEXT("\"profiles\":{\"default\":{\"safety_profile\":\"AutoRepair\"}}")
			TEXT("}"),
			Error));
	TestTrue(TEXT("user setting fixture clears overrides"), UserSettingBackup.Write(TEXT("{}"), Error));

	FBlueprintHelperSettingsPresenter Presenter;
	Presenter.Reload();
	const TArray<FBlueprintHelperSettingRowViewModel>& Rows = Presenter.GetRows();

	const TArray<FString> ExpectedDryRunPaths = {
		TEXT("tool_clusters.asset_factory.dry_run"),
		TEXT("tool_clusters.component.dry_run"),
		TEXT("tool_clusters.class_settings.dry_run"),
		TEXT("tool_clusters.blueprint_variables.dry_run"),
		TEXT("tool_clusters.object_property.dry_run"),
		TEXT("tool_clusters.data_table.dry_run"),
		TEXT("tool_clusters.umg_widget.dry_run"),
		TEXT("tool_clusters.graph_write.dry_run")
	};
	const TArray<FString> ExpectedRuntimeConsumedRows = {
		TEXT("cli.artifacts.default_output_dir"),
		TEXT("runtime.bridge.port"),
		TEXT("runtime.task_runtime.cache.partial_preview.ttl_seconds"),
		TEXT("runtime.task_runtime.execution_policy.dry_run_mode"),
		TEXT("review.artifact.snapshot_root"),
		TEXT("review.debug_bundle.root_dir"),
		TEXT("review.debug_bundle.enforce_root_path"),
		TEXT("review.performance.trace_warning_ms"),
		TEXT("review.performance.main_window_page_construct_warning_ms"),
		TEXT("review.performance.pending_load_page_size"),
		TEXT("review.performance.pending_load_scroll_prefetch_rows"),
		TEXT("review.performance.pending_load_validity_candidate_budget"),
		TEXT("review.performance.validity_sweep_enabled"),
		TEXT("review.performance.validity_sweep_max_record_hydrations_per_worker_batch"),
		TEXT("review.performance.validity_sweep_max_game_thread_targets_per_frame"),
		TEXT("review.performance.validity_sweep_max_game_thread_ms_per_frame"),
		TEXT("review.performance.validity_sweep_max_invalid_purges_per_batch"),
		TEXT("tool_clusters.graph_write.action_resolution.max_candidates"),
		TEXT("tool_clusters.graph_write.action_resolution.auto_search.max_candidates_per_statement"),
		TEXT("tool_clusters.graph_write.action_resolution.auto_search.max_statements"),
		TEXT("tool_clusters.graph_write.action_resolution.auto_search.max_total_ms"),
		TEXT("tool_clusters.graph_write.action_resolution.auto_search.detail_level"),
		TEXT("debug.screenshot.output_dir"),
		TEXT("debug.screenshot.default_capture_target"),
		TEXT("debug.screenshot.filename_prefix"),
		TEXT("debug.screenshot.graph_max_nodes_per_image"),
		TEXT("agent.task_worker.max_attempts"),
		TEXT("ui.layout_rule_editor.canvas_desired_size"),
		TEXT("ui.review_panel.overlay_filter_current_asset_only")
	};
	const TArray<FString> HiddenContractMetadataRows = {
		TEXT("review.version"),
		TEXT("review.debug_bundle.schema_review_panel"),
		TEXT("review.debug_bundle.schema_snapshot"),
		TEXT("review.debug_bundle.hash_source")
	};

	TArray<FString> DryRunPaths;
	bool bExitedDryRunCategory = false;
	bool bSawGraphWriteLayout = false;
	bool bSawContractMetadataRow = false;
	bool bSawLegacyDeveloperCopy = false;
	TOptional<FString> LayoutRuleEditorCategory;
	TOptional<FString> TaskSpecWorkbenchCategory;
	for (const FBlueprintHelperSettingRowViewModel& Row : Rows)
	{
		if (Row.DotPath == TEXT("tool_clusters.graph_write.layout"))
		{
			bSawGraphWriteLayout = true;
		}
		if (HiddenContractMetadataRows.Contains(Row.DotPath))
		{
			bSawContractMetadataRow = true;
		}
		if (Row.DotPath.StartsWith(TEXT("ui.layout_rule_editor.")) && !LayoutRuleEditorCategory.IsSet())
		{
			LayoutRuleEditorCategory = Row.CategoryLabel.ToString();
		}
		if (Row.DotPath.StartsWith(TEXT("ui.task_spec_workbench.")) && !TaskSpecWorkbenchCategory.IsSet())
		{
			TaskSpecWorkbenchCategory = Row.CategoryLabel.ToString();
		}

		if (Row.CategoryLabel.ToString() == TEXT("DryRun"))
		{
			TestFalse(TEXT("DryRun rows remain contiguous"), bExitedDryRunCategory);
			DryRunPaths.Add(Row.DotPath);
		}
		else if (DryRunPaths.Num() > 0)
		{
			bExitedDryRunCategory = true;
		}

		if (Row.bDeveloperOnly)
		{
			const FString CategoryText = Row.CategoryLabel.ToString();
			const FString HintText = Row.OverlapHint.ToString();
			bSawLegacyDeveloperCopy = bSawLegacyDeveloperCopy
				|| CategoryText.Contains(TEXT("Developer "))
				|| HintText.Contains(TEXT("Developer-only"))
				|| HintText.Contains(TEXT("default for"))
				|| HintText.Contains(TEXT("Format:"))
				|| Row.AccessStatusText.Contains(TEXT("Developer only"))
				|| Row.ConsumerStatusText.Contains(TEXT("Runtime consumed"));
		}
	}

	TestFalse(TEXT("GraphWrite layout setting row is removed"), bSawGraphWriteLayout);
	TestFalse(TEXT("contract metadata rows remain hidden"), bSawContractMetadataRow);
	TestFalse(TEXT("developer rows do not use legacy English explanatory copy"), bSawLegacyDeveloperCopy);
	TestTrue(TEXT("layout rule editor category exists"), LayoutRuleEditorCategory.IsSet());
	TestTrue(TEXT("task spec workbench category exists"), TaskSpecWorkbenchCategory.IsSet());
	if (LayoutRuleEditorCategory.IsSet() && TaskSpecWorkbenchCategory.IsSet())
	{
		TestNotEqual(
			TEXT("layout rule editor and task spec workbench settings are split by category"),
			LayoutRuleEditorCategory.GetValue(),
			TaskSpecWorkbenchCategory.GetValue());
		TestTrue(
			TEXT("layout rule editor category is system-specific"),
			LayoutRuleEditorCategory.GetValue().Contains(TEXT("布局规则编辑器")));
		TestTrue(
			TEXT("task spec workbench category is system-specific"),
			TaskSpecWorkbenchCategory.GetValue().Contains(TEXT("TaskSpec")));
	}
	TestEqual(TEXT("DryRun row count"), DryRunPaths.Num(), ExpectedDryRunPaths.Num());
	for (int32 Index = 0; Index < FMath::Min(DryRunPaths.Num(), ExpectedDryRunPaths.Num()); ++Index)
	{
		const FString DryRunOrderTestName = FString::Printf(TEXT("DryRun row order %d"), Index);
		TestEqual(*DryRunOrderTestName, DryRunPaths[Index], ExpectedDryRunPaths[Index]);
	}
	for (const FString& ExpectedPath : ExpectedRuntimeConsumedRows)
	{
		const FBlueprintHelperSettingRowViewModel* Row = Rows.FindByPredicate([&ExpectedPath](const FBlueprintHelperSettingRowViewModel& Candidate)
		{
			return Candidate.DotPath == ExpectedPath;
		});
		const FString RowPresentTestName = FString::Printf(TEXT("setting row exists: %s"), *ExpectedPath);
		TestTrue(*RowPresentTestName, Row != nullptr);
		if (Row)
		{
			const FString RuntimeConsumedTestName = FString::Printf(TEXT("setting row is marked consumed: %s"), *ExpectedPath);
			TestTrue(*RuntimeConsumedTestName, Row->bRuntimeConsumed);
		}
	}

	const FBlueprintHelperSettingRowViewModel* TaskWorkerMaxAttemptsRow = Rows.FindByPredicate([](const FBlueprintHelperSettingRowViewModel& Candidate)
	{
		return Candidate.DotPath == TEXT("agent.task_worker.max_attempts");
	});
	TestTrue(TEXT("task-worker max attempts row exists"), TaskWorkerMaxAttemptsRow != nullptr);
	if (TaskWorkerMaxAttemptsRow)
	{
		TestTrue(TEXT("task-worker max attempts row is user editable"), !TaskWorkerMaxAttemptsRow->bDeveloperOnly);
		TestEqual(TEXT("task-worker max attempts row type"), TaskWorkerMaxAttemptsRow->ValueType, EBlueprintHelperSettingValueType::Integer);
		TestTrue(TEXT("task-worker max attempts row has min"), TaskWorkerMaxAttemptsRow->bHasMinValue);
		TestTrue(TEXT("task-worker max attempts row has max"), TaskWorkerMaxAttemptsRow->bHasMaxValue);
		TestEqual(TEXT("task-worker max attempts row min"), TaskWorkerMaxAttemptsRow->MinValue, 1.0);
		TestEqual(TEXT("task-worker max attempts row max"), TaskWorkerMaxAttemptsRow->MaxValue, 10.0);
	}

	return true;
}
