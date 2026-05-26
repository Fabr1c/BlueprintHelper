// BlueprintHelper Service Layer 。Blueprint Class Settings 服务
// 。6 簇：蓝图 Class Settings 读写

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintClassSettings/BlueprintHelperClassSettingsTypes.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class UBlueprint;
class UClass;
class UObject;
class FBlueprintHelperGraphResolver;

/**
 * Blueprint Class Settings 服务，提供以下功能：
 * 1. 读取蓝图 Class Settings。 * 2. 添加 / 移除 Implemented Interface。 * 3. 设置 Class Defaults / CDO 默认属性。 */
class BLUEPRINTHELPER_API FBlueprintHelperClassSettingsService
{
public:
	explicit FBlueprintHelperClassSettingsService(const FBlueprintHelperGraphResolver& InResolver);

	/** 读取蓝图 Class Settings，返。parent_class / generated_class / implemented_interfaces / class_default_count。*/
	FBlueprintHelperToolResultBase ReadClassSettings(const FString& AssetPath) const;

	/** 添加单个 Implemented Interface。*/
	FBlueprintHelperToolResultBase AddImplementedInterface(
		const FString& AssetPath,
		const FString& InterfacePath,
		bool bDryRun = false) const;

	/** 批量添加 Implemented Interfaces，事务式：任一无效则全部不应用。*/
	FBlueprintHelperToolResultBase AddImplementedInterfaces(
		const FString& AssetPath,
		const TArray<FString>& InterfacePaths,
		bool bDryRun = false) const;

	/** 移除单个 Implemented Interface。*/
	FBlueprintHelperToolResultBase RemoveImplementedInterface(
		const FString& AssetPath,
		const FString& InterfacePath,
		bool bDryRun = false) const;

	/** 批量移除 Implemented Interfaces，事务式：任一无效则全部不应用。*/
	FBlueprintHelperToolResultBase RemoveImplementedInterfaces(
		const FString& AssetPath,
		const TArray<FString>& InterfacePaths,
		bool bDryRun = false) const;

	/** 设置单个 Class Default 属性值。*/
	FBlueprintHelperToolResultBase SetClassDefaultProperty(
		const FString& AssetPath,
		const FString& PropertyPath,
		const TSharedPtr<FJsonValue>& Value,
		bool bDryRun = false) const;

	/** 批量设置 Class Default 属性值，事务式：任一无效则全部不应用。*/
	FBlueprintHelperToolResultBase SetClassDefaultProperties(
		const FString& AssetPath,
		const TArray<FBlueprintHelperClassDefaultPropertySetting>& Settings,
		bool bDryRun = false) const;

	/** Reparent a Blueprint to a new parent class. */
	FBlueprintHelperToolResultBase ReparentBlueprint(
		const FString& AssetPath,
		const FString& NewParentClassPath,
		bool bDryRun = false) const;

private:
	UBlueprint* ResolveBlueprint(const FString& AssetPath, FString& OutErrorCode, FString& OutErrorMessage) const;
	UClass* ResolveInterfaceClass(const FString& InterfacePath, FString& OutCode, FString& OutMessage) const;
	UClass* ResolveParentClass(const FString& ParentClassPath, FString& OutCode, FString& OutMessage) const;
	UObject* ResolveClassDefaultObject(UBlueprint* Blueprint, FString& OutCode, FString& OutMessage) const;

	static FString NormalizeObjectPath(const FString& Path);
	static FString GetClassPath(const UClass* Class);
	static FString GetGeneratedClassShortName(const UBlueprint* Blueprint);
	static FString GetInterfaceAssetPath(const UClass* InterfaceClass);
	static int32 CountEditableClassDefaults(UObject* CDO);

	static bool IsInterfaceImplemented(UBlueprint* Blueprint, UClass* InterfaceClass);
	static bool AddInterfaceToBlueprint(UBlueprint* Blueprint, UClass* InterfaceClass);
	static bool RemoveInterfaceFromBlueprint(UBlueprint* Blueprint, UClass* InterfaceClass);

	static bool ResolvePropertyPath(
		UObject* RootObject,
		const FString& PropertyPath,
		FProperty*& OutProperty,
		void*& OutValuePtr,
		FString& OutExpectedType,
		FString& OutErrorCode,
		FString& OutErrorMessage);

	static bool JsonValueToImportText(
		const TSharedPtr<FJsonValue>& Value,
		FString& OutText,
		FString& OutSummary,
		FString& OutActualType,
		FString& OutError);

	static bool ValidateClassDefaultSetting(
		UObject* CDO,
		const FBlueprintHelperClassDefaultPropertySetting& Setting,
		FBlueprintHelperInvalidClassDefaultSetting& OutInvalid);

	static FBlueprintHelperValidationSummary MakeValidation(bool bShouldCompile, bool bShouldSave);
	static FBlueprintHelperToolError MakeError(
		const FString& Code,
		EBlueprintHelperToolStage Stage,
		const FString& Message,
		const FString& Field = FString());

	const FBlueprintHelperGraphResolver& Resolver;
};
