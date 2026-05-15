// BlueprintHelper shared safety profile resolver

#include "Systems/Config/BlueprintHelperSafetyProfileResolver.h"

#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
bool TryReadNestedString(
	const TSharedPtr<FJsonObject>& Root,
	const FString& ObjectField,
	const FString& StringField,
	FString& OutValue)
{
	if (!Root.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* Nested = nullptr;
	if (!Root->TryGetObjectField(ObjectField, Nested) || !Nested || !Nested->IsValid())
	{
		return false;
	}

	return (*Nested)->TryGetStringField(StringField, OutValue) && !OutValue.IsEmpty();
}

bool TryReadProjectAgentProfileSafetyProfile(FString& OutProfile)
{
	const FString ProfilePath = FPaths::ProjectDir() / TEXT(".blueprinthelper/agent-profile.json");
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *ProfilePath))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	return TryReadNestedString(Root, TEXT("active_profile"), TEXT("safety_profile"), OutProfile)
		|| TryReadNestedString(Root, TEXT("safety"), TEXT("safety_profile"), OutProfile);
}

bool TryReadPluginConfigSafetyProfile(FString& OutProfile)
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BlueprintHelper"));
	if (!Plugin.IsValid())
	{
		return false;
	}

	const FString ConfigPath = Plugin->GetBaseDir() / TEXT("Config/FilterPlugin.ini");
	return FPaths::FileExists(ConfigPath)
		&& GConfig
		&& GConfig->GetString(TEXT("BlueprintHelper"), TEXT("SafetyProfile"), OutProfile, ConfigPath)
		&& !OutProfile.IsEmpty();
}

EBlueprintHelperSafetyProfile ParseSafetyProfile(const FString& Profile)
{
	if (Profile.Equals(TEXT("readonly"), ESearchCase::IgnoreCase)
		|| Profile.Equals(TEXT("read_only"), ESearchCase::IgnoreCase)
		|| Profile.Equals(TEXT("read-only"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperSafetyProfile::ReadOnly;
	}
	if (Profile.Equals(TEXT("standard"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperSafetyProfile::Standard;
	}
	if (Profile.Equals(TEXT("autorepair"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperSafetyProfile::AutoRepair;
	}
	return EBlueprintHelperSafetyProfile::Conservative;
}
}

EBlueprintHelperSafetyProfile FBlueprintHelperSafetyProfileResolver::ResolveSafetyProfile()
{
	FString Profile;
	if (TryReadProjectAgentProfileSafetyProfile(Profile) || TryReadPluginConfigSafetyProfile(Profile))
	{
		return ParseSafetyProfile(Profile);
	}

	return EBlueprintHelperSafetyProfile::Conservative;
}

FString FBlueprintHelperSafetyProfileResolver::ResolveSafetyProfileString()
{
	return SafetyProfileToString(ResolveSafetyProfile());
}

bool FBlueprintHelperSafetyProfileResolver::IsAutoRepair()
{
	return ResolveSafetyProfile() == EBlueprintHelperSafetyProfile::AutoRepair;
}
