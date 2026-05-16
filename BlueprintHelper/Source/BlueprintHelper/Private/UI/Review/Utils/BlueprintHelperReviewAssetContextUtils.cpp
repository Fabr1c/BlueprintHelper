// BlueprintHelper Review asset context utility functions implementation.

#include "UI/Review/Utils/BlueprintHelperReviewAssetContextUtils.h"

#include "Blueprint/UserWidget.h"
#include "Engine/Blueprint.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"
#include "WidgetBlueprint.h"

UObject* FBlueprintHelperReviewAssetContextUtils::ResolveLoadedOrLoadObject(
	const FString& ObjectPath,
	const FString& AssetPath)
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

	const FString PackageName =
		FBlueprintHelperReviewAssetContext::MakePackageNameFromAssetPath(AssetPath);
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

void FBlueprintHelperReviewAssetContextUtils::PopulateBlueprintContext(
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

void FBlueprintHelperReviewAssetContextUtils::PopulateClassContext(
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
