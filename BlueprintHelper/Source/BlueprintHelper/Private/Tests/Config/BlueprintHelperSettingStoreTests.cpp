#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Systems/Config/BlueprintHelperProjectConfigPaths.h"
#include "Systems/Config/BlueprintHelperSettingStore.h"
#include "Systems/ToolClusters/BlueprintHelperToolClusterConfigResolver.h"
#include "UI/Settings/BlueprintHelperSettingsPresenter.h"

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

	TArray<FString> DryRunPaths;
	bool bExitedDryRunCategory = false;
	bool bSawGraphWriteLayout = false;
	bool bSawLegacyDeveloperCopy = false;
	for (const FBlueprintHelperSettingRowViewModel& Row : Rows)
	{
		if (Row.DotPath == TEXT("tool_clusters.graph_write.layout"))
		{
			bSawGraphWriteLayout = true;
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
	TestFalse(TEXT("developer rows do not use legacy English explanatory copy"), bSawLegacyDeveloperCopy);
	TestEqual(TEXT("DryRun row count"), DryRunPaths.Num(), ExpectedDryRunPaths.Num());
	for (int32 Index = 0; Index < FMath::Min(DryRunPaths.Num(), ExpectedDryRunPaths.Num()); ++Index)
	{
		const FString DryRunOrderTestName = FString::Printf(TEXT("DryRun row order %d"), Index);
		TestEqual(*DryRunOrderTestName, DryRunPaths[Index], ExpectedDryRunPaths[Index]);
	}

	return true;
}
