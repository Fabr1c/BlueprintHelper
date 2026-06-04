#include "Systems/Config/BlueprintHelperProjectConfigPaths.h"

#include "Misc/Paths.h"

FString FBlueprintHelperProjectConfigPaths::GetProjectConfigDir()
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT(".blueprinthelper"));
}

FString FBlueprintHelperProjectConfigPaths::GetProjectProfilePath()
{
	return FPaths::Combine(GetProjectConfigDir(), TEXT("project-profile.json"));
}

FString FBlueprintHelperProjectConfigPaths::GetAgentProfilePath()
{
	return GetProjectProfilePath();
}

FString FBlueprintHelperProjectConfigPaths::GetGraphLayoutRulesPath()
{
	return FPaths::Combine(GetProjectConfigDir(), TEXT("GraphLayoutRules.json"));
}

FString FBlueprintHelperProjectConfigPaths::GetProjectSettingPath()
{
	return FPaths::Combine(GetProjectConfigDir(), TEXT("setting.json"));
}

FString FBlueprintHelperProjectConfigPaths::GetUserSettingOverridePath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BlueprintHelper"), TEXT("setting.user.json"));
}
