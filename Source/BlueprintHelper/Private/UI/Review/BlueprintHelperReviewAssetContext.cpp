// BlueprintHelper Review asset context implementation.

#include "UI/Review/BlueprintHelperReviewAssetContext.h"

#include "Blueprint/UserWidget.h"
#include "Engine/Blueprint.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"
#include "WidgetBlueprint.h"

namespace BlueprintHelperReviewAssetContextPrivate
{
	static UObject* ResolveLoadedOrLoadObject(const FString& ObjectPath, const FString& AssetPath)
	{
		if (ObjectPath.IsEmpty())
		{
			return nullptr;
		}

		if (UObject* ExistingObject = FindObject<UObject>(nullptr, *ObjectPath))
		{
			return ExistingObject;
		}
		if (ObjectPath != AssetPath)
		{
			if (UObject* ExistingObject = FindObject<UObject>(nullptr, *AssetPath))
			{
				return ExistingObject;
			}
		}

		const FString PackageName = FBlueprintHelperReviewAssetContext::MakePackageNameFromAssetPath(AssetPath);
		if (!FPackageName::IsValidLongPackageName(PackageName)
			|| !FPackageName::DoesPackageExist(PackageName))
		{
			return nullptr;
		}

		if (UObject* LoadedObject = LoadObject<UObject>(nullptr, *ObjectPath))
		{
			return LoadedObject;
		}
		if (ObjectPath != AssetPath)
		{
			return LoadObject<UObject>(nullptr, *AssetPath);
		}
		return nullptr;
	}

	static void PopulateBlueprintContext(
		FBlueprintHelperReviewAssetContext& Context,
		UBlueprint* Blueprint)
	{
		Context.Blueprint = Blueprint;
		Context.AssetKind = Cast<UWidgetBlueprint>(Blueprint)
			? EBlueprintHelperReviewAssetKind::WidgetBlueprint
			: EBlueprintHelperReviewAssetKind::Blueprint;
		if (Blueprint && Blueprint->GeneratedClass)
		{
			Context.DefaultObject = Blueprint->GeneratedClass->GetDefaultObject();
			if (Blueprint->GeneratedClass->IsChildOf(UUserWidget::StaticClass()))
			{
				Context.AssetKind = EBlueprintHelperReviewAssetKind::WidgetBlueprint;
			}
		}
	}

	static void PopulateClassContext(
		FBlueprintHelperReviewAssetContext& Context,
		UClass* Class)
	{
		if (!Class)
		{
			return;
		}

		Context.DefaultObject = Class->GetDefaultObject();
		if (UBlueprint* GeneratedByBlueprint = Cast<UBlueprint>(Class->ClassGeneratedBy))
		{
			PopulateBlueprintContext(Context, GeneratedByBlueprint);
			if (Context.AssetObject.IsValid())
			{
				Context.AssetObject = Class;
			}
		}
		else if (Class->IsChildOf(UUserWidget::StaticClass()))
		{
			Context.AssetKind = EBlueprintHelperReviewAssetKind::WidgetBlueprint;
		}
	}
}

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
	if (FPackageName::IsValidObjectPath(InAssetPath))
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
	Context.AssetPath = InAssetPath;
	Context.PackageName = MakePackageNameFromAssetPath(InAssetPath);
	Context.ObjectPath = MakeObjectPathFromAssetPath(InAssetPath);

	UObject* Object = BlueprintHelperReviewAssetContextPrivate::ResolveLoadedOrLoadObject(
		Context.ObjectPath,
		InAssetPath);
	if (!Object)
	{
		return Context;
	}

	Context.AssetObject = Object;
	if (UBlueprint* Blueprint = Cast<UBlueprint>(Object))
	{
		BlueprintHelperReviewAssetContextPrivate::PopulateBlueprintContext(Context, Blueprint);
		return Context;
	}
	if (UDataTable* DataTable = Cast<UDataTable>(Object))
	{
		Context.DataTable = DataTable;
		Context.AssetKind = EBlueprintHelperReviewAssetKind::DataTable;
		return Context;
	}
	if (Cast<UDataAsset>(Object))
	{
		Context.AssetKind = EBlueprintHelperReviewAssetKind::DataAsset;
		return Context;
	}
	if (UClass* Class = Cast<UClass>(Object))
	{
		BlueprintHelperReviewAssetContextPrivate::PopulateClassContext(Context, Class);
		if (Context.AssetKind != EBlueprintHelperReviewAssetKind::Unknown)
		{
			return Context;
		}
	}

	Context.AssetKind = EBlueprintHelperReviewAssetKind::GenericObject;
	return Context;
}
