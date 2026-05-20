// BlueprintHelper settings store implementation.

#include "Systems/Config/BlueprintHelperSettingStore.h"

#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Systems/Config/BlueprintHelperProjectConfigPaths.h"

namespace
{
constexpr const TCHAR* BlueprintHelperSettingSchema = TEXT("BlueprintHelper.Setting.v1");
constexpr const TCHAR* BlueprintHelperSettingVersion = TEXT("0.5.0");
}

FBlueprintHelperSettingView FBlueprintHelperSettingStore::Load()
{
	FBlueprintHelperSettingView View;
	View.Schema = BlueprintHelperSettingSchema;
	View.Version = BlueprintHelperSettingVersion;
	View.DefaultSettingPath = GetDefaultSettingPath();
	View.ProjectSettingPath = FBlueprintHelperProjectConfigPaths::GetProjectSettingPath();
	View.UserSettingOverridePath = FBlueprintHelperProjectConfigPaths::GetUserSettingOverridePath();
	View.bProjectSettingExists = FPaths::FileExists(View.ProjectSettingPath);
	View.bUserOverrideExists = FPaths::FileExists(View.UserSettingOverridePath);

	FString JsonText;
	if (View.bUserOverrideExists && LoadFileIfExists(View.UserSettingOverridePath, JsonText))
	{
		View.EffectiveSourcePath = View.UserSettingOverridePath;
	}
	else if (View.bProjectSettingExists && LoadFileIfExists(View.ProjectSettingPath, JsonText))
	{
		View.EffectiveSourcePath = View.ProjectSettingPath;
	}
	else if (LoadFileIfExists(View.DefaultSettingPath, JsonText))
	{
		View.EffectiveSourcePath = View.DefaultSettingPath;
	}
	else
	{
		JsonText = GetBuiltInDefaultSettingJson();
		View.EffectiveSourcePath = TEXT("built-in");
	}

	View.EffectiveJson = PrettyPrintJsonOrOriginal(JsonText);
	View.bLoaded = !View.EffectiveJson.IsEmpty();
	View.StatusText = FString::Printf(
		TEXT("Loaded %s"),
		View.EffectiveSourcePath.IsEmpty() ? TEXT("settings") : *View.EffectiveSourcePath);
	return View;
}

bool FBlueprintHelperSettingStore::EnsureProjectSetting(FString& OutPath, FString& OutError)
{
	OutPath = FBlueprintHelperProjectConfigPaths::GetProjectSettingPath();
	OutError.Reset();
	if (FPaths::FileExists(OutPath))
	{
		return true;
	}

	FString JsonText;
	if (!LoadFileIfExists(GetDefaultSettingPath(), JsonText))
	{
		JsonText = GetBuiltInDefaultSettingJson();
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutPath), true);
	if (!FFileHelper::SaveStringToFile(JsonText, *OutPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("Failed to write %s"), *OutPath);
		return false;
	}
	return true;
}

FString FBlueprintHelperSettingStore::GetDefaultSettingPath()
{
	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BlueprintHelper")))
	{
		return FPaths::Combine(Plugin->GetBaseDir(), TEXT("Config"), TEXT("DefaultSetting.json"));
	}
	return FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("BlueprintHelper"),
		TEXT("BlueprintHelper"),
		TEXT("Config"),
		TEXT("DefaultSetting.json"));
}

FString FBlueprintHelperSettingStore::GetBuiltInDefaultSettingJson()
{
	return TEXT("{\n")
		TEXT("  \"schema\": \"BlueprintHelper.Setting.v1\",\n")
		TEXT("  \"version\": \"0.5.0\",\n")
		TEXT("  \"active_profile\": \"default\",\n")
		TEXT("  \"ui\": { \"main_window\": { \"default_tab\": \"tools\" } },\n")
		TEXT("  \"runtime\": { \"bridge\": { \"port\": 54321 } },\n")
		TEXT("  \"tool_clusters\": {},\n")
		TEXT("  \"debug\": {}\n")
		TEXT("}\n");
}

bool FBlueprintHelperSettingStore::LoadFileIfExists(const FString& Path, FString& OutText)
{
	OutText.Reset();
	return FPaths::FileExists(Path) && FFileHelper::LoadFileToString(OutText, *Path);
}

FString FBlueprintHelperSettingStore::PrettyPrintJsonOrOriginal(const FString& JsonText)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return JsonText;
	}

	FString PrettyText;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PrettyText);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return PrettyText;
}
