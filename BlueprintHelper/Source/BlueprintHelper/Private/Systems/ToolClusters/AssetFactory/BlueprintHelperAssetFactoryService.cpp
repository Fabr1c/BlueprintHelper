// BlueprintHelper Service Layer 。Asset Factory 服务实现

#include "Systems/ToolClusters/AssetFactory/BlueprintHelperAssetFactoryService.h"
#include "Shared/AssetFactory/BlueprintHelperAssetFactoryTypes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Shared/BlueprintHelperUserDefinedStructVersionCompat.h"
#include "Factories/BlueprintFactory.h"
#include "Factories/DataAssetFactory.h"
#include "Factories/DataTableFactory.h"
#include "Factories/MaterialFactoryNew.h"
#include "Kismet2/StructureEditorUtils.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/Material.h"
#include "Misc/PackageName.h"
#include "PackageTools.h"
#include "UObject/SavePackage.h"
#include "UObject/Interface.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"

FBlueprintHelperAssetFactoryService::FBlueprintHelperAssetFactoryService() = default;

class FBlueprintHelperAssetFactoryServiceLocalUtils
{
public:
	static FString BlueprintHelperAssetPackageName(const FString& AssetPath)
	{
		int32 DotIndex = INDEX_NONE;
		if (AssetPath.FindChar(TEXT('.'), DotIndex))
		{
			return AssetPath.Left(DotIndex);
		}
		return AssetPath;
	}

	static FString BlueprintHelperAssetObjectPath(const FString& AssetPath)
	{
		const FString PackageName = BlueprintHelperAssetPackageName(AssetPath);
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
		return FString::Printf(TEXT("%s.%s"), *PackageName, *AssetName);
	}

	static UClass* ResolveAssetFactoryClass(const FString& ClassText, UClass* DefaultClass, UClass* RequiredParentClass)
	{
		const FString TrimmedClassText = ClassText.TrimStartAndEnd();
		UClass* ResolvedClass = TrimmedClassText.IsEmpty() ? DefaultClass : nullptr;
		if (!TrimmedClassText.IsEmpty())
		{
			UClass* FoundClass = nullptr;
			const bool bScriptClassPath = TrimmedClassText.StartsWith(TEXT("/Script/"));
			const bool bContentPath = TrimmedClassText.StartsWith(TEXT("/")) && !bScriptClassPath;

			TArray<FString> ClassPathCandidates;
			if (bScriptClassPath)
			{
				ClassPathCandidates.Add(TrimmedClassText);
			}
			else if (bContentPath)
			{
				if (TrimmedClassText.EndsWith(TEXT("_C")))
				{
					ClassPathCandidates.Add(TrimmedClassText);
				}
				else if (TrimmedClassText.Contains(TEXT(".")))
				{
					ClassPathCandidates.Add(FString::Printf(TEXT("%s_C"), *TrimmedClassText));
				}
				else
				{
					ClassPathCandidates.Add(FString::Printf(
						TEXT("%s_C"),
						*BlueprintHelperAssetObjectPath(TrimmedClassText)));
				}
			}
			else
			{
				ClassPathCandidates.Add(TrimmedClassText);
				if (!TrimmedClassText.Contains(TEXT(".")))
				{
					ClassPathCandidates.Add(FString::Printf(TEXT("/Script/Engine.%s"), *TrimmedClassText));
					ClassPathCandidates.Add(FString::Printf(TEXT("/Script/UMG.%s"), *TrimmedClassText));
				}
			}

			for (const FString& ClassPathCandidate : ClassPathCandidates)
			{
				FoundClass = FindObject<UClass>(nullptr, *ClassPathCandidate);
				if (!FoundClass)
				{
					FoundClass = LoadObject<UClass>(nullptr, *ClassPathCandidate);
				}
				if (FoundClass)
				{
					break;
				}
			}

			if (!FoundClass && bContentPath)
			{
				TArray<FString> BlueprintPathCandidates;
				BlueprintPathCandidates.Add(TrimmedClassText);
				if (!TrimmedClassText.Contains(TEXT(".")))
				{
					BlueprintPathCandidates.Add(BlueprintHelperAssetObjectPath(TrimmedClassText));
				}

				for (const FString& BlueprintPathCandidate : BlueprintPathCandidates)
				{
					UBlueprint* BlueprintAsset = FindObject<UBlueprint>(nullptr, *BlueprintPathCandidate);
					if (!BlueprintAsset)
					{
						BlueprintAsset = LoadObject<UBlueprint>(nullptr, *BlueprintPathCandidate);
					}
					if (BlueprintAsset && BlueprintAsset->GeneratedClass)
					{
						FoundClass = BlueprintAsset->GeneratedClass.Get();
						break;
					}
				}
			}

			if (!FoundClass && !bContentPath)
			{
				FoundClass = UClass::TryFindTypeSlow<UClass>(TrimmedClassText);
			}

			if (FoundClass)
			{
				ResolvedClass = FoundClass;
			}
		}

		if (ResolvedClass && RequiredParentClass && !ResolvedClass->IsChildOf(RequiredParentClass))
		{
			return nullptr;
		}

		return ResolvedClass;
	}

	static UScriptStruct* ResolveAssetFactoryRowStruct(const FString& RowStructText)
	{
		const FString TrimmedRowStruct = RowStructText.TrimStartAndEnd();
		if (TrimmedRowStruct.IsEmpty())
		{
			return nullptr;
		}

		UScriptStruct* RowStruct = FindObject<UScriptStruct>(nullptr, *TrimmedRowStruct);
		if (!RowStruct)
		{
			RowStruct = LoadObject<UScriptStruct>(nullptr, *TrimmedRowStruct);
		}
		if (!RowStruct && !TrimmedRowStruct.Contains(TEXT(".")))
		{
			const FString ObjectPath = BlueprintHelperAssetObjectPath(TrimmedRowStruct);
			RowStruct = FindObject<UScriptStruct>(nullptr, *ObjectPath);
			if (!RowStruct)
			{
				RowStruct = LoadObject<UScriptStruct>(nullptr, *ObjectPath);
			}
		}
		if (!RowStruct)
		{
			RowStruct = UClass::TryFindTypeSlow<UScriptStruct>(TrimmedRowStruct);
		}

		return RowStruct;
	}

	static UClass* ResolveExistingAssetClass(const FString& AssetPath)
	{
		const FString ObjectPath = BlueprintHelperAssetObjectPath(AssetPath);
		UObject* ExistingObject = FindObject<UObject>(nullptr, *ObjectPath);
		if (!ExistingObject)
		{
			ExistingObject = LoadObject<UObject>(nullptr, *ObjectPath);
		}
		if (ExistingObject)
		{
			return ExistingObject->GetClass();
		}

		FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		FAssetData AssetData = AssetRegistry.Get().GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
		if (!AssetData.IsValid())
		{
			return nullptr;
		}

		const FString ClassPath = AssetData.AssetClassPath.ToString();
		UClass* ExistingClass = FindObject<UClass>(nullptr, *ClassPath);
		if (!ExistingClass)
		{
			ExistingClass = LoadObject<UClass>(nullptr, *ClassPath);
		}
		if (!ExistingClass)
		{
			ExistingClass = UClass::TryFindTypeSlow<UClass>(AssetData.AssetClassPath.GetAssetName().ToString());
		}

		return ExistingClass;
	}

	static bool DoesExistingAssetMatchRequestedType(
		const FString& AssetPath,
		EBlueprintHelperAssetType AssetType,
		const FString& DataAssetClass,
		const FString& ExpectedAssetClassName)
	{
		FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		const FAssetData AssetData = AssetRegistry.Get().GetAssetByObjectPath(FSoftObjectPath(BlueprintHelperAssetObjectPath(AssetPath)));
		if (!AssetData.IsValid())
		{
			return false;
		}

		if (AssetType == EBlueprintHelperAssetType::DataAsset)
		{
			UClass* ExistingClass = ResolveExistingAssetClass(AssetPath);
			UClass* RequestedClass = ResolveAssetFactoryClass(DataAssetClass, UDataAsset::StaticClass(), UDataAsset::StaticClass());
			return ExistingClass && RequestedClass && ExistingClass->IsChildOf(RequestedClass);
		}

		return AssetData.AssetClassPath.GetAssetName().ToString().Equals(ExpectedAssetClassName, ESearchCase::IgnoreCase);
	}

	static bool TryMakeStructFieldPinType(const FString& TypeText, FEdGraphPinType& OutPinType)
	{
		const FString Key = TypeText.TrimStartAndEnd().ToLower();
		if (Key == TEXT("int") || Key == TEXT("integer"))
		{
			OutPinType = FEdGraphPinType(UEdGraphSchema_K2::PC_Int, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType());
			return true;
		}
		if (Key == TEXT("float") || Key == TEXT("double") || Key == TEXT("real"))
		{
			OutPinType = FEdGraphPinType(UEdGraphSchema_K2::PC_Real, UEdGraphSchema_K2::PC_Float, nullptr, EPinContainerType::None, false, FEdGraphTerminalType());
			return true;
		}
		if (Key == TEXT("bool") || Key == TEXT("boolean"))
		{
			OutPinType = FEdGraphPinType(UEdGraphSchema_K2::PC_Boolean, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType());
			return true;
		}
		if (Key == TEXT("string"))
		{
			OutPinType = FEdGraphPinType(UEdGraphSchema_K2::PC_String, NAME_None, nullptr, EPinContainerType::None, false, FEdGraphTerminalType());
			return true;
		}

		return false;
	}

	static bool ShouldSkipStructDefaultValue(const FEdGraphPinType& PinType, const FString& DefaultValue)
	{
		const FString TrimmedValue = DefaultValue.TrimStartAndEnd();
		if (PinType.PinCategory == UEdGraphSchema_K2::PC_String)
		{
			return TrimmedValue.IsEmpty();
		}
		if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
		{
			return TrimmedValue == TEXT("0");
		}
		if (PinType.PinCategory == UEdGraphSchema_K2::PC_Real)
		{
			return TrimmedValue == TEXT("0") || TrimmedValue == TEXT("0.0") || TrimmedValue == TEXT("0.000000");
		}
		if (PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
		{
			return TrimmedValue.Equals(TEXT("false"), ESearchCase::IgnoreCase) || TrimmedValue == TEXT("0");
		}
		return false;
	}

	static void DiscardFailedUserDefinedStruct(UUserDefinedStruct* Struct, UPackage* Package)
	{
		if (!Struct)
		{
			return;
		}

		Struct->ClearFlags(RF_Public | RF_Standalone);
		const FName DiscardName = MakeUniqueObjectName(
			GetTransientPackage(),
			UUserDefinedStruct::StaticClass(),
			FName(*FString::Printf(TEXT("%s_Failed"), *Struct->GetName())));
		Struct->Rename(*DiscardName.ToString(), GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
		Struct->MarkAsGarbage();

		if (Package)
		{
			Package->ClearDirtyFlag();
		}
	}

	static bool ApplyUserDefinedStructFields(UUserDefinedStruct* Struct, const TArray<FBlueprintHelperAssetFactoryFieldSpec>& Fields)
	{
		if (!Struct || Fields.Num() == 0)
		{
			return Struct != nullptr;
		}

		for (int32 FieldIndex = 0; FieldIndex < Fields.Num(); ++FieldIndex)
		{
			const FBlueprintHelperAssetFactoryFieldSpec& Field = Fields[FieldIndex];
			if (Field.Name.TrimStartAndEnd().IsEmpty())
			{
				return false;
			}

			FEdGraphPinType PinType;
			if (!TryMakeStructFieldPinType(Field.Type, PinType))
			{
				return false;
			}

			TArray<FStructVariableDescription>& Descriptions = FStructureEditorUtils::GetVarDesc(Struct);
			FGuid VarGuid;
			if (FieldIndex == 0 && Descriptions.Num() > 0)
			{
				VarGuid = Descriptions[0].VarGuid;
			}
			else
			{
				if (!FStructureEditorUtils::AddVariable(Struct, PinType))
				{
					return false;
				}
				VarGuid = FStructureEditorUtils::GetVarDesc(Struct).Last().VarGuid;
			}

			if (!FStructureEditorUtils::RenameVariable(Struct, VarGuid, Field.Name))
			{
				return false;
			}

			FStructVariableDescription* VarDesc = FStructureEditorUtils::GetVarDescByGuid(Struct, VarGuid);
			if (!VarDesc)
			{
				return false;
			}
			if (VarDesc->ToPinType() != PinType && !FStructureEditorUtils::ChangeVariableType(Struct, VarGuid, PinType))
			{
				return false;
			}
			if (Field.bHasDefaultValue &&
				!ShouldSkipStructDefaultValue(PinType, Field.DefaultValue) &&
				!FStructureEditorUtils::ChangeVariableDefaultValue(Struct, VarGuid, Field.DefaultValue))
			{
				return false;
			}
		}

		FStructureEditorUtils::CompileStructure(Struct);
		return Struct->Status == UDSS_UpToDate;
	}

};

FBlueprintHelperAssetFactoryData FBlueprintHelperAssetFactoryService::CreateAsset(
	const FString& AssetPath,
	EBlueprintHelperAssetType AssetType,
	const FString& ParentClass,
	const FString& ValueType,
	EBlueprintHelperAssetCollisionPolicy CollisionPolicy,
	bool bDryRun) const
{
	return CreateAsset(
		AssetPath,
		AssetType,
		ParentClass,
		ValueType,
		TEXT(""),
		TEXT(""),
		TArray<FBlueprintHelperAssetFactoryFieldSpec>(),
		CollisionPolicy,
		bDryRun);
}

FBlueprintHelperAssetFactoryData FBlueprintHelperAssetFactoryService::CreateAsset(
	const FString& AssetPath,
	EBlueprintHelperAssetType AssetType,
	const FString& ParentClass,
	const FString& ValueType,
	const FString& RowStruct,
	const FString& DataAssetClass,
	const TArray<FBlueprintHelperAssetFactoryFieldSpec>& Fields,
	EBlueprintHelperAssetCollisionPolicy CollisionPolicy,
	bool bDryRun) const
{
	FBlueprintHelperAssetFactoryData Data;
	Data.Factory.AssetType = AssetType;
	Data.Factory.FactoryType = AssetTypeToFactoryType(AssetType);
	Data.Factory.ParentClass = (AssetType == EBlueprintHelperAssetType::WidgetBlueprint && ParentClass.TrimStartAndEnd().IsEmpty())
		? TEXT("UserWidget")
		: ParentClass;
	Data.Factory.ValueType = ValueType;
	Data.Factory.RowStruct = RowStruct;
	Data.Factory.DataAssetClass = DataAssetClass;
	Data.Factory.Fields = Fields;
	Data.Asset.AssetPath = AssetPath;
	Data.Asset.AssetClass = AssetTypeToAssetClass(AssetType);
	Data.Collision.Policy = CollisionPolicy;

	// ─── 冲突检。───
	const bool bExists = AssetExists(AssetPath);
	Data.Asset.bAlreadyExisted = bExists;

	if (bExists)
	{
		if (CollisionPolicy == EBlueprintHelperAssetCollisionPolicy::ReuseIfExists)
		{
			// 检查同类型
			if (FBlueprintHelperAssetFactoryServiceLocalUtils::DoesExistingAssetMatchRequestedType(
				AssetPath,
				AssetType,
				DataAssetClass,
				Data.Asset.AssetClass))
			{
				Data.Collision.bHandled = true;
				Data.Collision.ExistingAssetPath = AssetPath;
				Data.Asset.bCreated = false;
				return Data;
			}
			// 类型不匹配
			Data.Asset.bCreated = false;
			return Data; // 调用方检。asset_type_mismatch
		}
		// FailIfExists：不创建
		Data.Asset.bCreated = false;
		Data.Collision.bHandled = false;
		return Data;
	}

	if (bDryRun)
	{
		Data.Asset.bCreated = false;
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
		bCreated = CreateStructure(AssetPath, Fields);
		break;
	case EBlueprintHelperAssetType::InputAction:
		bCreated = CreateInputAction(AssetPath, ValueType.IsEmpty() ? TEXT("bool") : ValueType);
		break;
	case EBlueprintHelperAssetType::InputMappingContext:
		bCreated = CreateInputMappingContext(AssetPath);
		break;
	case EBlueprintHelperAssetType::DataAsset:
		bCreated = CreateDataAsset(AssetPath, DataAssetClass);
		break;
	case EBlueprintHelperAssetType::DataTable:
		bCreated = CreateDataTable(AssetPath, RowStruct);
		break;
	case EBlueprintHelperAssetType::WidgetBlueprint:
		bCreated = CreateWidgetBlueprint(AssetPath, Data.Factory.ParentClass);
		break;
	case EBlueprintHelperAssetType::Material:
		bCreated = CreateMaterial(AssetPath);
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

bool FBlueprintHelperAssetFactoryService::TryNormalizeAssetTypeAndParent(
	const FString& AssetTypeText,
	FString& InOutParentClass,
	EBlueprintHelperAssetType& OutAssetType)
{
	const FString Key = AssetTypeText.TrimStartAndEnd().ToLower();
	if (Key == TEXT("blueprint") || Key == TEXT("blueprint_class") || Key == TEXT("blueprintclass") || Key == TEXT("actor"))
	{
		OutAssetType = EBlueprintHelperAssetType::BlueprintClass;
		if (InOutParentClass.TrimStartAndEnd().IsEmpty())
		{
			InOutParentClass = TEXT("Actor");
		}
		return true;
	}
	if (Key == TEXT("blueprint_interface") || Key == TEXT("blueprintinterface"))
	{
		OutAssetType = EBlueprintHelperAssetType::BlueprintInterface;
		InOutParentClass.Empty();
		return true;
	}
	if (Key == TEXT("structure"))
	{
		OutAssetType = EBlueprintHelperAssetType::Structure;
		return true;
	}
	if (Key == TEXT("input_action") || Key == TEXT("inputaction"))
	{
		OutAssetType = EBlueprintHelperAssetType::InputAction;
		return true;
	}
	if (Key == TEXT("input_mapping_context") || Key == TEXT("inputmappingcontext"))
	{
		OutAssetType = EBlueprintHelperAssetType::InputMappingContext;
		return true;
	}
	if (Key == TEXT("data_asset") || Key == TEXT("dataasset"))
	{
		OutAssetType = EBlueprintHelperAssetType::DataAsset;
		return true;
	}
	if (Key == TEXT("data_table") || Key == TEXT("datatable"))
	{
		OutAssetType = EBlueprintHelperAssetType::DataTable;
		return true;
	}
	if (Key == TEXT("widget_blueprint") || Key == TEXT("widgetblueprint") || Key == TEXT("widget"))
	{
		OutAssetType = EBlueprintHelperAssetType::WidgetBlueprint;
		if (InOutParentClass.TrimStartAndEnd().IsEmpty())
		{
			InOutParentClass = TEXT("UserWidget");
		}
		return true;
	}
	if (Key == TEXT("material") || Key == TEXT("mat"))
	{
		OutAssetType = EBlueprintHelperAssetType::Material;
		InOutParentClass.Empty();
		return true;
	}

	OutAssetType = EBlueprintHelperAssetType::Unknown;
	return false;
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
	case EBlueprintHelperAssetType::Material:             return EBlueprintHelperFactoryType::Material;
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
	case EBlueprintHelperAssetType::Material:             return TEXT("Material");
	default:                                              return TEXT("Unknown");
	}
}

bool FBlueprintHelperAssetFactoryService::AssetExists(const FString& AssetPath)
{
	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FAssetData AssetData = AssetRegistry.Get().GetAssetByObjectPath(FSoftObjectPath(FBlueprintHelperAssetFactoryServiceLocalUtils::BlueprintHelperAssetObjectPath(AssetPath)));
	return AssetData.IsValid();
}

FString FBlueprintHelperAssetFactoryService::GetExistingAssetClass(const FString& AssetPath)
{
	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FAssetData AssetData = AssetRegistry.Get().GetAssetByObjectPath(FSoftObjectPath(FBlueprintHelperAssetFactoryServiceLocalUtils::BlueprintHelperAssetObjectPath(AssetPath)));
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

	const FString PackageName = FBlueprintHelperAssetFactoryServiceLocalUtils::BlueprintHelperAssetPackageName(AssetPath);
	const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package) return false;

	UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
	Factory->ParentClass = ParentUClass;

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

bool FBlueprintHelperAssetFactoryService::CreateBlueprintInterface(const FString& AssetPath)
{
	const FString PackageName = FBlueprintHelperAssetFactoryServiceLocalUtils::BlueprintHelperAssetPackageName(AssetPath);
	const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package) return false;

	UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
	Factory->BlueprintType = BPTYPE_Interface;
	Factory->ParentClass = UInterface::StaticClass();

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

bool FBlueprintHelperAssetFactoryService::CreateStructure(
	const FString& AssetPath,
	const TArray<FBlueprintHelperAssetFactoryFieldSpec>& Fields)
{
	const FString PackageName = FBlueprintHelperAssetFactoryServiceLocalUtils::BlueprintHelperAssetPackageName(AssetPath);
	const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package) return false;

	// 通过类名查找 UUserDefinedStruct
	UUserDefinedStruct* NewStruct = FStructureEditorUtils::CreateUserDefinedStruct(
		Package,
		FName(*AssetName),
		RF_Public | RF_Standalone);
	if (!NewStruct) return false;

	if (!FBlueprintHelperAssetFactoryServiceLocalUtils::ApplyUserDefinedStructFields(NewStruct, Fields))
	{
		FBlueprintHelperAssetFactoryServiceLocalUtils::DiscardFailedUserDefinedStruct(NewStruct, Package);
		return false;
	}

	FAssetRegistryModule::AssetCreated(NewStruct);
	Package->MarkPackageDirty();
	return true;
}

bool FBlueprintHelperAssetFactoryService::CreateInputAction(const FString& AssetPath, const FString& ValueType)
{
	const FString PackageName = FBlueprintHelperAssetFactoryServiceLocalUtils::BlueprintHelperAssetPackageName(AssetPath);
	const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);

	UPackage* Package = CreatePackage(*PackageName);
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
	const FString PackageName = FBlueprintHelperAssetFactoryServiceLocalUtils::BlueprintHelperAssetPackageName(AssetPath);
	const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package) return false;

	UInputMappingContext* NewContext = NewObject<UInputMappingContext>(Package, *AssetName, RF_Public | RF_Standalone);

	FAssetRegistryModule::AssetCreated(NewContext);
	Package->MarkPackageDirty();
	return true;
}

bool FBlueprintHelperAssetFactoryService::CreateDataAsset(const FString& AssetPath, const FString& AssetClass)
{
	const FString PackageName = FBlueprintHelperAssetFactoryServiceLocalUtils::BlueprintHelperAssetPackageName(AssetPath);
	const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);

	UClass* Class = FBlueprintHelperAssetFactoryServiceLocalUtils::ResolveAssetFactoryClass(AssetClass, UDataAsset::StaticClass(), UDataAsset::StaticClass());
	if (!Class) return false;
	if (Class->HasAnyClassFlags(CLASS_Abstract)) return false;

	UPackage* Package = CreatePackage(*PackageName);
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

bool FBlueprintHelperAssetFactoryService::CreateDataTable(const FString& AssetPath, const FString& RowStruct)
{
	UScriptStruct* ResolvedRowStruct = FBlueprintHelperAssetFactoryServiceLocalUtils::ResolveAssetFactoryRowStruct(RowStruct);
	if (!ResolvedRowStruct)
	{
		return false;
	}

	const FString PackageName = FBlueprintHelperAssetFactoryServiceLocalUtils::BlueprintHelperAssetPackageName(AssetPath);
	const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package) return false;

	UDataTableFactory* Factory = NewObject<UDataTableFactory>();
	Factory->Struct = ResolvedRowStruct;

	UObject* NewAsset = Factory->FactoryCreateNew(
		UDataTable::StaticClass(), Package, *AssetName,
		RF_Public | RF_Standalone, nullptr, GWarn);

	if (NewAsset)
	{
		FAssetRegistryModule::AssetCreated(NewAsset);
		Package->MarkPackageDirty();
		return true;
	}

	return false;
}

bool FBlueprintHelperAssetFactoryService::CreateWidgetBlueprint(const FString& AssetPath, const FString& ParentClass)
{
	UClass* ParentUClass = FBlueprintHelperAssetFactoryServiceLocalUtils::ResolveAssetFactoryClass(ParentClass, UUserWidget::StaticClass(), UUserWidget::StaticClass());
	if (!ParentUClass) return false;

	const FString PackageName = FBlueprintHelperAssetFactoryServiceLocalUtils::BlueprintHelperAssetPackageName(AssetPath);
	const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package) return false;

	UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
	Factory->BlueprintType = BPTYPE_Normal;
	Factory->ParentClass = ParentUClass;

	UObject* NewAsset = Factory->FactoryCreateNew(
		UWidgetBlueprint::StaticClass(), Package, *AssetName,
		RF_Public | RF_Standalone, nullptr, GWarn);

	if (NewAsset)
	{
		FAssetRegistryModule::AssetCreated(NewAsset);
		Package->MarkPackageDirty();
		return true;
	}

	return false;
}

bool FBlueprintHelperAssetFactoryService::CreateMaterial(const FString& AssetPath)
{
	const FString PackageName = FBlueprintHelperAssetFactoryServiceLocalUtils::BlueprintHelperAssetPackageName(AssetPath);
	const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package) return false;

	UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
	UObject* NewAsset = Factory->FactoryCreateNew(
		UMaterial::StaticClass(),
		Package,
		*AssetName,
		RF_Public | RF_Standalone,
		nullptr,
		GWarn);

	if (NewAsset)
	{
		FAssetRegistryModule::AssetCreated(NewAsset);
		Package->MarkPackageDirty();
		return true;
	}

	return false;
}
