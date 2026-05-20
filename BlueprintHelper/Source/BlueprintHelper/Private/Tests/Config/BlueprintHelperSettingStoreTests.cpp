#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Systems/Config/BlueprintHelperProjectConfigPaths.h"
#include "Systems/Config/BlueprintHelperSettingStore.h"

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
