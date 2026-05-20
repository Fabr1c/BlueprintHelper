#include "Systems/GraphLayout/BlueprintHelperGraphLayoutRuleSourceResolver.h"

#include "Misc/Paths.h"
#include "Systems/Config/BlueprintHelperProjectConfigPaths.h"
#include "Systems/Config/BlueprintHelperRuntimeSettingResolver.h"
#include "Systems/Config/BlueprintHelperSettingStore.h"

FString FBlueprintHelperGraphLayoutRuleSourceResolver::ResolveRuleSourcePath()
{
	const FString ConfiguredSource = FBlueprintHelperRuntimeSettingResolver::GetString(
		TEXT("graph_layout.rules_source"),
		TEXT("GraphLayoutRules.json")).TrimStartAndEnd();

	if (ConfiguredSource.IsEmpty() || ConfiguredSource == TEXT("GraphLayoutRules.json"))
	{
		return GetDefaultRuleSourcePath();
	}

	if (FPaths::IsRelative(ConfiguredSource))
	{
		const FString Candidate = NormalizeAbsolutePath(
			FPaths::Combine(FBlueprintHelperProjectConfigPaths::GetProjectConfigDir(), ConfiguredSource));
		return IsUnderDirectory(Candidate, FBlueprintHelperProjectConfigPaths::GetProjectConfigDir())
			? Candidate
			: GetDefaultRuleSourcePath();
	}

	const FString AbsolutePath = NormalizeAbsolutePath(ConfiguredSource);
	return IsAllowedAbsolutePath(AbsolutePath)
		? AbsolutePath
		: GetDefaultRuleSourcePath();
}

FString FBlueprintHelperGraphLayoutRuleSourceResolver::GetDefaultRuleSourcePath()
{
	return FBlueprintHelperProjectConfigPaths::GetGraphLayoutRulesPath();
}

bool FBlueprintHelperGraphLayoutRuleSourceResolver::IsAllowedAbsolutePath(const FString& AbsolutePath)
{
	return IsUnderDirectory(AbsolutePath, FPaths::ProjectDir()) ||
		IsUnderDirectory(AbsolutePath, FPaths::GetPath(FBlueprintHelperSettingStore::GetDefaultSettingPath()));
}

FString FBlueprintHelperGraphLayoutRuleSourceResolver::NormalizeAbsolutePath(const FString& Path)
{
	FString Result = FPaths::ConvertRelativePathToFull(Path);
	FPaths::NormalizeFilename(Result);
	FPaths::CollapseRelativeDirectories(Result);
	return Result;
}

bool FBlueprintHelperGraphLayoutRuleSourceResolver::IsUnderDirectory(
	const FString& CandidatePath,
	const FString& RootDirectory)
{
	if (CandidatePath.IsEmpty() || RootDirectory.IsEmpty())
	{
		return false;
	}

	FString NormalizedCandidate = NormalizeAbsolutePath(CandidatePath);
	FString NormalizedRoot = NormalizeAbsolutePath(RootDirectory);
	FPaths::NormalizeDirectoryName(NormalizedRoot);
	if (!NormalizedRoot.EndsWith(TEXT("/")))
	{
		NormalizedRoot += TEXT("/");
	}
	return NormalizedCandidate.StartsWith(NormalizedRoot, ESearchCase::IgnoreCase);
}
