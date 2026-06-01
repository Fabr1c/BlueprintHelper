#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Systems/Config/BlueprintHelperProjectConfigPaths.h"
#include "Systems/Config/BlueprintHelperRuntimeSettingResolver.h"
#include "Systems/Debug/BlueprintHelperScreenshotSettings.h"

namespace
{
struct FScopedBlueprintHelperRuntimeSettingFileBackup
{
	explicit FScopedBlueprintHelperRuntimeSettingFileBackup(const FString& InPath)
		: Path(InPath)
	{
		bHadOriginal = FPaths::FileExists(Path) && FFileHelper::LoadFileToString(OriginalText, *Path);
	}

	~FScopedBlueprintHelperRuntimeSettingFileBackup()
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
	FBlueprintHelperRuntimeSettingResolverTest,
	"BlueprintHelper.Settings.RuntimeResolver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperRuntimeSettingResolverTest::RunTest(const FString& Parameters)
{
	FString Error;
	const FScopedBlueprintHelperRuntimeSettingFileBackup ProjectSettingBackup(FBlueprintHelperProjectConfigPaths::GetProjectSettingPath());
	const FScopedBlueprintHelperRuntimeSettingFileBackup UserSettingBackup(FBlueprintHelperProjectConfigPaths::GetUserSettingOverridePath());

	TestTrue(
		TEXT("project setting fixture writes"),
		ProjectSettingBackup.Write(
			TEXT("{")
			TEXT("\"ui\":{\"review_panel\":{")
			TEXT("\"diff_frame_outer_padding\":7.25,")
			TEXT("\"surface_geometry_padding\":[10.0,10.0],")
			TEXT("\"diff_action_spacing\":\"1,2,3,4\",")
			TEXT("\"overlay_filter_current_asset_only\":\"false\"")
			TEXT("}},")
			TEXT("\"runtime\":{\"bridge\":{\"port\":\"6000\"}}")
			TEXT(",\"debug\":{\"screenshot\":{\"output_dir\":\"EvidenceShots\",\"default_capture_target\":\"active_viewport\",\"filename_prefix\":\"shot\",\"graph_max_nodes_per_image\":3}}")
			TEXT("}"),
			Error));
	TestTrue(TEXT("user setting fixture clears overrides"), UserSettingBackup.Write(TEXT("{}"), Error));

	TestEqual(
		TEXT("double reads number"),
		FBlueprintHelperRuntimeSettingResolver::GetDouble(TEXT("ui.review_panel.diff_frame_outer_padding"), 1.0),
		7.25);

	const FVector2D VectorValue = FBlueprintHelperRuntimeSettingResolver::GetVector2D(
		TEXT("ui.review_panel.surface_geometry_padding"),
		FVector2D::ZeroVector);
	TestEqual(TEXT("vector reads array x"), VectorValue.X, 10.0);
	TestEqual(TEXT("vector reads array y"), VectorValue.Y, 10.0);

	TestEqual(
		TEXT("missing int returns default"),
		FBlueprintHelperRuntimeSettingResolver::GetInt(TEXT("missing.path"), 42),
		42);

	TestEqual(
		TEXT("int reads string"),
		FBlueprintHelperRuntimeSettingResolver::GetInt(TEXT("runtime.bridge.port"), 42),
		6000);

	TestFalse(
		TEXT("bool reads string"),
		FBlueprintHelperRuntimeSettingResolver::GetBool(TEXT("ui.review_panel.overlay_filter_current_asset_only"), true));

	const FMargin MarginValue = FBlueprintHelperRuntimeSettingResolver::GetMargin(
		TEXT("ui.review_panel.diff_action_spacing"),
		FMargin());
	TestEqual(TEXT("margin reads string left"), MarginValue.Left, 1.0f);
	TestEqual(TEXT("margin reads string top"), MarginValue.Top, 2.0f);
	TestEqual(TEXT("margin reads string right"), MarginValue.Right, 3.0f);
	TestEqual(TEXT("margin reads string bottom"), MarginValue.Bottom, 4.0f);

	FString Diagnostics;
	TestEqual(
		TEXT("missing double returns default"),
		FBlueprintHelperRuntimeSettingResolver::GetDouble(TEXT("missing.double"), 8.0, &Diagnostics),
		8.0);
	TestFalse(TEXT("missing double produces diagnostics"), Diagnostics.IsEmpty());

	TSharedPtr<FJsonValue> JsonValue = FBlueprintHelperRuntimeSettingResolver::GetJsonValue(
		TEXT("ui.review_panel.surface_geometry_padding[1]"),
		&Diagnostics);
	TestTrue(TEXT("json value reads array index"), JsonValue.IsValid() && JsonValue->Type == EJson::Number);
	TestEqual(TEXT("json array index value"), JsonValue->AsNumber(), 10.0);

	const FBlueprintHelperScreenshotSettings ScreenshotSettings =
		FBlueprintHelperScreenshotSettings::Load();
	TestEqual(TEXT("screenshot output dir comes from settings"), ScreenshotSettings.OutputDir, FString(TEXT("EvidenceShots")));
	TestTrue(
		TEXT("screenshot default target comes from settings"),
		ScreenshotSettings.DefaultCaptureTarget == EBlueprintHelperScreenshotTarget::ActiveViewport);
	TestEqual(TEXT("screenshot filename prefix comes from settings"), ScreenshotSettings.FilenamePrefix, FString(TEXT("shot")));
	TestEqual(TEXT("screenshot graph tile node cap comes from settings"), ScreenshotSettings.GraphMaxNodesPerImage, 3);

	return true;
}
