// BlueprintHelper settings store.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class FJsonValue;

struct FBlueprintHelperSettingView
{
	FString Schema;
	FString Version;
	FString DefaultSettingPath;
	FString ProjectSettingPath;
	FString UserSettingOverridePath;
	FString EffectiveSourcePath;
	FString EffectiveJson;
	FString StatusText;
	FString ErrorText;
	bool bLoaded = false;
	bool bProjectSettingExists = false;
	bool bUserOverrideExists = false;
};

class BLUEPRINTHELPER_API FBlueprintHelperSettingStore
{
public:
	static FBlueprintHelperSettingView Load();
	static bool EnsureProjectSetting(FString& OutPath, FString& OutError);
	static bool LoadEffectiveSettingObject(TSharedPtr<FJsonObject>& OutObject, FString& OutError);
	static bool LoadEffectiveSettingJson(FString& OutJson, FString& OutError);
	static bool TryGetEffectiveJsonValue(const FString& DotPath, TSharedPtr<FJsonValue>& OutValue, FString& OutError);
	static bool TryGetProjectJsonValue(const FString& DotPath, TSharedPtr<FJsonValue>& OutValue, FString& OutError);
	static bool UpdateProjectSettingValue(const FString& DotPath, const FString& NewValue, FString& OutError);
	static bool ResetProjectSettingValue(const FString& DotPath, FString& OutError);
	static bool GetSettingValue(const FString& DotPath, FString& OutCurrentValue, FString& OutDefaultValue, bool& bOutHasProjectOverride, FString& OutError);
	static bool UpdateSettingJsonText(const FString& InputJson, const FString& DotPath, const FString& NewValue, FString& OutJson, FString& OutError);
	static bool RemoveSettingJsonPath(const FString& InputJson, const FString& DotPath, FString& OutJson, FString& OutError);
	static FString GetDefaultSettingPath();
	static FString GetBuiltInDefaultSettingJson();

private:
	static bool LoadFileIfExists(const FString& Path, FString& OutText);
	static FString PrettyPrintJsonOrOriginal(const FString& JsonText);
};
