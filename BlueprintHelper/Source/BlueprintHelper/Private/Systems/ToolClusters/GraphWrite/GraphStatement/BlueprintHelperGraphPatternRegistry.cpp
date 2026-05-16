#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphPatternRegistry.h"

#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphPatternRegistryUtils.h"
FBlueprintHelperGraphPatternRegistry& FBlueprintHelperGraphPatternRegistry::Get()
{
	static FBlueprintHelperGraphPatternRegistry Registry;
	return Registry;
}

const FBlueprintHelperGraphPatternBinding* FBlueprintHelperGraphPatternRegistry::FindBinding(const FString& PatternName)
{
	EnsureLoaded();
	return Bindings.Find(FBlueprintHelperGraphPatternRegistryUtils::NormalizePatternKey(PatternName));
}

FString FBlueprintHelperGraphPatternRegistry::ResolveAlias(const FString& PatternName, const FString& Name)
{
	const FBlueprintHelperGraphPatternBinding* Binding = FindBinding(PatternName);
	if (!Binding || !Binding->bEnabled)
	{
		return Name;
	}

	if (const FString* Alias = Binding->Aliases.Find(FBlueprintHelperGraphPatternRegistryUtils::NormalizeLookupKey(Name)))
	{
		return *Alias;
	}
	return Name;
}

void FBlueprintHelperGraphPatternRegistry::ApplyPinAliasesAndDefaults(
	const FString& PatternName,
	TMap<FString, FString>& Values)
{
	const FBlueprintHelperGraphPatternBinding* Binding = FindBinding(PatternName);
	if (!Binding || !Binding->bEnabled)
	{
		return;
	}

	for (const TPair<FString, FString>& AliasPair : Binding->PinAliases)
	{
		FString Value;
		if (Values.RemoveAndCopyValue(AliasPair.Key, Value))
		{
			Values.FindOrAdd(AliasPair.Value, Value);
		}
	}

	for (const TPair<FString, FString>& DefaultPair : Binding->Defaults)
	{
		Values.FindOrAdd(DefaultPair.Key, DefaultPair.Value);
	}
}

void FBlueprintHelperGraphPatternRegistry::ResetForTests()
{
	bLoaded = false;
	LoadedFiles.Reset();
	Bindings.Reset();
}

void FBlueprintHelperGraphPatternRegistry::EnsureLoaded()
{
	if (bLoaded)
	{
		return;
	}
	bLoaded = true;

	if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BlueprintHelper")))
	{
		LoadDirectory(FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"), TEXT("GraphPatterns")));
	}

	LoadDirectory(FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("BlueprintHelper"), TEXT("BlueprintHelper"), TEXT("Resources"), TEXT("GraphPatterns")));
	LoadDirectory(FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("BlueprintHelper"), TEXT("Resources"), TEXT("GraphPatterns")));
	LoadDirectory(FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("BlueprintHelper"), TEXT("GraphPatterns")));
}

void FBlueprintHelperGraphPatternRegistry::LoadDirectory(const FString& DirectoryPath)
{
	if (DirectoryPath.IsEmpty() || !IFileManager::Get().DirectoryExists(*DirectoryPath))
	{
		return;
	}

	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(DirectoryPath, TEXT("*.json")), true, false);
	Files.Sort();

	for (const FString& FileName : Files)
	{
		LoadFile(FPaths::Combine(DirectoryPath, FileName));
	}
}

void FBlueprintHelperGraphPatternRegistry::LoadFile(const FString& FilePath)
{
	const FString NormalizedFilePath = FPaths::ConvertRelativePathToFull(FilePath);
	if (LoadedFiles.Contains(NormalizedFilePath))
	{
		return;
	}
	LoadedFiles.Add(NormalizedFilePath);

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *NormalizedFilePath))
	{
		return;
	}

	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return;
	}

	FString PatternName;
	if (!JsonObject->TryGetStringField(TEXT("pattern"), PatternName) || PatternName.TrimStartAndEnd().IsEmpty())
	{
		return;
	}

	FBlueprintHelperGraphPatternBinding Binding;
	JsonObject->TryGetBoolField(TEXT("enabled"), Binding.bEnabled);
	FBlueprintHelperGraphPatternRegistryUtils::ReadStringMapField(JsonObject, TEXT("aliases"), Binding.Aliases, true);
	FBlueprintHelperGraphPatternRegistryUtils::ReadStringMapField(JsonObject, TEXT("pin_aliases"), Binding.PinAliases, true);
	FBlueprintHelperGraphPatternRegistryUtils::ReadStringMapField(JsonObject, TEXT("defaults"), Binding.Defaults, false);
	MergeBinding(PatternName, Binding);
}

void FBlueprintHelperGraphPatternRegistry::MergeBinding(
	const FString& PatternName,
	const FBlueprintHelperGraphPatternBinding& Binding)
{
	FBlueprintHelperGraphPatternBinding& Target = Bindings.FindOrAdd(FBlueprintHelperGraphPatternRegistryUtils::NormalizePatternKey(PatternName));
	Target.bEnabled = Binding.bEnabled;

	for (const TPair<FString, FString>& Pair : Binding.Aliases)
	{
		Target.Aliases.Add(Pair.Key, Pair.Value);
	}
	for (const TPair<FString, FString>& Pair : Binding.PinAliases)
	{
		Target.PinAliases.Add(Pair.Key, Pair.Value);
	}
	for (const TPair<FString, FString>& Pair : Binding.Defaults)
	{
		Target.Defaults.Add(Pair.Key, Pair.Value);
	}
}
