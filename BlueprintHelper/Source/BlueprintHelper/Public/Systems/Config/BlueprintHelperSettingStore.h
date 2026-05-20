// BlueprintHelper settings store.

#pragma once

#include "CoreMinimal.h"

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
	static FString GetDefaultSettingPath();
	static FString GetBuiltInDefaultSettingJson();

private:
	static bool LoadFileIfExists(const FString& Path, FString& OutText);
	static FString PrettyPrintJsonOrOriginal(const FString& JsonText);
};
