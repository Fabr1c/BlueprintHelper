#include "Systems/Config/BlueprintHelperProjectConfigPaths.h"

#include "Misc/Paths.h"

FString FBlueprintHelperProjectConfigPaths::GetProjectConfigDir()
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT(".blueprinthelper"));
}

FString FBlueprintHelperProjectConfigPaths::GetAgentProfilePath()
{
	return FPaths::Combine(GetProjectConfigDir(), TEXT("agent-profile.json"));
}

FString FBlueprintHelperProjectConfigPaths::GetGraphLayoutRulesPath()
{
	return FPaths::Combine(GetProjectConfigDir(), TEXT("GraphLayoutRules.json"));
}
