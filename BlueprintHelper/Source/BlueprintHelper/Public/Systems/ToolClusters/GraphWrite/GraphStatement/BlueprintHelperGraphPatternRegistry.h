#pragma once

#include "CoreMinimal.h"

struct BLUEPRINTHELPER_API FBlueprintHelperGraphPatternBinding
{
	bool bEnabled = true;
	TMap<FString, FString> Aliases;
	TMap<FString, FString> PinAliases;
	TMap<FString, FString> Defaults;
};

class BLUEPRINTHELPER_API FBlueprintHelperGraphPatternRegistry
{
public:
	static FBlueprintHelperGraphPatternRegistry& Get();

	const FBlueprintHelperGraphPatternBinding* FindBinding(const FString& PatternName);
	FString ResolveAlias(const FString& PatternName, const FString& Name);
	void ApplyPinAliasesAndDefaults(const FString& PatternName, TMap<FString, FString>& Values);
	void ResetForTests();

private:
	void EnsureLoaded();
	void LoadDirectory(const FString& DirectoryPath);
	void LoadFile(const FString& FilePath);
	void MergeBinding(const FString& PatternName, const FBlueprintHelperGraphPatternBinding& Binding);

	bool bLoaded = false;
	TSet<FString> LoadedFiles;
	TMap<FString, FBlueprintHelperGraphPatternBinding> Bindings;
};
