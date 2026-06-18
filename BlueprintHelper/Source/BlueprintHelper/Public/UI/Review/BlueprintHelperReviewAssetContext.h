// BlueprintHelper Review asset context.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class UBlueprint;
class UDataTable;
class UMaterial;
class UMaterialInstanceConstant;
class UMaterialInterface;
class UObject;
class UUserDefinedStruct;

enum class EBlueprintHelperReviewAssetKind : uint8
{
	Unknown,
	Blueprint,
	WidgetBlueprint,
	DataTable,
	DataAsset,
	Material,
	MaterialInstance,
	Structure,
	GenericObject
};

inline const TCHAR* BlueprintHelperReviewAssetKindToString(EBlueprintHelperReviewAssetKind Kind)
{
	struct FBlueprintHelperReviewAssetKindName
	{
		EBlueprintHelperReviewAssetKind Kind;
		const TCHAR* Name;
	};

	static const FBlueprintHelperReviewAssetKindName KindNames[] =
	{
		{ EBlueprintHelperReviewAssetKind::Blueprint, TEXT("blueprint") },
		{ EBlueprintHelperReviewAssetKind::WidgetBlueprint, TEXT("widget_blueprint") },
		{ EBlueprintHelperReviewAssetKind::DataTable, TEXT("data_table") },
		{ EBlueprintHelperReviewAssetKind::DataAsset, TEXT("data_asset") },
		{ EBlueprintHelperReviewAssetKind::Material, TEXT("material") },
		{ EBlueprintHelperReviewAssetKind::MaterialInstance, TEXT("material_instance") },
		{ EBlueprintHelperReviewAssetKind::Structure, TEXT("structure") },
		{ EBlueprintHelperReviewAssetKind::GenericObject, TEXT("generic_object") }
	};

	for (const FBlueprintHelperReviewAssetKindName& Entry : KindNames)
	{
		if (Entry.Kind == Kind)
		{
			return Entry.Name;
		}
	}
	return TEXT("unknown");
}

struct BLUEPRINTHELPER_API FBlueprintHelperReviewAssetContext
{
	FString AssetPath;
	FString PackageName;
	FString ObjectPath;
	EBlueprintHelperReviewAssetKind AssetKind = EBlueprintHelperReviewAssetKind::Unknown;
	TWeakObjectPtr<UObject> AssetObject;
	TWeakObjectPtr<UBlueprint> Blueprint;
	TWeakObjectPtr<UDataTable> DataTable;
	TWeakObjectPtr<UMaterialInterface> MaterialInterface;
	TWeakObjectPtr<UMaterial> Material;
	TWeakObjectPtr<UMaterialInstanceConstant> MaterialInstance;
	TWeakObjectPtr<UUserDefinedStruct> Structure;
	TWeakObjectPtr<UObject> DefaultObject;

	bool IsValid() const;
	bool IsBlueprintLike() const;

	static FString MakePackageNameFromAssetPath(const FString& InAssetPath);
	static FString MakeObjectPathFromAssetPath(const FString& InAssetPath);
	static FBlueprintHelperReviewAssetContext LoadForAssetPath(const FString& InAssetPath);
};
