// BlueprintHelper settings store implementation.

#include "Systems/Config/BlueprintHelperSettingStore.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Systems/Config/BlueprintHelperProjectConfigPaths.h"
#include "Systems/Config/Utils/BlueprintHelperConfigUtils.h"

FBlueprintHelperSettingView FBlueprintHelperSettingStore::Load()
{
	FBlueprintHelperSettingView View;
	View.Schema = UBlueprintHelperConfigUtils::GetSettingSchema();
	View.Version = UBlueprintHelperConfigUtils::GetSettingVersion();
	View.DefaultSettingPath = GetDefaultSettingPath();
	View.ProjectSettingPath = FBlueprintHelperProjectConfigPaths::GetProjectSettingPath();
	View.UserSettingOverridePath = FBlueprintHelperProjectConfigPaths::GetUserSettingOverridePath();
	View.bProjectSettingExists = FPaths::FileExists(View.ProjectSettingPath);
	View.bUserOverrideExists = FPaths::FileExists(View.UserSettingOverridePath);

	FString EffectiveJson;
	if (LoadEffectiveSettingJson(EffectiveJson, View.ErrorText))
	{
		View.EffectiveSourcePath = TEXT("built-in + default + project + user");
		View.EffectiveJson = PrettyPrintJsonOrOriginal(EffectiveJson);
		View.bLoaded = !View.EffectiveJson.IsEmpty();
		View.StatusText = FString::Printf(TEXT("Loaded %s"), *View.EffectiveSourcePath);
	}
	else
	{
		View.EffectiveSourcePath = TEXT("built-in + default + project + user");
		View.StatusText = TEXT("Settings failed to load");
	}

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

bool FBlueprintHelperSettingStore::LoadEffectiveSettingObject(TSharedPtr<FJsonObject>& OutObject, FString& OutError)
{
	OutObject.Reset();
	OutError.Reset();

	TSharedPtr<FJsonObject> EffectiveObject;
	if (!UBlueprintHelperConfigUtils::ParseJsonObject(GetBuiltInDefaultSettingJson(), EffectiveObject, OutError))
	{
		return false;
	}

	if (!UBlueprintHelperConfigUtils::MergeJsonFileIfExists(GetDefaultSettingPath(), EffectiveObject, OutError))
	{
		return false;
	}

	if (!UBlueprintHelperConfigUtils::MergeJsonFileIfExists(FBlueprintHelperProjectConfigPaths::GetProjectSettingPath(), EffectiveObject, OutError))
	{
		return false;
	}

	if (!UBlueprintHelperConfigUtils::MergeJsonFileIfExists(FBlueprintHelperProjectConfigPaths::GetUserSettingOverridePath(), EffectiveObject, OutError))
	{
		return false;
	}

	OutObject = EffectiveObject;
	return OutObject.IsValid();
}

bool FBlueprintHelperSettingStore::LoadEffectiveSettingJson(FString& OutJson, FString& OutError)
{
	OutJson.Reset();

	TSharedPtr<FJsonObject> EffectiveObject;
	if (!LoadEffectiveSettingObject(EffectiveObject, OutError))
	{
		return false;
	}

	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	if (!FJsonSerializer::Serialize(EffectiveObject.ToSharedRef(), Writer))
	{
		OutError = TEXT("setting_json_serialize_failed");
		return false;
	}

	return true;
}

bool FBlueprintHelperSettingStore::TryGetEffectiveJsonValue(const FString& DotPath, TSharedPtr<FJsonValue>& OutValue, FString& OutError)
{
	OutValue.Reset();

	TSharedPtr<FJsonObject> EffectiveObject;
	if (!LoadEffectiveSettingObject(EffectiveObject, OutError))
	{
		return false;
	}

	return UBlueprintHelperConfigUtils::TryGetValueAtPath(EffectiveObject, DotPath, OutValue, OutError);
}

bool FBlueprintHelperSettingStore::TryGetProjectJsonValue(const FString& DotPath, TSharedPtr<FJsonValue>& OutValue, FString& OutError)
{
	OutValue.Reset();
	OutError.Reset();

	FString ProjectJson;
	if (!LoadFileIfExists(FBlueprintHelperProjectConfigPaths::GetProjectSettingPath(), ProjectJson))
	{
		return false;
	}

	TSharedPtr<FJsonObject> ProjectObject;
	if (!UBlueprintHelperConfigUtils::ParseJsonObject(ProjectJson, ProjectObject, OutError))
	{
		return false;
	}

	return UBlueprintHelperConfigUtils::TryGetValueAtPath(ProjectObject, DotPath, OutValue, OutError);
}

bool FBlueprintHelperSettingStore::UpdateProjectSettingValue(const FString& DotPath, const FString& NewValue, FString& OutError)
{
	const FString SettingPath = FBlueprintHelperProjectConfigPaths::GetProjectSettingPath();
	FString InputJson;
	if (!LoadFileIfExists(SettingPath, InputJson))
	{
		InputJson = TEXT("{}\n");
	}

	FString OutputJson;
	if (!UpdateSettingJsonText(InputJson, DotPath, NewValue, OutputJson, OutError))
	{
		return false;
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(SettingPath), true);
	if (!FFileHelper::SaveStringToFile(OutputJson, *SettingPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("setting_write_failed:%s"), *SettingPath);
		return false;
	}
	return true;
}

bool FBlueprintHelperSettingStore::ResetProjectSettingValue(const FString& DotPath, FString& OutError)
{
	const FString SettingPath = FBlueprintHelperProjectConfigPaths::GetProjectSettingPath();
	FString InputJson;
	if (!LoadFileIfExists(SettingPath, InputJson))
	{
		InputJson = TEXT("{}\n");
	}

	FString OutputJson;
	if (!RemoveSettingJsonPath(InputJson, DotPath, OutputJson, OutError))
	{
		return false;
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(SettingPath), true);
	if (!FFileHelper::SaveStringToFile(OutputJson, *SettingPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("setting_write_failed:%s"), *SettingPath);
		return false;
	}
	return true;
}

bool FBlueprintHelperSettingStore::GetSettingValue(const FString& DotPath, FString& OutCurrentValue, FString& OutDefaultValue, bool& bOutHasProjectOverride, FString& OutError)
{
	OutCurrentValue.Reset();
	OutDefaultValue.Reset();
	bOutHasProjectOverride = false;
	OutError.Reset();

	TSharedPtr<FJsonObject> DefaultObject;
	if (!UBlueprintHelperConfigUtils::ParseJsonObject(GetBuiltInDefaultSettingJson(), DefaultObject, OutError))
	{
		return false;
	}
	if (!UBlueprintHelperConfigUtils::MergeJsonFileIfExists(GetDefaultSettingPath(), DefaultObject, OutError))
	{
		return false;
	}

	TSharedPtr<FJsonValue> DefaultValue;
	if (UBlueprintHelperConfigUtils::TryGetValueAtPath(DefaultObject, DotPath, DefaultValue, OutError))
	{
		OutDefaultValue = UBlueprintHelperConfigUtils::JsonValueToSettingString(DefaultValue);
		OutCurrentValue = OutDefaultValue;
	}

	const FString ProjectSettingPath = FBlueprintHelperProjectConfigPaths::GetProjectSettingPath();
	FString ProjectJson;
	if (LoadFileIfExists(ProjectSettingPath, ProjectJson))
	{
		TSharedPtr<FJsonObject> ProjectObject;
		FString ProjectError;
		if (UBlueprintHelperConfigUtils::ParseJsonObject(ProjectJson, ProjectObject, ProjectError))
		{
			TSharedPtr<FJsonValue> ProjectValue;
			if (UBlueprintHelperConfigUtils::TryGetValueAtPath(ProjectObject, DotPath, ProjectValue, ProjectError))
			{
				OutCurrentValue = UBlueprintHelperConfigUtils::JsonValueToSettingString(ProjectValue);
				bOutHasProjectOverride = true;
			}
		}
	}

	const FString UserOverridePath = FBlueprintHelperProjectConfigPaths::GetUserSettingOverridePath();
	FString UserJson;
	if (LoadFileIfExists(UserOverridePath, UserJson))
	{
		TSharedPtr<FJsonObject> UserObject;
		FString UserError;
		if (UBlueprintHelperConfigUtils::ParseJsonObject(UserJson, UserObject, UserError))
		{
			TSharedPtr<FJsonValue> UserValue;
			if (UBlueprintHelperConfigUtils::TryGetValueAtPath(UserObject, DotPath, UserValue, UserError))
			{
				OutCurrentValue = UBlueprintHelperConfigUtils::JsonValueToSettingString(UserValue);
			}
		}
	}

	return !OutCurrentValue.IsEmpty() || !OutDefaultValue.IsEmpty();
}

bool FBlueprintHelperSettingStore::UpdateSettingJsonText(const FString& InputJson, const FString& DotPath, const FString& NewValue, FString& OutJson, FString& OutError)
{
	TSharedPtr<FJsonObject> RootObject;
	if (!UBlueprintHelperConfigUtils::ParseJsonObject(InputJson, RootObject, OutError))
	{
		return false;
	}

	TArray<FString> Parts;
	if (!UBlueprintHelperConfigUtils::SplitDotPath(DotPath, Parts, OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Cursor = RootObject;
	for (int32 Index = 0; Index < Parts.Num() - 1; ++Index)
	{
		const FString& Part = Parts[Index];
		TSharedPtr<FJsonObject> Child;
		if (!UBlueprintHelperConfigUtils::TryGetObjectFieldSafe(Cursor, Part, Child))
		{
			Child = MakeShared<FJsonObject>();
			Cursor->SetObjectField(Part, Child);
		}
		Cursor = Child;
	}

	Cursor->SetField(Parts.Last(), UBlueprintHelperConfigUtils::ConvertSettingStringToJsonValue(NewValue));
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	if (!FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer))
	{
		OutError = TEXT("setting_json_serialize_failed");
		return false;
	}
	return true;
}

bool FBlueprintHelperSettingStore::RemoveSettingJsonPath(const FString& InputJson, const FString& DotPath, FString& OutJson, FString& OutError)
{
	TSharedPtr<FJsonObject> RootObject;
	if (!UBlueprintHelperConfigUtils::ParseJsonObject(InputJson, RootObject, OutError))
	{
		return false;
	}

	TArray<FString> Parts;
	if (!UBlueprintHelperConfigUtils::SplitDotPath(DotPath, Parts, OutError))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Cursor = RootObject;
	for (int32 Index = 0; Index < Parts.Num() - 1; ++Index)
	{
		TSharedPtr<FJsonObject> Child;
		if (!UBlueprintHelperConfigUtils::TryGetObjectFieldSafe(Cursor, Parts[Index], Child))
		{
			OutJson = InputJson;
			return true;
		}
		Cursor = Child;
	}

	Cursor->RemoveField(Parts.Last());
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	if (!FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer))
	{
		OutError = TEXT("setting_json_serialize_failed");
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
		TEXT("  \"profiles\": { \"default\": { \"safety_profile\": \"standard\" } },\n")
		TEXT("  \"safety\": { \"preview_required\": true, \"write_approval_required\": true, \"approval_bypass\": false },\n")
		TEXT("  \"cli\": { \"artifacts\": { \"default_output_dir\": \"Saved/BlueprintHelper/Cli\" } },\n")
		TEXT("  \"ui\": { \"main_window\": { \"default_tab\": \"tools\" } },\n")
		TEXT("  \"runtime\": { \"bridge\": { \"port\": 54321 } },\n")
		TEXT("  \"review\": {\n")
		TEXT("    \"performance\": {\n")
		TEXT("      \"trace_warning_ms\": 16,\n")
		TEXT("      \"main_window_page_construct_warning_ms\": 8,\n")
		TEXT("      \"pending_load_page_size\": 100,\n")
		TEXT("      \"pending_load_scroll_prefetch_rows\": 24,\n")
		TEXT("      \"pending_load_validity_candidate_budget\": 256,\n")
		TEXT("      \"validity_sweep_enabled\": true,\n")
		TEXT("      \"validity_sweep_max_record_hydrations_per_worker_batch\": 8,\n")
		TEXT("      \"validity_sweep_max_game_thread_targets_per_frame\": 2,\n")
		TEXT("      \"validity_sweep_max_game_thread_ms_per_frame\": 1.0,\n")
		TEXT("      \"validity_sweep_max_invalid_purges_per_batch\": 32\n")
		TEXT("    }\n")
		TEXT("  },\n")
		TEXT("  \"tool_clusters\": {\n")
		TEXT("    \"graph_write\": {\n")
		TEXT("      \"action_resolution\": {\n")
		TEXT("        \"max_candidates\": 8,\n")
		TEXT("        \"default_search_mode\": \"\",\n")
		TEXT("        \"default_ambiguity_policy\": \"\"\n")
		TEXT("      }\n")
		TEXT("    }\n")
		TEXT("  },\n")
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
