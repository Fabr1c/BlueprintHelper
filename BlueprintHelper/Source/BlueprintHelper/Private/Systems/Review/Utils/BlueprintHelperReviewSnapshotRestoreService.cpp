// BlueprintHelper Review BlueprintHelperReviewSnapshotRestoreService implementation.

#include "Systems/Review/Utils/BlueprintHelperReviewSnapshotRestoreService.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ActorComponent.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "DataTableEditorUtils.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphUtilities.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "HAL/FileManager.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/StructureEditorUtils.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Shared/Review/BlueprintHelperReviewStatusUtils.h"
#include "Shared/Review/BlueprintHelperReviewTargetKindRegistry.h"
#include "Systems/Debug/BlueprintHelperDebugCaseStoreService.h"
#include "Systems/Debug/BlueprintHelperDebugEntryService.h"
#include "Systems/Review/BlueprintHelperReviewHashService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Systems/ToolClusters/CleanupOwnership/BlueprintHelperConvertBlockToUserOwnedService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/ToolClusters/ObjectProperty/BlueprintHelperPropertyReflectionService.h"
#include "Systems/Transactions/BlueprintHelperTransactionJournalService.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "WidgetBlueprint.h"
#include "Systems/Review/Utils/BlueprintHelperReviewActionRecordUtils.h"

bool FBlueprintHelperReviewSnapshotRestoreService::IsAssetFactoryTarget(const FBlueprintHelperReviewAtomicTarget& Target)
	{
		return FBlueprintHelperReviewTargetKindRegistry::IsAssetFactoryTargetKind(Target.TargetKind)
			|| Target.TargetKey.StartsWith(TEXT("asset_factory:"), ESearchCase::IgnoreCase);
	}
FString FBlueprintHelperReviewSnapshotRestoreService::ExtractTargetName(const FBlueprintHelperReviewAtomicTarget& Target)
	{
		if (!Target.PropertyPath.IsEmpty())
		{
			return Target.PropertyPath;
		}
		if (!Target.ComponentPath.IsEmpty())
		{
			return Target.ComponentPath;
		}

		int32 LastColon = INDEX_NONE;
		if (Target.TargetKey.FindLastChar(TEXT(':'), LastColon))
		{
			return Target.TargetKey.Mid(LastColon + 1);
		}
		return Target.DisplayLabel;
	}
void FBlueprintHelperReviewSnapshotRestoreService::SplitWidgetPropertyTarget(
		const FString& TargetName,
		FString& OutWidgetName,
		FString& OutPropertyName)
	{
		OutWidgetName = TargetName;
		OutPropertyName.Reset();

		FString Left;
		FString Right;
		if (TargetName.Split(TEXT("."), &Left, &Right) && !Left.IsEmpty())
		{
			OutWidgetName = Left;
			OutPropertyName = Right;
		}
	}
bool FBlueprintHelperReviewSnapshotRestoreService::ParseReviewSnapshotJson(
		const FString& SnapshotJson,
		TSharedPtr<FJsonObject>& OutSnapshot,
		FString& OutError)
	{
		if (SnapshotJson.IsEmpty())
		{
			OutError = TEXT("missing_before_snapshot_json");
			return false;
		}

		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SnapshotJson);
		if (!FJsonSerializer::Deserialize(Reader, OutSnapshot) || !OutSnapshot.IsValid())
		{
			OutError = TEXT("before_snapshot_json_parse_failed");
			return false;
		}
		return true;
	}
int32 FBlueprintHelperReviewSnapshotRestoreService::FindBlueprintVariableIndex(UBlueprint* Blueprint, const FName VariableName)
	{
		if (!Blueprint)
		{
			return INDEX_NONE;
		}

		for (int32 Index = 0; Index < Blueprint->NewVariables.Num(); ++Index)
		{
			if (Blueprint->NewVariables[Index].VarName == VariableName)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}
USCS_Node* FBlueprintHelperReviewSnapshotRestoreService::FindScsNodeByName(UBlueprint* Blueprint, const FString& ComponentName)
	{
		if (!Blueprint || !Blueprint->SimpleConstructionScript)
		{
			return nullptr;
		}

		for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (Node && Node->GetVariableName().ToString() == ComponentName)
			{
				return Node;
			}
		}
		return nullptr;
	}
void FBlueprintHelperReviewSnapshotRestoreService::MarkBlueprintReviewRestoreModified(UBlueprint* Blueprint)
	{
		if (!Blueprint)
		{
			return;
		}

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		if (Blueprint->GetOutermost())
		{
			Blueprint->GetOutermost()->MarkPackageDirty();
		}
	}
bool FBlueprintHelperReviewSnapshotRestoreService::RestoreBlueprintVariableFromSnapshot(
		const FBlueprintHelperReviewAtomicTarget& Target,
		const TSharedPtr<FJsonObject>& Snapshot,
		FString& OutError)
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *Target.AssetPath));
		if (!Blueprint)
		{
			OutError = FString::Printf(TEXT("blueprint_not_found:%s"), *Target.AssetPath);
			return false;
		}

		const FString VariableName = ExtractTargetName(Target);
		if (VariableName.IsEmpty())
		{
			OutError = TEXT("missing_variable_name");
			return false;
		}

		bool bSnapshotExists = false;
		Snapshot->TryGetBoolField(TEXT("exists"), bSnapshotExists);
		const FName VariableFName(*VariableName);
		const int32 VariableIndex = FindBlueprintVariableIndex(Blueprint, VariableFName);

		const FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Review Restore Variable")));
		Blueprint->Modify();
		if (!bSnapshotExists)
		{
			if (VariableIndex != INDEX_NONE)
			{
				FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, VariableFName);
				MarkBlueprintReviewRestoreModified(Blueprint);
			}
			return true;
		}

		if (VariableIndex == INDEX_NONE)
		{
			FString PinCategory;
			FString PinSubCategory;
			FString PinSubCategoryObjectPath;
			Snapshot->TryGetStringField(TEXT("pin_category"), PinCategory);
			Snapshot->TryGetStringField(TEXT("pin_sub_category"), PinSubCategory);
			Snapshot->TryGetStringField(TEXT("pin_sub_category_object"), PinSubCategoryObjectPath);
			if (PinCategory.IsEmpty())
			{
				OutError = FString::Printf(TEXT("snapshot_restore_variable_missing_type:%s"), *VariableName);
				return false;
			}

			FEdGraphPinType PinType;
			PinType.PinCategory = FName(*PinCategory);
			PinType.PinSubCategory = FName(*PinSubCategory);
			if (!PinSubCategoryObjectPath.IsEmpty())
			{
				PinType.PinSubCategoryObject = LoadObject<UObject>(nullptr, *PinSubCategoryObjectPath);
			}

			if (!FBlueprintEditorUtils::AddMemberVariable(Blueprint, VariableFName, PinType))
			{
				OutError = FString::Printf(TEXT("snapshot_restore_variable_recreate_failed:%s"), *VariableName);
				return false;
			}

			const int32 NewVariableIndex = FindBlueprintVariableIndex(Blueprint, VariableFName);
			if (NewVariableIndex == INDEX_NONE)
			{
				OutError = FString::Printf(TEXT("snapshot_restore_variable_recreate_missing:%s"), *VariableName);
				return false;
			}

			FString Category;
			if (Snapshot->TryGetStringField(TEXT("category"), Category))
			{
				Blueprint->NewVariables[NewVariableIndex].Category = FText::FromString(Category);
			}

			FString GuidString;
			FGuid ParsedGuid;
			if (Snapshot->TryGetStringField(TEXT("guid"), GuidString) && FGuid::Parse(GuidString, ParsedGuid))
			{
				Blueprint->NewVariables[NewVariableIndex].VarGuid = ParsedGuid;
			}

			FString DefaultValue;
			if (Snapshot->TryGetStringField(TEXT("default_value"), DefaultValue))
			{
				Blueprint->NewVariables[NewVariableIndex].DefaultValue = DefaultValue;
			}
			MarkBlueprintReviewRestoreModified(Blueprint);
			return true;
		}

		FString DefaultValue;
		if (Snapshot->TryGetStringField(TEXT("default_value"), DefaultValue))
		{
			Blueprint->NewVariables[VariableIndex].DefaultValue = DefaultValue;
		}
		MarkBlueprintReviewRestoreModified(Blueprint);
		return true;
	}
bool FBlueprintHelperReviewSnapshotRestoreService::RestoreComponentPropertiesFromSnapshot(
		UObject* ComponentTemplate,
		const TSharedPtr<FJsonObject>& Snapshot,
		FString& OutError)
	{
		if (!ComponentTemplate)
		{
			OutError = TEXT("missing_component_template");
			return false;
		}

		const TSharedPtr<FJsonObject>* PropertiesObject = nullptr;
		if (!Snapshot->TryGetObjectField(TEXT("properties"), PropertiesObject) || !PropertiesObject || !PropertiesObject->IsValid())
		{
			return true;
		}

		const TArray<TSharedPtr<FJsonValue>>* PropertiesArray = nullptr;
		if (!(*PropertiesObject)->TryGetArrayField(TEXT("properties"), PropertiesArray) || !PropertiesArray)
		{
			return true;
		}

		for (const TSharedPtr<FJsonValue>& PropertyValue : *PropertiesArray)
		{
			const TSharedPtr<FJsonObject> PropertyJson = PropertyValue.IsValid() ? PropertyValue->AsObject() : nullptr;
			if (!PropertyJson.IsValid())
			{
				continue;
			}

			FString PropertyName;
			FString ValueText;
			if (!PropertyJson->TryGetStringField(TEXT("name"), PropertyName) ||
				!PropertyJson->TryGetStringField(TEXT("value"), ValueText) ||
				PropertyName.IsEmpty())
			{
				continue;
			}

			FProperty* Property = ComponentTemplate->GetClass()
				? ComponentTemplate->GetClass()->FindPropertyByName(FName(*PropertyName))
				: nullptr;
			if (!Property)
			{
				continue;
			}

			void* ValuePtr = Property->ContainerPtrToValuePtr<void>(ComponentTemplate);
			if (!Property->ImportText_Direct(*ValueText, ValuePtr, ComponentTemplate, PPF_None))
			{
				OutError = FString::Printf(TEXT("component_property_restore_failed:%s"), *PropertyName);
				return false;
			}
		}

		return true;
	}
bool FBlueprintHelperReviewSnapshotRestoreService::RestoreComponentFromSnapshot(
		const FBlueprintHelperReviewAtomicTarget& Target,
		const TSharedPtr<FJsonObject>& Snapshot,
		FString& OutError)
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *Target.AssetPath));
		if (!Blueprint || !Blueprint->SimpleConstructionScript)
		{
			OutError = FString::Printf(TEXT("blueprint_not_found:%s"), *Target.AssetPath);
			return false;
		}

		const FString ComponentName = ExtractTargetName(Target);
		if (ComponentName.IsEmpty())
		{
			OutError = TEXT("missing_component_name");
			return false;
		}

		bool bSnapshotExists = false;
		Snapshot->TryGetBoolField(TEXT("exists"), bSnapshotExists);
		USCS_Node* Node = FindScsNodeByName(Blueprint, ComponentName);

		const FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Review Restore Component")));
		Blueprint->Modify();
		Blueprint->SimpleConstructionScript->Modify();
		if (!bSnapshotExists)
		{
			if (Node)
			{
				Node->Modify();
				Blueprint->SimpleConstructionScript->RemoveNode(Node);
				MarkBlueprintReviewRestoreModified(Blueprint);
			}
			return true;
		}

		if (!Node || !Node->ComponentTemplate)
		{
			FString ComponentClassPath;
			if (!Snapshot->TryGetStringField(TEXT("component_class"), ComponentClassPath) || ComponentClassPath.IsEmpty())
			{
				OutError = FString::Printf(TEXT("snapshot_restore_component_missing_class:%s"), *ComponentName);
				return false;
			}

			UClass* ComponentClass = LoadObject<UClass>(nullptr, *ComponentClassPath);
			if (!ComponentClass || !ComponentClass->IsChildOf(UActorComponent::StaticClass()))
			{
				OutError = FString::Printf(TEXT("snapshot_restore_component_invalid_class:%s"), *ComponentClassPath);
				return false;
			}

			Node = Blueprint->SimpleConstructionScript->CreateNode(ComponentClass, FName(*ComponentName));
			if (!Node || !Node->ComponentTemplate)
			{
				OutError = FString::Printf(TEXT("snapshot_restore_component_recreate_failed:%s"), *ComponentName);
				return false;
			}

			FString ParentComponentName;
			Snapshot->TryGetStringField(TEXT("parent_component"), ParentComponentName);
			if (!ParentComponentName.IsEmpty())
			{
				if (USCS_Node* ParentNode = FindScsNodeByName(Blueprint, ParentComponentName))
				{
					ParentNode->Modify();
					ParentNode->AddChildNode(Node);
				}
				else
				{
					Blueprint->SimpleConstructionScript->AddNode(Node);
				}
			}
			else
			{
				Blueprint->SimpleConstructionScript->AddNode(Node);
			}
		}

		Node->Modify();
		Node->ComponentTemplate->Modify();
		if (!RestoreComponentPropertiesFromSnapshot(Node->ComponentTemplate, Snapshot, OutError))
		{
			return false;
		}
		MarkBlueprintReviewRestoreModified(Blueprint);
		return true;
	}
UObject* FBlueprintHelperReviewSnapshotRestoreService::LoadReviewTargetAsset(const FString& AssetPath)
	{
		if (AssetPath.IsEmpty())
		{
			return nullptr;
		}

		if (UObject* Asset = FSoftObjectPath(AssetPath).TryLoad())
		{
			return Asset;
		}
		if (!AssetPath.Contains(TEXT(".")) && FPackageName::IsValidLongPackageName(AssetPath))
		{
			const FString ObjectPath = FString::Printf(
				TEXT("%s.%s"),
				*AssetPath,
				*FPackageName::GetShortName(AssetPath));
			return FSoftObjectPath(ObjectPath).TryLoad();
		}
		return nullptr;
	}
bool FBlueprintHelperReviewSnapshotRestoreService::RestoreDataTableRowFromSnapshot(
		const FBlueprintHelperReviewAtomicTarget& Target,
		const TSharedPtr<FJsonObject>& Snapshot,
		FString& OutError)
	{
		UDataTable* DataTable = Cast<UDataTable>(LoadReviewTargetAsset(Target.AssetPath));
		if (!DataTable || !DataTable->GetRowStruct())
		{
			OutError = FString::Printf(TEXT("datatable_not_found:%s"), *Target.AssetPath);
			return false;
		}

		const FString RowName = ExtractTargetName(Target);
		if (RowName.IsEmpty())
		{
			OutError = TEXT("missing_datatable_row_name");
			return false;
		}

		bool bSnapshotExists = false;
		Snapshot->TryGetBoolField(TEXT("exists"), bSnapshotExists);
		const FName RowFName(*RowName);
		uint8* const* RowData = DataTable->GetRowMap().Find(RowFName);

		const FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Review Restore DataTable Row")));
		DataTable->Modify();
		if (!bSnapshotExists)
		{
			if (RowData)
			{
				DataTable->RemoveRow(RowFName);
				DataTable->MarkPackageDirty();
			}
			return true;
		}

		if (!RowData || !*RowData)
		{
			FDataTableEditorUtils::BroadcastPreChange(DataTable, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
			uint8* NewRowData = FDataTableEditorUtils::AddRow(DataTable, RowFName);
			FDataTableEditorUtils::BroadcastPostChange(DataTable, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
			if (!NewRowData)
			{
				OutError = FString::Printf(TEXT("snapshot_restore_datatable_row_recreate_failed:%s"), *RowName);
				return false;
			}
			RowData = DataTable->GetRowMap().Find(RowFName);
			if (!RowData || !*RowData)
			{
				OutError = FString::Printf(TEXT("snapshot_restore_datatable_row_recreate_missing:%s"), *RowName);
				return false;
			}
		}

		FString RowValue;
		if (Snapshot->TryGetStringField(TEXT("value"), RowValue))
		{
			FDataTableEditorUtils::BroadcastPreChange(DataTable, FDataTableEditorUtils::EDataTableChangeInfo::RowData);
			const TCHAR* ImportResult = DataTable->GetRowStruct()->ImportText(
				*RowValue,
				*RowData,
				DataTable,
				PPF_Copy,
				GWarn,
				GetPathNameSafe(DataTable->GetRowStruct()));
			if (!ImportResult)
			{
				FDataTableEditorUtils::BroadcastPostChange(DataTable, FDataTableEditorUtils::EDataTableChangeInfo::RowData);
				OutError = FString::Printf(TEXT("datatable_row_restore_failed:%s"), *RowName);
				return false;
			}
			DataTable->HandleDataTableChanged(RowFName);
			FDataTableEditorUtils::BroadcastPostChange(DataTable, FDataTableEditorUtils::EDataTableChangeInfo::RowData);
			DataTable->MarkPackageDirty();
		}
		return true;
	}
FStructVariableDescription* FBlueprintHelperReviewSnapshotRestoreService::FindStructFieldByReviewName(
		UUserDefinedStruct* Struct,
		const FString& FieldName)
	{
		if (!Struct || FieldName.IsEmpty())
		{
			return nullptr;
		}

		TArray<FStructVariableDescription>& Descriptions = FStructureEditorUtils::GetVarDesc(Struct);
		for (FStructVariableDescription& Description : Descriptions)
		{
			const FString VarName = Description.VarName.ToString();
			const FString FriendlyName = Description.FriendlyName;
			const FString DisplayName = FStructureEditorUtils::GetVariableFriendlyName(Struct, Description.VarGuid);
			if (VarName.Equals(FieldName, ESearchCase::IgnoreCase)
				|| FriendlyName.Equals(FieldName, ESearchCase::IgnoreCase)
				|| DisplayName.Equals(FieldName, ESearchCase::IgnoreCase)
				|| VarName.StartsWith(FieldName + TEXT("_"), ESearchCase::IgnoreCase))
			{
				return &Description;
			}
		}
		return nullptr;
	}
bool FBlueprintHelperReviewSnapshotRestoreService::RestoreStructFieldFromSnapshot(
		const FBlueprintHelperReviewAtomicTarget& Target,
		const TSharedPtr<FJsonObject>& Snapshot,
		FString& OutError)
	{
		UUserDefinedStruct* Struct = Cast<UUserDefinedStruct>(LoadReviewTargetAsset(Target.AssetPath));
		if (!Struct)
		{
			OutError = FString::Printf(TEXT("struct_not_found:%s"), *Target.AssetPath);
			return false;
		}

		const FString FieldName = ExtractTargetName(Target);
		if (FieldName.IsEmpty())
		{
			OutError = TEXT("missing_struct_field_name");
			return false;
		}

		bool bSnapshotExists = false;
		Snapshot->TryGetBoolField(TEXT("exists"), bSnapshotExists);
		FStructVariableDescription* FieldDescription = FindStructFieldByReviewName(Struct, FieldName);

		if (!bSnapshotExists)
		{
			if (FieldDescription)
			{
				const FGuid FieldGuid = FieldDescription->VarGuid;
				Struct->Modify();
				if (!FStructureEditorUtils::RemoveVariable(Struct, FieldGuid))
				{
					OutError = FString::Printf(TEXT("struct_field_remove_failed:%s"), *FieldName);
					return false;
				}
				if (Struct->GetOutermost())
				{
					Struct->GetOutermost()->MarkPackageDirty();
				}
			}
			return true;
		}

		if (!FieldDescription)
		{
			OutError = FString::Printf(TEXT("struct_field_recreate_required:%s"), *FieldName);
			return false;
		}

		FString DefaultValue;
		if (Snapshot->TryGetStringField(TEXT("default_value"), DefaultValue))
		{
			Struct->Modify();
			if (!FStructureEditorUtils::ChangeVariableDefaultValue(Struct, FieldDescription->VarGuid, DefaultValue))
			{
				OutError = FString::Printf(TEXT("struct_field_default_restore_failed:%s"), *FieldName);
				return false;
			}
			if (Struct->GetOutermost())
			{
				Struct->GetOutermost()->MarkPackageDirty();
			}
		}
		return true;
	}
bool FBlueprintHelperReviewSnapshotRestoreService::ParseSnapshotBoolValue(const FString& ValueText, bool& OutValue)
	{
		const FString Normalized = ValueText.TrimStartAndEnd();
		if (Normalized.IsEmpty()
			|| Normalized.Equals(TEXT("false"), ESearchCase::IgnoreCase)
			|| Normalized == TEXT("0"))
		{
			OutValue = false;
			return true;
		}
		if (Normalized.Equals(TEXT("true"), ESearchCase::IgnoreCase)
			|| Normalized == TEXT("1"))
		{
			OutValue = true;
			return true;
		}
		return false;
	}
bool FBlueprintHelperReviewSnapshotRestoreService::RestoreObjectPropertyFromSnapshot(
		const FBlueprintHelperReviewAtomicTarget& Target,
		const TSharedPtr<FJsonObject>& Snapshot,
		FString& OutError)
	{
		UObject* Asset = LoadReviewTargetAsset(Target.AssetPath);
		if (!Asset || !Asset->GetClass())
		{
			OutError = FString::Printf(TEXT("asset_not_found:%s"), *Target.AssetPath);
			return false;
		}

		UBlueprint* Blueprint =
			FBlueprintHelperReviewTargetKindRegistry::IsClassDefaultPropertyTargetKind(Target.TargetKind)
			? Cast<UBlueprint>(Asset)
			: nullptr;
		UObject* PropertyOwner = Asset;
		if (Blueprint)
		{
			UClass* DefaultClass = Blueprint->GeneratedClass
				? Blueprint->GeneratedClass
				: Blueprint->SkeletonGeneratedClass;
			PropertyOwner = DefaultClass ? DefaultClass->GetDefaultObject() : nullptr;
		}
		if (!PropertyOwner || !PropertyOwner->GetClass())
		{
			OutError = FString::Printf(TEXT("property_owner_not_found:%s"), *Target.AssetPath);
			return false;
		}

		const FString PropertyPath = ExtractTargetName(Target);
		if (PropertyPath.IsEmpty())
		{
			OutError = TEXT("missing_property_path");
			return false;
		}

		bool bSnapshotExists = false;
		Snapshot->TryGetBoolField(TEXT("exists"), bSnapshotExists);
		if (!bSnapshotExists)
		{
			return true;
		}

		FProperty* Property = nullptr;
		void* ValuePtr = nullptr;
		FString ExpectedType;
		FString ErrorCode;
		FString ErrorMessage;
		if (!FBlueprintHelperPropertyReflectionService::ResolvePropertyPath(
			PropertyOwner,
			PropertyPath,
			Property,
			ValuePtr,
			ExpectedType,
			ErrorCode,
			ErrorMessage) ||
			!Property ||
			!ValuePtr)
		{
			OutError = FString::Printf(
				TEXT("property_not_found:%s:%s:%s"),
				*PropertyPath,
				*ErrorCode,
				*ErrorMessage);
			return false;
		}

		FString ValueText;
		if (!Snapshot->TryGetStringField(TEXT("value"), ValueText))
		{
			return true;
		}

		const FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Review Restore Object Property")));
		if (Blueprint)
		{
			Blueprint->Modify();
		}
		PropertyOwner->Modify();
		PropertyOwner->PreEditChange(Property);
		if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
		{
			bool bBoolValue = false;
			if (!ParseSnapshotBoolValue(ValueText, bBoolValue))
			{
				OutError = FString::Printf(TEXT("object_property_bool_restore_failed:%s"), *PropertyPath);
				return false;
			}
			BoolProperty->SetPropertyValue(ValuePtr, bBoolValue);
		}
		else if (!Property->ImportText_Direct(*ValueText, ValuePtr, PropertyOwner, PPF_None))
		{
			OutError = FString::Printf(TEXT("object_property_restore_failed:%s"), *PropertyPath);
			return false;
		}
		FPropertyChangedEvent PropertyChangedEvent(Property, EPropertyChangeType::ValueSet);
		PropertyOwner->PostEditChangeProperty(PropertyChangedEvent);
		if (Blueprint)
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		}
		else
		{
			Asset->MarkPackageDirty();
		}
		return true;
	}
bool FBlueprintHelperReviewSnapshotRestoreService::RestoreWidgetFromSnapshot(
		const FBlueprintHelperReviewAtomicTarget& Target,
		const TSharedPtr<FJsonObject>& Snapshot,
		FString& OutError)
	{
		UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(LoadReviewTargetAsset(Target.AssetPath));
		if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
		{
			OutError = FString::Printf(TEXT("widget_blueprint_not_found:%s"), *Target.AssetPath);
			return false;
		}

		FString WidgetName;
		FString PropertyName;
		SplitWidgetPropertyTarget(ExtractTargetName(Target), WidgetName, PropertyName);
		if (WidgetName.IsEmpty())
		{
			OutError = TEXT("missing_widget_name");
			return false;
		}

		bool bSnapshotExists = false;
		Snapshot->TryGetBoolField(TEXT("exists"), bSnapshotExists);
		UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName));

		const FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Review Restore Widget")));
		WidgetBlueprint->Modify();
		WidgetBlueprint->WidgetTree->Modify();
		if (!bSnapshotExists)
		{
			if (Widget)
			{
				Widget->Modify();
				WidgetBlueprint->WidgetTree->RemoveWidget(Widget);
				MarkBlueprintReviewRestoreModified(WidgetBlueprint);
			}
			return true;
		}

		if (!Widget)
		{
			FString WidgetClassPath;
			if (!Snapshot->TryGetStringField(TEXT("widget_class"), WidgetClassPath) || WidgetClassPath.IsEmpty())
			{
				OutError = FString::Printf(TEXT("snapshot_restore_widget_missing_class:%s"), *WidgetName);
				return false;
			}

			UClass* WidgetClass = LoadObject<UClass>(nullptr, *WidgetClassPath);
			if (!WidgetClass || !WidgetClass->IsChildOf(UWidget::StaticClass()))
			{
				OutError = FString::Printf(TEXT("snapshot_restore_widget_invalid_class:%s"), *WidgetClassPath);
				return false;
			}

			Widget = WidgetBlueprint->WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*WidgetName));
			if (!Widget)
			{
				OutError = FString::Printf(TEXT("snapshot_restore_widget_recreate_failed:%s"), *WidgetName);
				return false;
			}

			FString ParentWidgetName;
			Snapshot->TryGetStringField(TEXT("parent_widget"), ParentWidgetName);
			if (!ParentWidgetName.IsEmpty())
			{
				UWidget* ParentWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*ParentWidgetName));
				UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
				if (!ParentPanel)
				{
					OutError = FString::Printf(TEXT("snapshot_restore_widget_parent_not_panel:%s"), *ParentWidgetName);
					return false;
				}

				int32 ChildIndex = INDEX_NONE;
				double ChildIndexNumber = INDEX_NONE;
				if (Snapshot->TryGetNumberField(TEXT("child_index"), ChildIndexNumber))
				{
					ChildIndex = FMath::RoundToInt(ChildIndexNumber);
				}
				UPanelSlot* NewSlot = ChildIndex >= 0 && ChildIndex <= ParentPanel->GetChildrenCount()
					? ParentPanel->InsertChildAt(ChildIndex, Widget)
					: ParentPanel->AddChild(Widget);
				if (!NewSlot)
				{
					OutError = FString::Printf(TEXT("snapshot_restore_widget_attach_failed:%s"), *WidgetName);
					return false;
				}
			}
			else if (!WidgetBlueprint->WidgetTree->RootWidget)
			{
				WidgetBlueprint->WidgetTree->RootWidget = Widget;
			}
		}

		if (FBlueprintHelperReviewTargetKindRegistry::GetHandlerKind(Target.TargetKind)
			== EBlueprintHelperReviewTargetHandlerKind::UMGWidgetProperty)
		{
			if (PropertyName.IsEmpty())
			{
				Snapshot->TryGetStringField(TEXT("property_path"), PropertyName);
			}
			if (PropertyName.IsEmpty())
			{
				return true;
			}

			FProperty* Property = Widget->GetClass()
				? Widget->GetClass()->FindPropertyByName(FName(*PropertyName))
				: nullptr;
			if (!Property)
			{
				OutError = FString::Printf(TEXT("widget_property_not_found:%s"), *PropertyName);
				return false;
			}

			FString ValueText;
			if (Snapshot->TryGetStringField(TEXT("value"), ValueText))
			{
				Widget->Modify();
				void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Widget);
				if (!Property->ImportText_Direct(*ValueText, ValuePtr, Widget, PPF_None))
				{
					OutError = FString::Printf(TEXT("widget_property_restore_failed:%s"), *PropertyName);
					return false;
				}
				MarkBlueprintReviewRestoreModified(WidgetBlueprint);
			}
		}
		else
		{
			Widget->Modify();
			if (!RestoreComponentPropertiesFromSnapshot(Widget, Snapshot, OutError))
			{
				return false;
			}
			MarkBlueprintReviewRestoreModified(WidgetBlueprint);
		}

		return true;
	}
bool FBlueprintHelperReviewSnapshotRestoreService::ExecuteSnapshotRestore(
		const FBlueprintHelperReviewAtomicTarget& Target,
		FString& OutError)
	{
		TSharedPtr<FJsonObject> Snapshot;
		if (!ParseReviewSnapshotJson(Target.BeforeSnapshotJson, Snapshot, OutError))
		{
			return false;
		}

		using FSnapshotRestoreHandler = TFunction<bool()>;
		struct FSnapshotRestoreRoute
		{
			EBlueprintHelperReviewTargetHandlerKind HandlerKind;
			FSnapshotRestoreHandler Handler;
		};

		const TArray<FSnapshotRestoreRoute> Routes =
		{
			{ EBlueprintHelperReviewTargetHandlerKind::BlueprintVariable, [&Target, &Snapshot, &OutError]() { return RestoreBlueprintVariableFromSnapshot(Target, Snapshot, OutError); } },
			{ EBlueprintHelperReviewTargetHandlerKind::Component, [&Target, &Snapshot, &OutError]() { return RestoreComponentFromSnapshot(Target, Snapshot, OutError); } },
			{ EBlueprintHelperReviewTargetHandlerKind::DataTableRow, [&Target, &Snapshot, &OutError]() { return RestoreDataTableRowFromSnapshot(Target, Snapshot, OutError); } },
			{ EBlueprintHelperReviewTargetHandlerKind::StructField, [&Target, &Snapshot, &OutError]() { return RestoreStructFieldFromSnapshot(Target, Snapshot, OutError); } },
			{ EBlueprintHelperReviewTargetHandlerKind::ObjectProperty, [&Target, &Snapshot, &OutError]() { return RestoreObjectPropertyFromSnapshot(Target, Snapshot, OutError); } },
			{ EBlueprintHelperReviewTargetHandlerKind::UMGWidget, [&Target, &Snapshot, &OutError]() { return RestoreWidgetFromSnapshot(Target, Snapshot, OutError); } },
			{ EBlueprintHelperReviewTargetHandlerKind::UMGWidgetProperty, [&Target, &Snapshot, &OutError]() { return RestoreWidgetFromSnapshot(Target, Snapshot, OutError); } }
		};

		const EBlueprintHelperReviewTargetHandlerKind HandlerKind =
			FBlueprintHelperReviewTargetKindRegistry::GetHandlerKind(Target.TargetKind);
		for (const FSnapshotRestoreRoute& Route : Routes)
		{
			if (Route.HandlerKind == HandlerKind)
			{
				return Route.Handler();
			}
		}

		OutError = FString::Printf(TEXT("snapshot_restore_unsupported_target_kind:%s"), *Target.TargetKind);
		return false;
	}
bool FBlueprintHelperReviewSnapshotRestoreService::ShouldUseSnapshotRestore(const FBlueprintHelperReviewAtomicTarget& Target)
	{
		return !Target.BeforeSnapshotJson.IsEmpty()
			&& FBlueprintHelperReviewTargetKindRegistry::SupportsSnapshotRestore(Target.TargetKind);
	}
FString FBlueprintHelperReviewSnapshotRestoreService::MakeObjectPathFromAssetPath(FString AssetPath)
	{
		AssetPath.TrimStartAndEndInline();
		if (AssetPath.IsEmpty())
		{
			return FString();
		}
		if (AssetPath.Contains(TEXT(".")))
		{
			return AssetPath;
		}

		const FString PackageName = AssetPath;
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
		return AssetName.IsEmpty()
			? FString()
			: FString::Printf(TEXT("%s.%s"), *PackageName, *AssetName);
	}
FBlueprintHelperReviewActionResult FBlueprintHelperReviewSnapshotRestoreService::RejectAssetFactoryTargetWithDefaultDispatcher(
		const FBlueprintHelperReviewVisibleChange& Change,
		const FBlueprintHelperReviewAtomicTarget& Target)
	{
		const FString ObjectPath = MakeObjectPathFromAssetPath(Target.AssetPath.IsEmpty() ? Change.AssetPath : Target.AssetPath);
		if (ObjectPath.IsEmpty())
		{
			return FBlueprintHelperReviewActionRecordUtils::MakeRejectFailureResult(
				Change,
				EBlueprintHelperReviewChangeStatus::NeedsAction,
				TEXT("asset_factory_missing_asset_path"));
		}

		UObject* AssetObject = FindObject<UObject>(nullptr, *ObjectPath);
		if (!AssetObject)
		{
			AssetObject = LoadObject<UObject>(nullptr, *ObjectPath);
		}

		FBlueprintHelperReviewActionResult Result;
		Result.TargetTransactionId = Change.LatestTransactionId;
		Result.RollbackMode = TEXT("asset_lifecycle_delete");
		Result.NewStatus = EBlueprintHelperReviewChangeStatus::Rejected;
		Result.bSupersededDataCompactionEligible = true;

		if (!AssetObject)
		{
			Result.bSucceeded = true;
			Result.Message = FString::Printf(TEXT("asset_already_missing:%s"), *ObjectPath);
			return Result;
		}

		TArray<UObject*> ObjectsToDelete;
		ObjectsToDelete.Add(AssetObject);
		const int32 DeletedCount = ObjectTools::ForceDeleteObjects(ObjectsToDelete, false);
		if (DeletedCount != ObjectsToDelete.Num())
		{
			return FBlueprintHelperReviewActionRecordUtils::MakeRejectFailureResult(
				Change,
				EBlueprintHelperReviewChangeStatus::RejectFailed,
				FString::Printf(TEXT("asset_delete_failed:%s"), *ObjectPath));
		}

		Result.bSucceeded = true;
		Result.Message = FString::Printf(TEXT("asset_deleted:%s"), *ObjectPath);
		return Result;
	}
