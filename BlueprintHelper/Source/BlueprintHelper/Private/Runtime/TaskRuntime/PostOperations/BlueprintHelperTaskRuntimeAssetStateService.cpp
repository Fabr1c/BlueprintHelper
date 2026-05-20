#include "Runtime/TaskRuntime/PostOperations/BlueprintHelperTaskRuntimeAssetStateService.h"

#include "Engine/Blueprint.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

FString FBlueprintHelperTaskRuntimeAssetStateService::NormalizePackageName(const FString& AssetPath)
{
	FString PackageName = AssetPath;
	PackageName.TrimStartAndEndInline();
	const int32 LastSlashIndex = PackageName.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	const int32 DotIndex = PackageName.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	if (DotIndex != INDEX_NONE && (LastSlashIndex == INDEX_NONE || DotIndex > LastSlashIndex))
	{
		PackageName.LeftInline(DotIndex, EAllowShrinking::No);
	}
	return PackageName;
}

FString FBlueprintHelperTaskRuntimeAssetStateService::BuildObjectPath(const FString& AssetPath)
{
	FString ObjectPath = AssetPath;
	ObjectPath.TrimStartAndEndInline();
	const int32 LastSlashIndex = ObjectPath.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	const int32 DotIndex = ObjectPath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	if (DotIndex != INDEX_NONE && (LastSlashIndex == INDEX_NONE || DotIndex > LastSlashIndex))
	{
		return ObjectPath;
	}

	const FString PackageName = NormalizePackageName(ObjectPath);
	if (PackageName.IsEmpty())
	{
		return FString();
	}

	return FString::Printf(TEXT("%s.%s"), *PackageName, *FPackageName::GetShortName(PackageName));
}

FBlueprintHelperTaskRuntimeAssetState FBlueprintHelperTaskRuntimeAssetStateService::ReadState(
	const FString& AssetPath)
{
	FBlueprintHelperTaskRuntimeAssetState State;
	State.AssetPath = AssetPath;
	State.PackageName = NormalizePackageName(AssetPath);
	State.ObjectPath = BuildObjectPath(AssetPath);

	UPackage* Package = State.PackageName.IsEmpty()
		? nullptr
		: FindPackage(nullptr, *State.PackageName);
	State.bPackageLoaded = Package != nullptr;
	State.bPackageDirty = Package ? Package->IsDirty() : false;

	UObject* Asset = State.ObjectPath.IsEmpty()
		? nullptr
		: StaticFindObject(UObject::StaticClass(), nullptr, *State.ObjectPath);
	if (!Asset && !State.ObjectPath.IsEmpty())
	{
		Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *State.ObjectPath);
	}

	if (!Package && !State.PackageName.IsEmpty())
	{
		Package = FindPackage(nullptr, *State.PackageName);
		State.bPackageLoaded = Package != nullptr;
		State.bPackageDirty = Package ? Package->IsDirty() : false;
	}

	State.bAssetLoaded = Asset != nullptr;
	State.bIsBlueprint = Cast<UBlueprint>(Asset) != nullptr;
	return State;
}
