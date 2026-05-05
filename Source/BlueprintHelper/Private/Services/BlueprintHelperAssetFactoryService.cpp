// BlueprintHelper Service Layer — Asset Factory 服务实现

#include "Services/BlueprintHelperAssetFactoryService.h"
#include "Structure/BlueprintHelperAssetFactoryTypes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "Factories/DataAssetFactory.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "PackageTools.h"
#include "UObject/SavePackage.h"

FBlueprintHelperAssetFactoryService::FBlueprintHelperAssetFactoryService() = default;

FBlueprintHelperAssetFactoryData FBlueprintHelperAssetFactoryService::CreateAsset(
	const FString& AssetPath,
	EBlueprintHelperAssetType AssetType,
	const FString& ParentClass,
	const FString& ValueType,
	EBlueprintHelperAssetCollisionPolicy CollisionPolicy) const
{
	FBlueprintHelperAssetFactoryData Data;
	Data.Factory.AssetType = AssetType;
	Data.Factory.FactoryType = AssetTypeToFactoryType(AssetType);
	Data.Factory.ParentClass = ParentClass;
	Data.Factory.ValueType = ValueType;
	Data.Asset.AssetPath = AssetPath;
	Data.Asset.AssetClass = AssetTypeToAssetClass(AssetType);
	Data.Collision.Policy = CollisionPolicy;

	// ─── 冲突检查 ───
	const bool bExists = AssetExists(AssetPath);
	Data.Asset.bAlreadyExisted = bExists;

	if (bExists)
	{
		if (CollisionPolicy == EBlueprintHelperAssetCollisionPolicy::ReuseIfExists)
		{
			// 检查同类型
			const FString ExistingClass = GetExistingAssetClass(AssetPath);
			if (ExistingClass.Equals(Data.Asset.AssetClass, ESearchCase::IgnoreCase))
			{
				Data.Collision.bHandled = true;
				Data.Collision.ExistingAssetPath = AssetPath;
				Data.Asset.bCreated = false;
				return Data;
			}
			// 类型不匹配
			Data.Asset.bCreated = false;
			return Data; // 调用方检测 asset_type_mismatch
		}
		// FailIfExists：不创建
		Data.Asset.bCreated = false;
		Data.Collision.bHandled = false;
		return Data;
	}

	// ─── 创建资产 ───
	bool bCreated = false;
	switch (AssetType)
	{
	case EBlueprintHelperAssetType::BlueprintClass:
		bCreated = CreateBlueprintClass(AssetPath, ParentClass.IsEmpty() ? TEXT("Actor") : ParentClass);
		break;
	case EBlueprintHelperAssetType::BlueprintInterface:
		bCreated = CreateBlueprintInterface(AssetPath);
		break;
	case EBlueprintHelperAssetType::Structure:
		bCreated = CreateStructure(AssetPath);
		break;
	case EBlueprintHelperAssetType::InputAction:
		bCreated = CreateInputAction(AssetPath, ValueType.IsEmpty() ? TEXT("bool") : ValueType);
		break;
	case EBlueprintHelperAssetType::InputMappingContext:
		bCreated = CreateInputMappingContext(AssetPath);
		break;
	case EBlueprintHelperAssetType::DataAsset:
		bCreated = CreateDataAsset(AssetPath, Data.Factory.DataAssetClass);
		break;
	default:
		break;
	}

	Data.Asset.bCreated = bCreated;

	if (bCreated)
	{
		Data.Collision.bHandled = false;
	}

	return Data;
}

bool FBlueprintHelperAssetFactoryService::ShouldCompile(EBlueprintHelperAssetType AssetType)
{
	switch (AssetType)
	{
	case EBlueprintHelperAssetType::BlueprintClass:
	case EBlueprintHelperAssetType::BlueprintInterface:
	case EBlueprintHelperAssetType::WidgetBlueprint:
		return true;
	default:
		return false;
	}
}

bool FBlueprintHelperAssetFactoryService::ShouldSave(EBlueprintHelperAssetType AssetType)
{
	return true; // 所有资产类型创建后都应该保存
}

EBlueprintHelperFactoryType FBlueprintHelperAssetFactoryService::AssetTypeToFactoryType(EBlueprintHelperAssetType Type)
{
	switch (Type)
	{
	case EBlueprintHelperAssetType::BlueprintClass:      return EBlueprintHelperFactoryType::Blueprint;
	case EBlueprintHelperAssetType::BlueprintInterface:   return EBlueprintHelperFactoryType::BlueprintInterface;
	case EBlueprintHelperAssetType::Structure:            return EBlueprintHelperFactoryType::Structure;
	case EBlueprintHelperAssetType::InputAction:          return EBlueprintHelperFactoryType::EnhancedInputAction;
	case EBlueprintHelperAssetType::InputMappingContext:  return EBlueprintHelperFactoryType::EnhancedInputMappingContext;
	case EBlueprintHelperAssetType::DataAsset:            return EBlueprintHelperFactoryType::DataAsset;
	case EBlueprintHelperAssetType::DataTable:            return EBlueprintHelperFactoryType::DataTable;
	case EBlueprintHelperAssetType::WidgetBlueprint:      return EBlueprintHelperFactoryType::WidgetBlueprint;
	default:                                              return EBlueprintHelperFactoryType::Unknown;
	}
}

FString FBlueprintHelperAssetFactoryService::AssetTypeToAssetClass(EBlueprintHelperAssetType Type)
{
	switch (Type)
	{
	case EBlueprintHelperAssetType::BlueprintClass:      return TEXT("Blueprint");
	case EBlueprintHelperAssetType::BlueprintInterface:   return TEXT("Blueprint");
	case EBlueprintHelperAssetType::Structure:            return TEXT("UserDefinedStruct");
	case EBlueprintHelperAssetType::InputAction:          return TEXT("InputAction");
	case EBlueprintHelperAssetType::InputMappingContext:  return TEXT("InputMappingContext");
	case EBlueprintHelperAssetType::DataAsset:            return TEXT("DataAsset");
	case EBlueprintHelperAssetType::DataTable:            return TEXT("DataTable");
	case EBlueprintHelperAssetType::WidgetBlueprint:      return TEXT("WidgetBlueprint");
	default:                                              return TEXT("Unknown");
	}
}

bool FBlueprintHelperAssetFactoryService::AssetExists(const FString& AssetPath)
{
	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FAssetData AssetData = AssetRegistry.Get().GetAssetByObjectPath(FSoftObjectPath(AssetPath));
	return AssetData.IsValid();
}

FString FBlueprintHelperAssetFactoryService::GetExistingAssetClass(const FString& AssetPath)
{
	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FAssetData AssetData = AssetRegistry.Get().GetAssetByObjectPath(FSoftObjectPath(AssetPath));
	if (AssetData.IsValid())
	{
		return AssetData.AssetClassPath.GetAssetName().ToString();
	}
	return TEXT("");
}

bool FBlueprintHelperAssetFactoryService::CreateBlueprintClass(const FString& AssetPath, const FString& ParentClass)
{
	// 找到父类 UClass
	UClass* ParentUClass = AActor::StaticClass(); // 默认 Actor
	if (!ParentClass.IsEmpty())
	{
		UClass* FoundClass = FindObject<UClass>(nullptr, *ParentClass);
		if (!FoundClass)
		{
			FString FullPath = FString::Printf(TEXT("/Script/Engine.%s"), *ParentClass);
			FoundClass = FindObject<UClass>(nullptr, *FullPath);
		}
		if (FoundClass) { ParentUClass = FoundClass; }
	}

	const FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	const FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package) return false;

	UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
	Factory->ParentClass = ParentUClass;

	UObject* NewAsset = Factory->FactoryCreateNew(
		ParentUClass, Package, *AssetName,
		RF_Public | RF_Standalone, nullptr, GWarn);

	if (NewAsset)
	{
		FAssetRegistryModule::AssetCreated(NewAsset);
		Package->MarkPackageDirty();
		return true;
	}

	return false;
}

bool FBlueprintHelperAssetFactoryService::CreateBlueprintInterface(const FString& AssetPath)
{
	const FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	const FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package) return false;

	UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
	Factory->BlueprintType = BPTYPE_Interface;

	UObject* NewAsset = Factory->FactoryCreateNew(
		UBlueprint::StaticClass(), Package, *AssetName,
		RF_Public | RF_Standalone, nullptr, GWarn);

	if (NewAsset)
	{
		FAssetRegistryModule::AssetCreated(NewAsset);
		Package->MarkPackageDirty();
		return true;
	}

	return false;
}

bool FBlueprintHelperAssetFactoryService::CreateStructure(const FString& AssetPath)
{
	const FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	const FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package) return false;

	// 通过类名查找 UUserDefinedStruct
	UClass* StructClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.UserDefinedStruct"));
	if (!StructClass) return false;

	UObject* NewAsset = NewObject<UObject>(Package, StructClass, *AssetName, RF_Public | RF_Standalone);
	if (!NewAsset) return false;

	FAssetRegistryModule::AssetCreated(NewAsset);
	Package->MarkPackageDirty();
	return true;
}

bool FBlueprintHelperAssetFactoryService::CreateInputAction(const FString& AssetPath, const FString& ValueType)
{
	const FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	const FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package) return false;

	UInputAction* NewAction = NewObject<UInputAction>(Package, *AssetName, RF_Public | RF_Standalone);

	// 设置值类型
	if (ValueType.Equals(TEXT("axis1d"), ESearchCase::IgnoreCase))
		NewAction->ValueType = EInputActionValueType::Axis1D;
	else if (ValueType.Equals(TEXT("axis2d"), ESearchCase::IgnoreCase))
		NewAction->ValueType = EInputActionValueType::Axis2D;
	else if (ValueType.Equals(TEXT("axis3d"), ESearchCase::IgnoreCase))
		NewAction->ValueType = EInputActionValueType::Axis3D;
	else
		NewAction->ValueType = EInputActionValueType::Boolean;

	FAssetRegistryModule::AssetCreated(NewAction);
	Package->MarkPackageDirty();
	return true;
}

bool FBlueprintHelperAssetFactoryService::CreateInputMappingContext(const FString& AssetPath)
{
	const FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	const FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package) return false;

	UInputMappingContext* NewContext = NewObject<UInputMappingContext>(Package, *AssetName, RF_Public | RF_Standalone);

	FAssetRegistryModule::AssetCreated(NewContext);
	Package->MarkPackageDirty();
	return true;
}

bool FBlueprintHelperAssetFactoryService::CreateDataAsset(const FString& AssetPath, const FString& AssetClass)
{
	const FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
	const FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);

	UClass* Class = UDataAsset::StaticClass();
	if (!AssetClass.IsEmpty())
	{
		UClass* FoundClass = FindObject<UClass>(nullptr, *AssetClass);
		if (FoundClass && FoundClass->IsChildOf(UDataAsset::StaticClass()))
		{
			Class = FoundClass;
		}
	}

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package) return false;

	UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
	Factory->DataAssetClass = Class;

	UObject* NewAsset = Factory->FactoryCreateNew(
		Class, Package, *AssetName,
		RF_Public | RF_Standalone, nullptr, GWarn);

	if (NewAsset)
	{
		FAssetRegistryModule::AssetCreated(NewAsset);
		Package->MarkPackageDirty();
		return true;
	}

	return false;
}
