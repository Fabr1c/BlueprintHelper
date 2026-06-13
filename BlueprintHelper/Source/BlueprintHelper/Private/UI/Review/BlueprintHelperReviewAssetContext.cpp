// BlueprintHelper Review asset context implementation.

#include "UI/Review/BlueprintHelperReviewAssetContext.h"

#include "Blueprint/UserWidget.h"
#include "Engine/Blueprint.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Misc/PackageName.h"
#include "Shared/BlueprintHelperUserDefinedStructVersionCompat.h"
#include "UI/Review/Utils/BlueprintHelperReviewAssetContextUtils.h"
#include "UObject/UObjectGlobals.h"
#include "WidgetBlueprint.h"

bool FBlueprintHelperReviewAssetContext::IsValid() const
{
	return AssetObject.IsValid();
}

bool FBlueprintHelperReviewAssetContext::IsBlueprintLike() const
{
	return AssetKind == EBlueprintHelperReviewAssetKind::Blueprint
		|| AssetKind == EBlueprintHelperReviewAssetKind::WidgetBlueprint;
}

FString FBlueprintHelperReviewAssetContext::MakePackageNameFromAssetPath(const FString& InAssetPath)
{
	if (InAssetPath.IsEmpty())
	{
		return FString();
	}
	if (FPackageName::IsValidObjectPath(InAssetPath))
	{
		return FPackageName::ObjectPathToPackageName(InAssetPath);
	}

	FString PackageName = InAssetPath;
	int32 SubObjectIndex = INDEX_NONE;
	if (PackageName.FindChar(TEXT(':'), SubObjectIndex))
	{
		PackageName = PackageName.Left(SubObjectIndex);
	}

	int32 ObjectIndex = INDEX_NONE;
	if (PackageName.FindChar(TEXT('.'), ObjectIndex))
	{
		PackageName = PackageName.Left(ObjectIndex);
	}
	return PackageName;
}

FString FBlueprintHelperReviewAssetContext::MakeObjectPathFromAssetPath(const FString& InAssetPath)
{
	int32 ObjectSeparatorIndex = INDEX_NONE;
	if (InAssetPath.FindChar(TEXT('.'), ObjectSeparatorIndex)
		&& FPackageName::IsValidObjectPath(InAssetPath))
	{
		return InAssetPath;
	}

	const FString PackageName = MakePackageNameFromAssetPath(InAssetPath);
	if (!FPackageName::IsValidLongPackageName(PackageName))
	{
		return InAssetPath;
	}
	return FString::Printf(
		TEXT("%s.%s"),
		*PackageName,
		*FPackageName::GetLongPackageAssetName(PackageName));
}

FBlueprintHelperReviewAssetContext FBlueprintHelperReviewAssetContext::LoadForAssetPath(const FString& InAssetPath)
{
	FBlueprintHelperReviewAssetContext Context;
	Context.PackageName = MakePackageNameFromAssetPath(InAssetPath);
	Context.ObjectPath = MakeObjectPathFromAssetPath(InAssetPath);
	Context.AssetPath = Context.PackageName.IsEmpty() ? InAssetPath : Context.PackageName;

	UObject* Object = FBlueprintHelperReviewAssetContextUtils::ResolveLoadedOrLoadObject(
		Context.ObjectPath,
		InAssetPath);
	if (!Object)
	{
		return Context;
	}

	Context.AssetObject = Object;
	if (UBlueprint* Blueprint = Cast<UBlueprint>(Object))
	{
		FBlueprintHelperReviewAssetContextUtils::PopulateBlueprintContext(Context, Blueprint);
		return Context;
	}
	if (UDataTable* DataTable = Cast<UDataTable>(Object))
	{
		Context.DataTable = DataTable;
		Context.AssetKind = EBlueprintHelperReviewAssetKind::DataTable;
		return Context;
	}
	if (UUserDefinedStruct* Structure = Cast<UUserDefinedStruct>(Object))
	{
		Context.Structure = Structure;
		Context.AssetKind = EBlueprintHelperReviewAssetKind::Structure;
		return Context;
	}
	if (Cast<UDataAsset>(Object))
	{
		Context.AssetKind = EBlueprintHelperReviewAssetKind::DataAsset;
		return Context;
	}
	if (UClass* Class = Cast<UClass>(Object))
	{
		FBlueprintHelperReviewAssetContextUtils::PopulateClassContext(Context, Class);
		if (Context.AssetKind != EBlueprintHelperReviewAssetKind::Unknown)
		{
			return Context;
		}
	}

	Context.AssetKind = EBlueprintHelperReviewAssetKind::GenericObject;
	return Context;
}
