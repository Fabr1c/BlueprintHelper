#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/Debug/BlueprintHelperScreenshotCaptureService.h"

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Shared/Debug/BlueprintHelperScreenshotTypes.h"
#include "Systems/Config/BlueprintHelperProjectConfigPaths.h"
#include "Systems/Debug/BlueprintHelperDebugCaseStoreService.h"
#include "Systems/Debug/BlueprintHelperScreenshotSettings.h"

class FBlueprintHelperScreenshotSettingFileBackup
{
public:
	explicit FBlueprintHelperScreenshotSettingFileBackup(const FString& InPath)
		: Path(InPath)
	{
		bHadOriginal = FPaths::FileExists(Path) && FFileHelper::LoadFileToString(OriginalText, *Path);
	}

	~FBlueprintHelperScreenshotSettingFileBackup()
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

private:
	FString Path;
	FString OriginalText;
	bool bHadOriginal = false;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperScreenshotCaptureServiceSettingsTest,
	"BlueprintHelper.RuntimeDiagnostics.Screenshot.SettingsDriveOutputPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperScreenshotCaptureServiceSettingsTest::RunTest(const FString& Parameters)
{
	FString Error;
	const FBlueprintHelperScreenshotSettingFileBackup ProjectSettingBackup(
		FBlueprintHelperProjectConfigPaths::GetProjectSettingPath());
	const FBlueprintHelperScreenshotSettingFileBackup UserSettingBackup(
		FBlueprintHelperProjectConfigPaths::GetUserSettingOverridePath());

	TestTrue(
		TEXT("project setting writes screenshot output dir"),
		ProjectSettingBackup.Write(
			TEXT("{")
			TEXT("\"debug\":{\"screenshot\":{\"output_dir\":\"EvidenceShots\",\"graph_max_nodes_per_image\":3}}")
			TEXT("}"),
			Error));
	TestTrue(TEXT("user setting clears overrides"), UserSettingBackup.Write(TEXT("{}"), Error));

	const FString OutputDir = FBlueprintHelperScreenshotCaptureService::BuildOutputDirectory();
	TestTrue(TEXT("output dir uses configured screenshot subdir"), OutputDir.EndsWith(TEXT("EvidenceShots")));
	TestTrue(
		TEXT("output dir remains under debug root"),
		OutputDir.StartsWith(FBlueprintHelperDebugCaseStoreService::GetDebugRootDir()));
	const FBlueprintHelperScreenshotSettings Settings = FBlueprintHelperScreenshotSettings::Load();
	TestEqual(TEXT("graph max nodes setting loads"), Settings.GraphMaxNodesPerImage, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperScreenshotCaptureServiceMetadataTest,
	"BlueprintHelper.RuntimeDiagnostics.Screenshot.MetadataShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperScreenshotCaptureServiceMetadataTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("unsafe label is sanitized"),
		FBlueprintHelperScreenshotCaptureService::SanitizeFileLabel(TEXT("../bad label"), TEXT("editor")),
		FString(TEXT("badlabel")));

	FBlueprintHelperScreenshotCaptureResult Result;
	Result.bSuccess = true;
	Result.AbsolutePath = TEXT("C:/Project/Saved/BlueprintHelper/Debug/Screenshots/editor.png");
	Result.RelativePath = TEXT("Saved/BlueprintHelper/Debug/Screenshots/editor.png");
	Result.Width = 64;
	Result.Height = 32;
	Result.Target = TEXT("active_window");
	Result.CreatedAtUtc = TEXT("2026-06-02T00:00:00Z");

	TSharedRef<FJsonObject> Json = Result.ToJson();
	TestEqual(TEXT("schema is stable"), Json->GetStringField(TEXT("schema")),
		FString(TEXT("BlueprintHelper.EditorScreenshotResult.v1")));
	TestTrue(TEXT("ok serializes"), Json->GetBoolField(TEXT("ok")));
	TestEqual(TEXT("width serializes"), static_cast<int32>(Json->GetNumberField(TEXT("width"))), 64);
	TestEqual(TEXT("height serializes"), static_cast<int32>(Json->GetNumberField(TEXT("height"))), 32);
	TestEqual(TEXT("path alias serializes"), Json->GetStringField(TEXT("screenshot_path")), Result.AbsolutePath);

	FBlueprintHelperGraphScreenshotCaptureResult GraphResult;
	GraphResult.bSuccess = true;
	GraphResult.GraphName = TEXT("EventGraph");
	GraphResult.SelectedNodeCount = 5;
	GraphResult.ScreenshotCount = 2;
	Result.Target = TEXT("graph_panel");
	Result.TileIndex = 0;
	Result.TileCount = 2;
	Result.NodeCount = 3;
	Result.ViewLabel = TEXT("EventGraph tile 1/2");
	GraphResult.Screenshots.Add(Result);
	TSharedRef<FJsonObject> GraphJson = GraphResult.ToJson();
	TestEqual(TEXT("graph schema is stable"), GraphJson->GetStringField(TEXT("schema")),
		FString(TEXT("BlueprintHelper.GraphScreenshotResult.v1")));
	TestEqual(TEXT("graph screenshot count serializes"), static_cast<int32>(GraphJson->GetNumberField(TEXT("screenshot_count"))), 2);
	const TArray<TSharedPtr<FJsonValue>>* Screenshots = nullptr;
	TestTrue(TEXT("graph screenshots array serializes"), GraphJson->TryGetArrayField(TEXT("screenshots"), Screenshots) && Screenshots && Screenshots->Num() == 1);
	return true;
}

#endif
