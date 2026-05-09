// BlueprintHelper Review asset context.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class UBlueprint;
class UDataTable;
class UObject;

enum class EBlueprintHelperReviewAssetKind : uint8
{
	Unknown,
	Blueprint,
	WidgetBlueprint,
	DataTable,
	DataAsset,
	GenericObject
};

inline const TCHAR* BlueprintHelperReviewAssetKindToString(EBlueprintHelperReviewAssetKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperReviewAssetKind::Blueprint:       return TEXT("blueprint");
	case EBlueprintHelperReviewAssetKind::WidgetBlueprint: return TEXT("widget_blueprint");
	case EBlueprintHelperReviewAssetKind::DataTable:       return TEXT("data_table");
	case EBlueprintHelperReviewAssetKind::DataAsset:       return TEXT("data_asset");
	case EBlueprintHelperReviewAssetKind::GenericObject:   return TEXT("generic_object");
	default:                                               return TEXT("unknown");
	}
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
	TWeakObjectPtr<UObject> DefaultObject;

	bool IsValid() const;
	bool IsBlueprintLike() const;

	static FString MakePackageNameFromAssetPath(const FString& InAssetPath);
	static FString MakeObjectPathFromAssetPath(const FString& InAssetPath);
	static FBlueprintHelperReviewAssetContext LoadForAssetPath(const FString& InAssetPath);
};
