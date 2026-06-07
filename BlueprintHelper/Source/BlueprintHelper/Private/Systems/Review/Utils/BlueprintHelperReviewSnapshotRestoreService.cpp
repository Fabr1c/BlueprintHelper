// BlueprintHelper Review BlueprintHelperReviewSnapshotRestoreService implementation.

#include "Systems/Review/Utils/BlueprintHelperReviewSnapshotRestoreService.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ActorComponent.h"
#include "Components/NamedSlotInterface.h"
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
#include "Misc/FeedbackContext.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Shared/BlueprintHelperWidgetVersionCompat.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Shared/Review/BlueprintHelperReviewStatusUtils.h"
#include "Shared/Review/BlueprintHelperReviewTargetKindRegistry.h"
#include "Systems/Debug/BlueprintHelperDebugCaseStoreService.h"
#include "Systems/Debug/BlueprintHelperDebugEntryService.h"
#include "Systems/Review/BlueprintHelperReviewPerformanceTrace.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalBodySnapshotService.h"
#include "Systems/ToolClusters/BlueprintComponent/BlueprintHelperComponentFacts.h"
#include "Systems/ToolClusters/BlueprintVariables/BlueprintHelperVariableReplicationService.h"
#include "Systems/ToolClusters/BlueprintSignature/Utils/BlueprintHelperSignatureMutationUtils.h"
#include "Systems/ToolClusters/ObjectProperty/BlueprintHelperPropertyReflectionService.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "WidgetBlueprint.h"
#include "Systems/Review/Utils/BlueprintHelperReviewActionRecordUtils.h"

static FText BlueprintHelperReviewCategoryTextFromSnapshot(const FString& Category)
{
	if (Category == TEXT("ue_default_variable_category"))
	{
		return UEdGraphSchema_K2::VR_DefaultCategory;
	}

	return FText::FromString(Category);
}

class FBlueprintHelperReviewWidgetTreeRestoreHelper
{
public:
	static bool IsNamedSlotContentTarget(const FBlueprintHelperReviewAtomicTarget& Target)
	{
		return Target.TargetSubKind.Equals(TEXT("named_slot_content"), ESearchCase::IgnoreCase);
	}

	static bool IsSlotPropertyTarget(const FBlueprintHelperReviewAtomicTarget& Target)
	{
		return Target.TargetKind.Equals(TEXT("slot_property"), ESearchCase::IgnoreCase) ||
			Target.TargetSubKind.Equals(TEXT("slot_property"), ESearchCase::IgnoreCase);
	}

	static bool IsWidgetVariableTarget(const FBlueprintHelperReviewAtomicTarget& Target)
	{
		return Target.TargetKind.Equals(TEXT("widget_variable"), ESearchCase::IgnoreCase) ||
			Target.TargetSubKind.Equals(TEXT("widget_variable"), ESearchCase::IgnoreCase);
	}

	static void ReadWidgetAndPropertyTarget(
		const FBlueprintHelperReviewAtomicTarget& Target,
		const TSharedPtr<FJsonObject>& Snapshot,
		FString& OutWidgetName,
		FString& OutPropertyName)
	{
		OutWidgetName.Reset();
		OutPropertyName.Reset();
		if (Snapshot.IsValid())
		{
			Snapshot->TryGetStringField(TEXT("widget_name"), OutWidgetName);
			Snapshot->TryGetStringField(TEXT("property_path"), OutPropertyName);
		}
		TryReadAnchorString(Target, TEXT("widget_name"), OutWidgetName);
		TryReadAnchorString(Target, TEXT("property_path"), OutPropertyName);
		if (OutPropertyName.IsEmpty())
		{
			OutPropertyName = Target.PropertyPath;
		}

		const FString SlotPropertyPrefix = TEXT("slot_property:");
		const FString WidgetVariablePrefix = TEXT("widget_variable:");
		if (Target.TargetKey.StartsWith(SlotPropertyPrefix, ESearchCase::IgnoreCase))
		{
			const FString TargetTail = Target.TargetKey.Mid(SlotPropertyPrefix.Len());
			FString ParsedWidgetName;
			FString ParsedPropertyPath;
			if (TargetTail.Split(TEXT("."), &ParsedWidgetName, &ParsedPropertyPath) && !ParsedWidgetName.IsEmpty())
			{
				if (OutWidgetName.IsEmpty())
				{
					OutWidgetName = ParsedWidgetName;
				}
				if (OutPropertyName.IsEmpty())
				{
					OutPropertyName = ParsedPropertyPath;
				}
			}
		}
		else if (Target.TargetKey.StartsWith(WidgetVariablePrefix, ESearchCase::IgnoreCase) && OutWidgetName.IsEmpty())
		{
			OutWidgetName = Target.TargetKey.Mid(WidgetVariablePrefix.Len());
		}

		if (IsWidgetVariableTarget(Target) && OutPropertyName.IsEmpty())
		{
			OutPropertyName = TEXT("is_variable");
		}
	}

	static bool RestoreSlotProperty(
		UWidgetBlueprint* WidgetBlueprint,
		const FBlueprintHelperReviewAtomicTarget& Target,
		const TSharedPtr<FJsonObject>& Snapshot,
		FString& OutError)
	{
		if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
		{
			OutError = FString::Printf(TEXT("widget_blueprint_not_found:%s"), *Target.AssetPath);
			return false;
		}
		if (!Snapshot.IsValid())
		{
			OutError = TEXT("slot_property_restore_missing_snapshot");
			return false;
		}

		FString WidgetName;
		FString PropertyPath;
		ReadWidgetAndPropertyTarget(Target, Snapshot, WidgetName, PropertyPath);
		if (WidgetName.IsEmpty() || PropertyPath.IsEmpty())
		{
			OutError = TEXT("slot_property_restore_missing_widget_or_property");
			return false;
		}

		UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName));
		if (!Widget)
		{
			OutError = FString::Printf(TEXT("slot_property_restore_widget_not_found:%s"), *WidgetName);
			return false;
		}
		if (!Widget->Slot)
		{
			OutError = FString::Printf(TEXT("slot_property_restore_slot_missing:%s"), *WidgetName);
			return false;
		}

		FString ValueText;
		if (!Snapshot->TryGetStringField(TEXT("value"), ValueText))
		{
			return true;
		}

		FProperty* Property = nullptr;
		void* ValuePtr = nullptr;
		FString ExpectedType;
		FString ErrorCode;
		FString ErrorMessage;
		if (!FBlueprintHelperPropertyReflectionService::ResolvePropertyPath(
			Widget->Slot,
			PropertyPath,
			Property,
			ValuePtr,
			ExpectedType,
			ErrorCode,
			ErrorMessage) ||
			!Property ||
			!ValuePtr)
		{
			OutError = FString::Printf(TEXT("slot_property_restore_not_found:%s:%s:%s"), *PropertyPath, *ErrorCode, *ErrorMessage);
			return false;
		}

		WidgetBlueprint->Modify();
		WidgetBlueprint->WidgetTree->Modify();
		Widget->Slot->Modify();
		Widget->Slot->PreEditChange(Property);
		if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
		{
			bool bBoolValue = false;
			if (!FBlueprintHelperReviewSnapshotRestoreService::ParseSnapshotBoolValue(ValueText, bBoolValue))
			{
				OutError = FString::Printf(TEXT("slot_property_bool_restore_failed:%s"), *PropertyPath);
				return false;
			}
			BoolProperty->SetPropertyValue(ValuePtr, bBoolValue);
		}
		else if (!Property->ImportText_Direct(*ValueText, ValuePtr, Widget->Slot, PPF_None))
		{
			OutError = FString::Printf(TEXT("slot_property_restore_failed:%s"), *PropertyPath);
			return false;
		}
		FPropertyChangedEvent PropertyChangedEvent(Property, EPropertyChangeType::ValueSet);
		Widget->Slot->PostEditChangeProperty(PropertyChangedEvent);
		Widget->Slot->SynchronizeProperties();
		FBlueprintHelperReviewSnapshotRestoreService::MarkBlueprintReviewRestoreModified(WidgetBlueprint);
		return true;
	}

	static bool RestoreWidgetVariable(
		UWidgetBlueprint* WidgetBlueprint,
		const FBlueprintHelperReviewAtomicTarget& Target,
		const TSharedPtr<FJsonObject>& Snapshot,
		FString& OutError)
	{
		if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
		{
			OutError = FString::Printf(TEXT("widget_blueprint_not_found:%s"), *Target.AssetPath);
			return false;
		}
		if (!Snapshot.IsValid())
		{
			OutError = TEXT("widget_variable_restore_missing_snapshot");
			return false;
		}

		FString WidgetName;
		FString PropertyPath;
		ReadWidgetAndPropertyTarget(Target, Snapshot, WidgetName, PropertyPath);
		if (WidgetName.IsEmpty())
		{
			OutError = TEXT("widget_variable_restore_missing_widget");
			return false;
		}

		UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName));
		if (!Widget)
		{
			OutError = FString::Printf(TEXT("widget_variable_restore_widget_not_found:%s"), *WidgetName);
			return false;
		}

		bool bIsVariable = false;
		FString ValueText;
		if (!Snapshot->TryGetBoolField(TEXT("is_variable"), bIsVariable))
		{
			if (!Snapshot->TryGetStringField(TEXT("value"), ValueText))
			{
				return true;
			}
			if (!FBlueprintHelperReviewSnapshotRestoreService::ParseSnapshotBoolValue(ValueText, bIsVariable))
			{
				OutError = FString::Printf(TEXT("widget_variable_bool_restore_failed:%s"), *WidgetName);
				return false;
			}
		}

		WidgetBlueprint->Modify();
		WidgetBlueprint->WidgetTree->Modify();
		Widget->Modify();
		FBlueprintHelperWidgetVersionCompat::SetWidgetVariableState(WidgetBlueprint, Widget, bIsVariable);
		FBlueprintHelperReviewSnapshotRestoreService::MarkBlueprintReviewRestoreModified(WidgetBlueprint);
		return true;
	}

	static bool RestoreNamedSlotContent(
		UWidgetBlueprint* WidgetBlueprint,
		const FBlueprintHelperReviewAtomicTarget& Target,
		const TSharedPtr<FJsonObject>& Snapshot,
		FString& OutError)
	{
		if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
		{
			OutError = FString::Printf(TEXT("widget_blueprint_not_found:%s"), *Target.AssetPath);
			return false;
		}
		if (!Snapshot.IsValid())
		{
			OutError = TEXT("named_slot_restore_missing_snapshot");
			return false;
		}

		FString HostWidgetName;
		FString SlotName;
		ReadHostAndSlot(Target, Snapshot, HostWidgetName, SlotName);
		if (HostWidgetName.IsEmpty() || SlotName.IsEmpty())
		{
			OutError = TEXT("named_slot_restore_missing_host_or_slot");
			return false;
		}

		UWidget* HostWidget = WidgetBlueprint->WidgetTree->FindWidget(FName(*HostWidgetName));
		INamedSlotInterface* NamedSlotHost = Cast<INamedSlotInterface>(HostWidget);
		if (!HostWidget || !HasNamedSlot(NamedSlotHost, FName(*SlotName)))
		{
			OutError = FString::Printf(TEXT("named_slot_restore_slot_not_found:%s.%s"), *HostWidgetName, *SlotName);
			return false;
		}

		bool bSnapshotExists = false;
		Snapshot->TryGetBoolField(TEXT("exists"), bSnapshotExists);
		bool bContentExists = false;
		Snapshot->TryGetBoolField(TEXT("content_exists"), bContentExists);

		WidgetBlueprint->Modify();
		WidgetBlueprint->WidgetTree->Modify();
		HostWidget->Modify();

		UWidget* CurrentContent = NamedSlotHost->GetContentForSlot(FName(*SlotName));
		if (!bSnapshotExists || !bContentExists)
		{
			if (CurrentContent)
			{
				CurrentContent->Modify();
				NamedSlotHost->SetContentForSlot(FName(*SlotName), nullptr);
				if (!FBlueprintHelperWidgetVersionCompat::RetireSourceWidget(WidgetBlueprint, CurrentContent, OutError))
				{
					return false;
				}
			}
			FBlueprintHelperReviewSnapshotRestoreService::MarkBlueprintReviewRestoreModified(WidgetBlueprint);
			return true;
		}

		FString ContentWidgetName;
		FString ContentWidgetClass;
		Snapshot->TryGetStringField(TEXT("content_widget_name"), ContentWidgetName);
		Snapshot->TryGetStringField(TEXT("content_widget_class"), ContentWidgetClass);
		if (ContentWidgetName.IsEmpty() || ContentWidgetClass.IsEmpty())
		{
			OutError = TEXT("named_slot_restore_missing_content_snapshot");
			return false;
		}

		UWidget* DesiredContent = WidgetBlueprint->WidgetTree->FindWidget(FName(*ContentWidgetName));
		if (!DesiredContent)
		{
			UClass* WidgetClass = LoadObject<UClass>(nullptr, *ContentWidgetClass);
			if (!WidgetClass || !WidgetClass->IsChildOf(UWidget::StaticClass()))
			{
				OutError = FString::Printf(TEXT("named_slot_restore_invalid_content_class:%s"), *ContentWidgetClass);
				return false;
			}
			DesiredContent = WidgetBlueprint->WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*ContentWidgetName));
			if (!DesiredContent)
			{
				OutError = FString::Printf(TEXT("named_slot_restore_construct_failed:%s"), *ContentWidgetName);
				return false;
			}
			DesiredContent->SetFlags(RF_Transactional);
		}

		if (CurrentContent && CurrentContent != DesiredContent)
		{
			CurrentContent->Modify();
			NamedSlotHost->SetContentForSlot(FName(*SlotName), nullptr);
			if (!FBlueprintHelperWidgetVersionCompat::RetireSourceWidget(WidgetBlueprint, CurrentContent, OutError))
			{
				return false;
			}
		}
		NamedSlotHost->SetContentForSlot(FName(*SlotName), DesiredContent);
		FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(WidgetBlueprint, DesiredContent);
		FBlueprintHelperReviewSnapshotRestoreService::MarkBlueprintReviewRestoreModified(WidgetBlueprint);
		return true;
	}

private:
	static bool HasNamedSlot(INamedSlotInterface* NamedSlotHost, const FName& SlotName)
	{
		if (!NamedSlotHost || SlotName.IsNone())
		{
			return false;
		}

		TArray<FName> SlotNames;
		NamedSlotHost->GetSlotNames(SlotNames);
		return SlotNames.Contains(SlotName);
	}

	static bool TryReadAnchorString(
		const FBlueprintHelperReviewAtomicTarget& Target,
		const TCHAR* FieldName,
		FString& OutValue)
	{
		if (Target.AnchorJson.IsEmpty())
		{
			return false;
		}

		TSharedPtr<FJsonObject> Anchor;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Target.AnchorJson);
		if (!FJsonSerializer::Deserialize(Reader, Anchor) || !Anchor.IsValid())
		{
			return false;
		}
		return Anchor->TryGetStringField(FieldName, OutValue) && !OutValue.IsEmpty();
	}

	static void ReadHostAndSlot(
		const FBlueprintHelperReviewAtomicTarget& Target,
		const TSharedPtr<FJsonObject>& Snapshot,
		FString& OutHostWidgetName,
		FString& OutSlotName)
	{
		if (Snapshot.IsValid())
		{
			Snapshot->TryGetStringField(TEXT("host_widget_name"), OutHostWidgetName);
			Snapshot->TryGetStringField(TEXT("slot_name"), OutSlotName);
		}
		if (OutHostWidgetName.IsEmpty())
		{
			TryReadAnchorString(Target, TEXT("host_widget_name"), OutHostWidgetName);
		}
		if (OutSlotName.IsEmpty())
		{
			TryReadAnchorString(Target, TEXT("slot_name"), OutSlotName);
		}
	}
};

static bool BlueprintHelperRestoreVariableReplicationFromSnapshot(
	UBlueprint* Blueprint,
	const FName VariableName,
	const TSharedPtr<FJsonObject>& Snapshot,
	FString& OutError)
{
	const TSharedPtr<FJsonObject>* ReplicationObject = nullptr;
	if (!Blueprint ||
		!Snapshot.IsValid() ||
		!Snapshot->TryGetObjectField(TEXT("replication"), ReplicationObject) ||
		!ReplicationObject ||
		!ReplicationObject->IsValid())
	{
		return true;
	}

	const int32 VariableIndex =
		FBlueprintHelperReviewSnapshotRestoreService::FindBlueprintVariableIndex(Blueprint, VariableName);
	if (VariableIndex == INDEX_NONE)
	{
		OutError = FString::Printf(TEXT("snapshot_restore_variable_replication_missing:%s"), *VariableName.ToString());
		return false;
	}

	FString ModeString;
	if (!(*ReplicationObject)->TryGetStringField(TEXT("mode"), ModeString))
	{
		OutError = FString::Printf(TEXT("snapshot_restore_variable_replication_missing_mode:%s"), *VariableName.ToString());
		return false;
	}

	FBlueprintHelperVariableReplicationRequest Request;
	if (!FBlueprintHelperVariableReplicationService::TryStringToMode(ModeString, Request.Mode))
	{
		OutError = FString::Printf(TEXT("snapshot_restore_variable_replication_invalid_mode:%s"), *ModeString);
		return false;
	}

	FString ConditionString;
	if (!(*ReplicationObject)->TryGetStringField(TEXT("condition"), ConditionString))
	{
		ConditionString = TEXT("none");
	}
	if (!FBlueprintHelperVariableReplicationService::TryStringToCondition(ConditionString, Request.Condition))
	{
		OutError = FString::Printf(TEXT("snapshot_restore_variable_replication_invalid_condition:%s"), *ConditionString);
		return false;
	}

	FString NotifyFunctionName;
	(*ReplicationObject)->TryGetStringField(TEXT("notify_function"), NotifyFunctionName);
	if (!NotifyFunctionName.IsEmpty())
	{
		Request.NotifyFunctionName = FName(*NotifyFunctionName);
	}
	Request.bCreateNotifyFunction = true;
	Request.bReuseExistingNotifyFunction = true;

	bool bReplicationChanged = false;
	FBlueprintHelperVariableReplicationError ReplicationError;
	if (!FBlueprintHelperVariableReplicationService::ApplyToMemberVariable(
		Blueprint,
		VariableName,
		Blueprint->NewVariables[VariableIndex],
		Request,
		bReplicationChanged,
		ReplicationError))
	{
		OutError = ReplicationError.Code.IsEmpty()
			? ReplicationError.Message
			: FString::Printf(TEXT("%s:%s"), *ReplicationError.Code, *ReplicationError.Message);
		return false;
	}

	return true;
}

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
USCS_Node* FBlueprintHelperReviewSnapshotRestoreService::FindScsNodeByTemplatePath(UBlueprint* Blueprint, const FString& ComponentTemplatePath)
	{
		if (!Blueprint || !Blueprint->SimpleConstructionScript || ComponentTemplatePath.IsEmpty())
		{
			return nullptr;
		}

		for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (Node && Node->ComponentTemplate && Node->ComponentTemplate->GetPathName() == ComponentTemplatePath)
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
		Blueprint->NewVariables[NewVariableIndex].Category = BlueprintHelperReviewCategoryTextFromSnapshot(Category);
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
			if (!BlueprintHelperRestoreVariableReplicationFromSnapshot(Blueprint, VariableFName, Snapshot, OutError))
			{
				return false;
			}
			MarkBlueprintReviewRestoreModified(Blueprint);
			return true;
		}

		FString DefaultValue;
		if (Snapshot->TryGetStringField(TEXT("default_value"), DefaultValue))
		{
			Blueprint->NewVariables[VariableIndex].DefaultValue = DefaultValue;
		}
		if (!BlueprintHelperRestoreVariableReplicationFromSnapshot(Blueprint, VariableFName, Snapshot, OutError))
		{
			return false;
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
			FString PropertyPath;
			FString ValueText;
			PropertyJson->TryGetStringField(TEXT("property_path"), PropertyPath);
			PropertyJson->TryGetStringField(TEXT("name"), PropertyName);
			if (PropertyPath.IsEmpty())
			{
				PropertyPath = PropertyName;
			}
			if (!PropertyJson->TryGetStringField(TEXT("value"), ValueText) ||
				PropertyPath.IsEmpty())
			{
				continue;
			}

			FProperty* Property = nullptr;
			void* ValuePtr = nullptr;
			FString ExpectedType;
			FString ErrorCode;
			FString ErrorMessage;
			if (!FBlueprintHelperPropertyReflectionService::ResolvePropertyPath(
				ComponentTemplate,
				PropertyPath,
				Property,
				ValuePtr,
				ExpectedType,
				ErrorCode,
				ErrorMessage) ||
				!Property ||
				!ValuePtr)
			{
				OutError = FString::Printf(TEXT("component_property_restore_missing:%s:%s"), *PropertyPath, *ErrorCode);
				return false;
			}

			if (!Property->ImportText_Direct(*ValueText, ValuePtr, ComponentTemplate, PPF_None))
			{
				OutError = FString::Printf(TEXT("component_property_restore_failed:%s"), *PropertyPath);
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

		FString ComponentName = ExtractTargetName(Target);
		FString ComponentTemplatePath = Target.ComponentTemplatePath;
		if (ComponentTemplatePath.IsEmpty() && Snapshot.IsValid())
		{
			Snapshot->TryGetStringField(TEXT("component_template_path"), ComponentTemplatePath);
		}
		FString SnapshotComponentName;
		if (Snapshot.IsValid())
		{
			Snapshot->TryGetStringField(TEXT("component_name"), SnapshotComponentName);
		}
		if (!SnapshotComponentName.IsEmpty())
		{
			ComponentName = SnapshotComponentName;
		}
		FString ExpectedComponentId = Target.ComponentId;
		if (ExpectedComponentId.IsEmpty() && Snapshot.IsValid())
		{
			Snapshot->TryGetStringField(TEXT("component_id"), ExpectedComponentId);
		}
		if (ComponentName.IsEmpty() && ComponentTemplatePath.IsEmpty())
		{
			OutError = TEXT("missing_component_name");
			return false;
		}

		bool bSnapshotExists = false;
		Snapshot->TryGetBoolField(TEXT("exists"), bSnapshotExists);
		USCS_Node* Node = ComponentTemplatePath.IsEmpty()
			? nullptr
			: FindScsNodeByTemplatePath(Blueprint, ComponentTemplatePath);
		if (!Node &&
			!Target.ComponentTemplatePath.IsEmpty() &&
			Target.ComponentTemplatePath != ComponentTemplatePath)
		{
			Node = FindScsNodeByTemplatePath(Blueprint, Target.ComponentTemplatePath);
		}
		if (!Node && !ComponentTemplatePath.IsEmpty() && bSnapshotExists)
		{
			if (!ComponentName.IsEmpty() && FindScsNodeByName(Blueprint, ComponentName))
			{
				OutError = FString::Printf(TEXT("component_template_path_not_found:%s"), *ComponentTemplatePath);
				return false;
			}
		}
		if (!Node && !ComponentName.IsEmpty())
		{
			Node = FindScsNodeByName(Blueprint, ComponentName);
		}

		if (Node && !ExpectedComponentId.IsEmpty())
		{
			const FBlueprintHelperComponentInfo Info =
				FBlueprintHelperComponentFacts::BuildReadbackFact(*Blueprint, *Node);
			if (Info.ComponentId != ExpectedComponentId)
			{
				OutError = FString::Printf(
					TEXT("component_id_mismatch:%s:%s"),
					*ExpectedComponentId,
					*Info.ComponentId);
				return false;
			}
		}
		if (Node && ComponentName.IsEmpty())
		{
			ComponentName = Node->GetVariableName().ToString();
		}

		const FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Review Restore Component")));
		Blueprint->Modify();
		Blueprint->SimpleConstructionScript->Modify();
		if (!bSnapshotExists)
		{
			if (Node)
			{
				if (Node->GetChildNodes().Num() > 0)
				{
					OutError = FString::Printf(
						TEXT("snapshot_restore_component_has_children:%s"),
						*ComponentName);
					return false;
				}
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
					Node->ParentComponentOrVariableName = ParentNode->GetVariableName();
					Node->ParentComponentOwnerClassName = NAME_None;
					Node->bIsParentComponentNative = false;
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
		if (!ComponentName.IsEmpty() && Node->GetVariableName().ToString() != ComponentName)
		{
			FBlueprintEditorUtils::RenameComponentMemberVariable(Blueprint, Node, FName(*ComponentName));
		}

		FString SnapshotParentComponentName;
		Snapshot->TryGetStringField(TEXT("parent_component"), SnapshotParentComponentName);
		FString SnapshotSocketName;
		Snapshot->TryGetStringField(TEXT("socket_name"), SnapshotSocketName);
		USCS_Node* CurrentParentNode = Blueprint->SimpleConstructionScript->FindParentNode(Node);
		const FString CurrentParentComponentName = CurrentParentNode
			? CurrentParentNode->GetVariableName().ToString()
			: TEXT("");
		if (CurrentParentComponentName != SnapshotParentComponentName)
		{
			if (SnapshotParentComponentName.IsEmpty())
			{
				Blueprint->SimpleConstructionScript->RemoveNode(Node, false);
				Node->ParentComponentOrVariableName = NAME_None;
				Node->ParentComponentOwnerClassName = NAME_None;
				Node->bIsParentComponentNative = false;
				Node->AttachToName = NAME_None;
				Blueprint->SimpleConstructionScript->AddNode(Node);
			}
			else
			{
				USCS_Node* SnapshotParentNode = FindScsNodeByName(Blueprint, SnapshotParentComponentName);
				if (!SnapshotParentNode)
				{
					OutError = FString::Printf(
						TEXT("snapshot_restore_component_parent_not_found:%s"),
						*SnapshotParentComponentName);
					return false;
				}
				if (SnapshotParentNode == Node || SnapshotParentNode->IsChildOf(Node))
				{
					OutError = FString::Printf(
						TEXT("snapshot_restore_component_parent_cycle:%s"),
						*SnapshotParentComponentName);
					return false;
				}
				SnapshotParentNode->Modify();
				Blueprint->SimpleConstructionScript->RemoveNode(Node, false);
				SnapshotParentNode->AddChildNode(Node);
				Node->ParentComponentOrVariableName = SnapshotParentNode->GetVariableName();
				Node->ParentComponentOwnerClassName = NAME_None;
				Node->bIsParentComponentNative = false;
			}
		}
		if (Node->AttachToName.ToString() != SnapshotSocketName)
		{
			Node->AttachToName = SnapshotSocketName.IsEmpty() ? NAME_None : FName(*SnapshotSocketName);
		}
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
			const TCHAR* ImportResult = FBlueprintHelperVersionCompat::ImportScriptStructText(
				DataTable->GetRowStruct(),
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
				const TArray<FStructVariableDescription>& Descriptions = FStructureEditorUtils::GetVarDesc(Struct);
				if (Descriptions.Num() <= 1)
				{
					OutError = FString::Printf(TEXT("struct_field_last_row_cannot_remove:%s"), *FieldName);
					return false;
				}

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

		if (FBlueprintHelperReviewWidgetTreeRestoreHelper::IsNamedSlotContentTarget(Target))
		{
			const FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Review Restore Named Slot")));
			return FBlueprintHelperReviewWidgetTreeRestoreHelper::RestoreNamedSlotContent(
				WidgetBlueprint,
				Target,
				Snapshot,
				OutError);
		}
		if (FBlueprintHelperReviewWidgetTreeRestoreHelper::IsSlotPropertyTarget(Target))
		{
			const FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Review Restore Widget Slot Property")));
			return FBlueprintHelperReviewWidgetTreeRestoreHelper::RestoreSlotProperty(
				WidgetBlueprint,
				Target,
				Snapshot,
				OutError);
		}
		if (FBlueprintHelperReviewWidgetTreeRestoreHelper::IsWidgetVariableTarget(Target))
		{
			const FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Review Restore Widget Variable")));
			return FBlueprintHelperReviewWidgetTreeRestoreHelper::RestoreWidgetVariable(
				WidgetBlueprint,
				Target,
				Snapshot,
				OutError);
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
				if (Snapshot->TryGetNumberField(TEXT("virtual_index"), ChildIndexNumber))
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

static FString BlueprintHelperReviewExtractGraphTargetTail(const FString& TargetKey, const FString& Marker)
{
	const FString Token = Marker + TEXT(":");
	const int32 Pos = TargetKey.Find(Token, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	if (Pos != INDEX_NONE)
	{
		return TargetKey.Mid(Pos + Token.Len());
	}

	int32 LastColon = INDEX_NONE;
	if (TargetKey.FindLastChar(TEXT(':'), LastColon))
	{
		return TargetKey.Mid(LastColon + 1);
	}
	return TargetKey;
}

static FString BlueprintHelperReviewNormalizeGuidCandidate(const FString& Candidate)
{
	FString Trimmed = Candidate;
	Trimmed.TrimStartAndEndInline();
	if (Trimmed.IsEmpty())
	{
		return FString();
	}

	FGuid ParsedGuid;
	if (FGuid::Parse(Trimmed, ParsedGuid))
	{
		return ParsedGuid.ToString(EGuidFormats::Digits);
	}

	FString HexDigits;
	HexDigits.Reserve(Trimmed.Len());
	for (const TCHAR Ch : Trimmed)
	{
		if (FChar::IsHexDigit(Ch))
		{
			HexDigits.AppendChar(Ch);
		}
	}
	return HexDigits.Len() == 32 ? HexDigits : Trimmed;
}

static FString BlueprintHelperReviewGetGraphNodeBlockId(const UEdGraphNode* Node)
{
	if (!Node)
	{
		return FString();
	}
	if (UPackage* Package = Node->GetOutermost())
	{
		FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
		return MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId"));
	}
	return FString();
}

static bool BlueprintHelperReviewGraphNodeMatchesStableId(const UEdGraphNode* Node, const FString& Candidate)
{
	if (!Node || Candidate.IsEmpty())
	{
		return false;
	}

	if (Node->GetName().Equals(Candidate, ESearchCase::IgnoreCase))
	{
		return true;
	}

	const FString NodeGuidDigits = Node->NodeGuid.ToString(EGuidFormats::Digits);
	const FString CandidateGuidDigits = BlueprintHelperReviewNormalizeGuidCandidate(Candidate);
	if (!NodeGuidDigits.IsEmpty() && NodeGuidDigits.Equals(CandidateGuidDigits, ESearchCase::IgnoreCase))
	{
		return true;
	}

	if (UPackage* Package = Node->GetOutermost())
	{
		FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
		return MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId")).Equals(Candidate, ESearchCase::IgnoreCase)
			|| MetaData.GetValue(Node, TEXT("BlueprintHelperFeatureName")).Equals(Candidate, ESearchCase::IgnoreCase);
	}
	return false;
}

static UEdGraph* BlueprintHelperReviewFindGraph(UBlueprint* Blueprint, const FString& GraphName)
{
	if (!Blueprint)
	{
		return nullptr;
	}

	auto FindIn = [&GraphName](const TArray<UEdGraph*>& Graphs) -> UEdGraph*
	{
		for (UEdGraph* Graph : Graphs)
		{
			if (Graph && (GraphName.IsEmpty() || Graph->GetName() == GraphName))
			{
				return Graph;
			}
		}
		return nullptr;
	};

	if (UEdGraph* Graph = FindIn(Blueprint->UbergraphPages))
	{
		return Graph;
	}
	if (UEdGraph* Graph = FindIn(Blueprint->FunctionGraphs))
	{
		return Graph;
	}
	if (UEdGraph* Graph = FindIn(Blueprint->MacroGraphs))
	{
		return Graph;
	}
	return nullptr;
}

static void BlueprintHelperReviewCollectGraphRestoreNodes(
	UEdGraph* Graph,
	const FBlueprintHelperReviewAtomicTarget& Target,
	TArray<UEdGraphNode*>& OutNodes)
{
	if (!Graph)
	{
		return;
	}

	if (FBlueprintHelperReviewTargetKindRegistry::IsGraphNodeTarget(Target.TargetKind, Target.TargetKey))
	{
		const FString NodeName = Target.NodeGuid.IsEmpty()
			? BlueprintHelperReviewExtractGraphTargetTail(Target.TargetKey, TEXT("node"))
			: Target.NodeGuid;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (BlueprintHelperReviewGraphNodeMatchesStableId(Node, NodeName))
			{
				OutNodes.AddUnique(Node);
				return;
			}
		}
		return;
	}

	if (FBlueprintHelperReviewTargetKindRegistry::IsGraphBlockTarget(Target.TargetKind, Target.TargetKey))
	{
		const FString BlockId = BlueprintHelperReviewExtractGraphTargetTail(Target.TargetKey, TEXT("block"));
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && BlueprintHelperReviewGetGraphNodeBlockId(Node) == BlockId)
			{
				OutNodes.AddUnique(Node);
			}
		}
	}
}

static UEdGraphNode* BlueprintHelperReviewFindNodeBySnapshotGuid(UEdGraph* Graph, const FString& NodeGuid)
{
	if (!Graph || NodeGuid.IsEmpty())
	{
		return nullptr;
	}

	const FString NormalizedGuid = BlueprintHelperReviewNormalizeGuidCandidate(NodeGuid);
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node && Node->NodeGuid.ToString(EGuidFormats::Digits).Equals(NormalizedGuid, ESearchCase::IgnoreCase))
		{
			return Node;
		}
	}
	return nullptr;
}

static UEdGraphPin* BlueprintHelperReviewFindPinByName(UEdGraphNode* Node, const FString& PinName)
{
	if (!Node || PinName.IsEmpty())
	{
		return nullptr;
	}

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
		{
			return Pin;
		}
	}
	return nullptr;
}

static bool BlueprintHelperReviewRestoreNodePinsFromSnapshot(
	UEdGraph* Graph,
	UEdGraphNode* Node,
	const TSharedPtr<FJsonObject>& NodeSnapshot,
	FString& OutError)
{
	if (!Graph || !Node || !NodeSnapshot.IsValid())
	{
		OutError = TEXT("graph_node_snapshot_missing");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* PinValues = nullptr;
	if (!NodeSnapshot->TryGetArrayField(TEXT("pins"), PinValues) || !PinValues)
	{
		return true;
	}

	TArray<TPair<UEdGraphPin*, TSharedPtr<FJsonObject>>> PinsToRestore;
	for (const TSharedPtr<FJsonValue>& PinValue : *PinValues)
	{
		const TSharedPtr<FJsonObject> PinSnapshot = PinValue.IsValid() ? PinValue->AsObject() : nullptr;
		if (!PinSnapshot.IsValid())
		{
			continue;
		}

		FString PinName;
		PinSnapshot->TryGetStringField(TEXT("name"), PinName);
		if (UEdGraphPin* Pin = BlueprintHelperReviewFindPinByName(Node, PinName))
		{
			PinsToRestore.Add(TPair<UEdGraphPin*, TSharedPtr<FJsonObject>>(Pin, PinSnapshot));
		}
	}

	for (const TPair<UEdGraphPin*, TSharedPtr<FJsonObject>>& Entry : PinsToRestore)
	{
		UEdGraphPin* Pin = Entry.Key;
		if (!Pin)
		{
			continue;
		}
		Pin->Modify();
		Pin->BreakAllPinLinks(true);
		FString DefaultValue;
		if (Entry.Value.IsValid() && Entry.Value->TryGetStringField(TEXT("default_value"), DefaultValue))
		{
			Pin->DefaultValue = DefaultValue;
		}
	}

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (!Schema)
	{
		OutError = TEXT("graph_restore_k2_schema_missing");
		return false;
	}

	for (const TPair<UEdGraphPin*, TSharedPtr<FJsonObject>>& Entry : PinsToRestore)
	{
		UEdGraphPin* Pin = Entry.Key;
		const TSharedPtr<FJsonObject>& PinSnapshot = Entry.Value;
		if (!Pin || !PinSnapshot.IsValid())
		{
			continue;
		}

		const TArray<TSharedPtr<FJsonValue>>* LinkedValues = nullptr;
		if (!PinSnapshot->TryGetArrayField(TEXT("linked_to"), LinkedValues) || !LinkedValues)
		{
			continue;
		}

		for (const TSharedPtr<FJsonValue>& LinkedValue : *LinkedValues)
		{
			const FString LinkedRef = LinkedValue.IsValid() ? LinkedValue->AsString() : FString();
			FString LinkedNodeGuid;
			FString LinkedPinName;
			if (!LinkedRef.Split(TEXT(":"), &LinkedNodeGuid, &LinkedPinName) || LinkedNodeGuid.IsEmpty() || LinkedPinName.IsEmpty())
			{
				continue;
			}

			UEdGraphNode* LinkedNode = BlueprintHelperReviewFindNodeBySnapshotGuid(Graph, LinkedNodeGuid);
			UEdGraphPin* LinkedPin = BlueprintHelperReviewFindPinByName(LinkedNode, LinkedPinName);
			if (!LinkedPin)
			{
				continue;
			}

			UEdGraphPin* FromPin = Pin->Direction == EGPD_Output ? Pin : LinkedPin;
			UEdGraphPin* ToPin = Pin->Direction == EGPD_Output ? LinkedPin : Pin;
			if (!FromPin || !ToPin || FromPin->LinkedTo.Contains(ToPin))
			{
				continue;
			}
			Schema->TryCreateConnection(FromPin, ToPin);
		}
	}

	return true;
}

static bool BlueprintHelperReviewImportGraphRestoreText(
	UEdGraph* Graph,
	const FString& RestoreText,
	FString& OutError)
{
	if (RestoreText.IsEmpty())
	{
		OutError = TEXT("graph_snapshot_restore_text_missing");
		return false;
	}
	if (!FEdGraphUtilities::CanImportNodesFromText(Graph, RestoreText))
	{
		OutError = TEXT("graph_snapshot_restore_text_not_importable");
		return false;
	}

	TSet<UEdGraphNode*> ImportedNodes;
	FEdGraphUtilities::ImportNodesFromText(Graph, RestoreText, ImportedNodes);
	if (ImportedNodes.Num() == 0)
	{
		OutError = TEXT("graph_snapshot_restore_imported_no_nodes");
		return false;
	}
	return true;
}

static bool BlueprintHelperReviewImportGraphRestoreTextWithNodes(
	UEdGraph* Graph,
	const FString& RestoreText,
	TSet<UEdGraphNode*>& OutImportedNodes,
	FString& OutError)
{
	OutImportedNodes.Reset();
	if (RestoreText.IsEmpty())
	{
		OutError = TEXT("graph_snapshot_restore_text_missing");
		return false;
	}
	if (!FEdGraphUtilities::CanImportNodesFromText(Graph, RestoreText))
	{
		OutError = TEXT("graph_snapshot_restore_text_not_importable");
		return false;
	}

	FEdGraphUtilities::ImportNodesFromText(Graph, RestoreText, OutImportedNodes);
	if (OutImportedNodes.Num() == 0)
	{
		OutError = TEXT("graph_snapshot_restore_imported_no_nodes");
		return false;
	}
	return true;
}

static UEdGraphPin* BlueprintHelperReviewFindFirstExecPin(
	UEdGraphNode* Node,
	EEdGraphPinDirection Direction)
{
	if (!Node)
	{
		return nullptr;
	}

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->Direction == Direction && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			return Pin;
		}
	}
	return nullptr;
}

static UEdGraphNode* BlueprintHelperReviewFindImportedNodeByGuid(
	const TSet<UEdGraphNode*>& ImportedNodes,
	const FString& NodeGuid)
{
	if (NodeGuid.IsEmpty())
	{
		return nullptr;
	}

	const FString NormalizedGuid = BlueprintHelperReviewNormalizeGuidCandidate(NodeGuid);
	for (UEdGraphNode* Node : ImportedNodes)
	{
		if (Node && Node->NodeGuid.ToString(EGuidFormats::Digits).Equals(NormalizedGuid, ESearchCase::IgnoreCase))
		{
			return Node;
		}
	}
	return nullptr;
}

static FString BlueprintHelperReviewReadSnapshotCustomEventName(const TSharedPtr<FJsonObject>& NodeSnapshot)
{
	if (!NodeSnapshot.IsValid())
	{
		return TEXT("");
	}

	FString EventName;
	if (NodeSnapshot->TryGetStringField(TEXT("custom_function_name"), EventName))
	{
		EventName.TrimStartAndEndInline();
		if (!EventName.IsEmpty())
		{
			return EventName;
		}
	}

	NodeSnapshot->TryGetStringField(TEXT("title"), EventName);
	EventName.TrimStartAndEndInline();
	return EventName;
}

static void BlueprintHelperReviewRestoreCustomEventNameFromSnapshot(
	UEdGraphNode* Node,
	const TSharedPtr<FJsonObject>& NodeSnapshot)
{
	UK2Node_CustomEvent* EventNode = Cast<UK2Node_CustomEvent>(Node);
	if (!EventNode)
	{
		return;
	}

	const FString EventName = BlueprintHelperReviewReadSnapshotCustomEventName(NodeSnapshot);
	if (EventName.IsEmpty())
	{
		return;
	}

	EventNode->Modify();
	EventNode->OnRenameNode(EventName);
}

static void BlueprintHelperReviewRestoreImportedCustomEventNames(
	const TSharedPtr<FJsonObject>& Snapshot,
	const TSet<UEdGraphNode*>& ImportedNodes)
{
	if (!Snapshot.IsValid())
	{
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* NodeValues = nullptr;
	if (!Snapshot->TryGetArrayField(TEXT("nodes"), NodeValues) || !NodeValues)
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& NodeValue : *NodeValues)
	{
		const TSharedPtr<FJsonObject> NodeSnapshot = NodeValue.IsValid() ? NodeValue->AsObject() : nullptr;
		if (!NodeSnapshot.IsValid())
		{
			continue;
		}

		FString NodeGuid;
		NodeSnapshot->TryGetStringField(TEXT("guid"), NodeGuid);
		UEdGraphNode* EventNode = BlueprintHelperReviewFindImportedNodeByGuid(ImportedNodes, NodeGuid);
		if (!EventNode)
		{
			continue;
		}

		BlueprintHelperReviewRestoreCustomEventNameFromSnapshot(EventNode, NodeSnapshot);
	}
}

static UEdGraphPin* BlueprintHelperReviewFindPinInImportedNodes(
	const TSet<UEdGraphNode*>& ImportedNodes,
	const FString& PinName,
	EEdGraphPinDirection Direction)
{
	for (UEdGraphNode* Node : ImportedNodes)
	{
		if (!Node)
		{
			continue;
		}
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin &&
				Pin->Direction == Direction &&
				Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
			{
				return Pin;
			}
		}
	}
	return nullptr;
}

static bool BlueprintHelperReviewHasInboundExecFromImportedNode(
	UEdGraphPin* ExecInputPin,
	const TSet<UEdGraphNode*>& ImportedNodes)
{
	if (!ExecInputPin)
	{
		return false;
	}
	for (UEdGraphPin* LinkedPin : ExecInputPin->LinkedTo)
	{
		if (LinkedPin && ImportedNodes.Contains(LinkedPin->GetOwningNode()))
		{
			return true;
		}
	}
	return false;
}

static UEdGraphNode* BlueprintHelperReviewFindFirstImportedExecutableBodyNode(
	const TSet<UEdGraphNode*>& ImportedNodes)
{
	for (UEdGraphNode* Node : ImportedNodes)
	{
		UEdGraphPin* ExecIn = BlueprintHelperReviewFindFirstExecPin(Node, EGPD_Input);
		if (ExecIn && !BlueprintHelperReviewHasInboundExecFromImportedNode(ExecIn, ImportedNodes))
		{
			return Node;
		}
	}
	for (UEdGraphNode* Node : ImportedNodes)
	{
		if (BlueprintHelperReviewFindFirstExecPin(Node, EGPD_Input))
		{
			return Node;
		}
	}
	return nullptr;
}

static bool BlueprintHelperReviewConnectExternalBodyEntryLinks(
	UEdGraph* Graph,
	UEdGraphNode* EntryNode,
	const TSharedPtr<FJsonObject>& Snapshot,
	const TSet<UEdGraphNode*>& ImportedNodes)
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (!Schema || !Graph || !EntryNode || !Snapshot.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* LinkValues = nullptr;
	if (!Snapshot->TryGetArrayField(TEXT("entry_to_body_links"), LinkValues) || !LinkValues)
	{
		return false;
	}

	bool bConnectedAny = false;
	for (const TSharedPtr<FJsonValue>& LinkValue : *LinkValues)
	{
		const TSharedPtr<FJsonObject> LinkObject = LinkValue.IsValid() ? LinkValue->AsObject() : nullptr;
		const TSharedPtr<FJsonObject>* FromObjectPtr = nullptr;
		const TSharedPtr<FJsonObject>* ToObjectPtr = nullptr;
		if (!LinkObject.IsValid() ||
			!LinkObject->TryGetObjectField(TEXT("from"), FromObjectPtr) ||
			!FromObjectPtr || !FromObjectPtr->IsValid() ||
			!LinkObject->TryGetObjectField(TEXT("to"), ToObjectPtr) ||
			!ToObjectPtr || !ToObjectPtr->IsValid())
		{
			continue;
		}

		FString FromPinName;
		FString ToNodeGuid;
		FString ToPinName;
		(*FromObjectPtr)->TryGetStringField(TEXT("pin_name"), FromPinName);
		(*ToObjectPtr)->TryGetStringField(TEXT("node_guid"), ToNodeGuid);
		(*ToObjectPtr)->TryGetStringField(TEXT("pin_name"), ToPinName);
		UEdGraphPin* FromPin = BlueprintHelperReviewFindPinByName(EntryNode, FromPinName);
		UEdGraphNode* ToNode = BlueprintHelperReviewFindImportedNodeByGuid(ImportedNodes, ToNodeGuid);
		UEdGraphPin* ToPin = ToNode
			? BlueprintHelperReviewFindPinByName(ToNode, ToPinName)
			: BlueprintHelperReviewFindPinInImportedNodes(ImportedNodes, ToPinName, EGPD_Input);
		if (!FromPin || !ToPin)
		{
			continue;
		}

		FromPin->Modify();
		ToPin->Modify();
		ToPin->BreakAllPinLinks(true);
		UEdGraphPin* OutputPin = FromPin->Direction == EGPD_Output ? FromPin : ToPin;
		UEdGraphPin* InputPin = FromPin->Direction == EGPD_Output ? ToPin : FromPin;
		if (OutputPin && InputPin && Schema->TryCreateConnection(OutputPin, InputPin))
		{
			bConnectedAny = true;
		}
	}
	return bConnectedAny;
}

static bool BlueprintHelperReviewReconnectEntryToFirstImportedBodyNode(
	UEdGraph* Graph,
	UEdGraphNode* EntryNode,
	const TSet<UEdGraphNode*>& ImportedNodes,
	FString& OutError)
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (!Schema || !Graph || !EntryNode)
	{
		OutError = TEXT("external_body_restore_schema_or_context_missing");
		return false;
	}

	UEdGraphPin* EntryExecOut = BlueprintHelperReviewFindFirstExecPin(EntryNode, EGPD_Output);
	if (!EntryExecOut)
	{
		OutError = TEXT("external_body_entry_exec_output_not_found");
		return false;
	}

	UEdGraphNode* FirstBodyNode = BlueprintHelperReviewFindFirstImportedExecutableBodyNode(ImportedNodes);
	if (!FirstBodyNode)
	{
		return true;
	}

	UEdGraphPin* BodyExecIn = BlueprintHelperReviewFindFirstExecPin(FirstBodyNode, EGPD_Input);
	if (!BodyExecIn)
	{
		OutError = TEXT("external_body_restore_exec_input_not_found");
		return false;
	}

	EntryExecOut->Modify();
	BodyExecIn->Modify();
	BodyExecIn->BreakAllPinLinks(true);
	return Schema->TryCreateConnection(EntryExecOut, BodyExecIn);
}

bool FBlueprintHelperReviewSnapshotRestoreService::RestoreGraphFromSnapshot(
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

		UEdGraph* Graph = BlueprintHelperReviewFindGraph(Blueprint, Target.GraphName);
		if (!Graph)
		{
			OutError = FString::Printf(TEXT("graph_not_found:%s"), *Target.GraphName);
			return false;
		}

		bool bSnapshotExists = false;
		Snapshot->TryGetBoolField(TEXT("exists"), bSnapshotExists);
		TArray<UEdGraphNode*> NodesToRemove;
		BlueprintHelperReviewCollectGraphRestoreNodes(Graph, Target, NodesToRemove);

		const FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Review Restore Graph Snapshot")));
		Blueprint->Modify();
		Graph->Modify();

		if (!bSnapshotExists)
		{
			for (UEdGraphNode* Node : NodesToRemove)
			{
				if (Node)
				{
					FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
				}
			}
			MarkBlueprintReviewRestoreModified(Blueprint);
			Graph->NotifyGraphChanged();
			return true;
		}

		if (FBlueprintHelperReviewTargetKindRegistry::IsGraphNodeTarget(Target.TargetKind, Target.TargetKey))
		{
			const TSharedPtr<FJsonObject>* NodeSnapshot = nullptr;
			if (!Snapshot->TryGetObjectField(TEXT("node"), NodeSnapshot) || !NodeSnapshot || !NodeSnapshot->IsValid())
			{
				OutError = TEXT("graph_node_snapshot_missing");
				return false;
			}

			UEdGraphNode* Node = NodesToRemove.Num() > 0 ? NodesToRemove[0] : nullptr;
			if (Node)
			{
				if (!BlueprintHelperReviewRestoreNodePinsFromSnapshot(Graph, Node, *NodeSnapshot, OutError))
				{
					return false;
				}
				BlueprintHelperReviewRestoreCustomEventNameFromSnapshot(Node, *NodeSnapshot);
			}
			else
			{
				FString RestoreText;
				Snapshot->TryGetStringField(TEXT("restore_text"), RestoreText);
				if (!BlueprintHelperReviewImportGraphRestoreText(Graph, RestoreText, OutError))
				{
					return false;
				}
			}

			MarkBlueprintReviewRestoreModified(Blueprint);
			Graph->NotifyGraphChanged();
			return true;
		}

		if (FBlueprintHelperReviewTargetKindRegistry::IsGraphBlockTarget(Target.TargetKind, Target.TargetKey))
		{
			FString RestoreText;
			Snapshot->TryGetStringField(TEXT("restore_text"), RestoreText);
			const FString BlockId = BlueprintHelperReviewExtractGraphTargetTail(Target.TargetKey, TEXT("block"));
			for (UEdGraphNode* Node : NodesToRemove)
			{
				if (Node)
				{
					FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
				}
			}
			TSet<UEdGraphNode*> ImportedNodes;
			if (!BlueprintHelperReviewImportGraphRestoreTextWithNodes(Graph, RestoreText, ImportedNodes, OutError))
			{
				return false;
			}
			BlueprintHelperReviewRestoreImportedCustomEventNames(Snapshot, ImportedNodes);
			if (!BlockId.IsEmpty())
			{
				for (UEdGraphNode* Node : ImportedNodes)
				{
					if (!Node)
					{
						continue;
					}
					if (UPackage* Package = Node->GetOutermost())
					{
						FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
						MetaData.SetValue(Node, TEXT("BlueprintHelperOwned"), TEXT("true"));
						MetaData.SetValue(Node, TEXT("BlueprintHelperBlockId"), *BlockId);
					}
				}
			}
			MarkBlueprintReviewRestoreModified(Blueprint);
			Graph->NotifyGraphChanged();
			return true;
		}

		OutError = FString::Printf(TEXT("graph_snapshot_restore_unsupported_target_kind:%s"), *Target.TargetKind);
		return false;
	}

bool FBlueprintHelperReviewSnapshotRestoreService::RestoreExternalBoundaryFromSnapshot(
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

		UEdGraph* Graph = BlueprintHelperReviewFindGraph(Blueprint, Target.GraphName);
		if (!Graph)
		{
			OutError = FString::Printf(TEXT("graph_not_found:%s"), *Target.GraphName);
			return false;
		}

		bool bSnapshotExists = false;
		Snapshot->TryGetBoolField(TEXT("exists"), bSnapshotExists);
		if (!bSnapshotExists)
		{
			OutError = TEXT("graph_external_boundary_snapshot_missing");
			return false;
		}

		const TSharedPtr<FJsonObject>* NodeSnapshot = nullptr;
		if (!Snapshot->TryGetObjectField(TEXT("node"), NodeSnapshot) || !NodeSnapshot || !NodeSnapshot->IsValid())
		{
			OutError = TEXT("graph_external_boundary_node_snapshot_missing");
			return false;
		}

		FString NodeGuid = Target.NodeGuid;
		if (NodeGuid.IsEmpty())
		{
			(*NodeSnapshot)->TryGetStringField(TEXT("guid"), NodeGuid);
		}
		UEdGraphNode* Node = BlueprintHelperReviewFindNodeBySnapshotGuid(Graph, NodeGuid);
		if (!Node)
		{
			OutError = FString::Printf(TEXT("graph_external_boundary_node_not_found:%s"), *NodeGuid);
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* PinValues = nullptr;
		if (!(*NodeSnapshot)->TryGetArrayField(TEXT("pins"), PinValues) || !PinValues || PinValues->Num() == 0)
		{
			OutError = TEXT("graph_external_boundary_pin_snapshot_missing");
			return false;
		}

		bool bFoundBoundaryPin = false;
		for (const TSharedPtr<FJsonValue>& PinValue : *PinValues)
		{
			const TSharedPtr<FJsonObject> PinSnapshot = PinValue.IsValid() ? PinValue->AsObject() : nullptr;
			if (!PinSnapshot.IsValid())
			{
				continue;
			}

			FString PinName;
			PinSnapshot->TryGetStringField(TEXT("name"), PinName);
			if (BlueprintHelperReviewFindPinByName(Node, PinName))
			{
				bFoundBoundaryPin = true;
				break;
			}
		}
		if (!bFoundBoundaryPin)
		{
			OutError = FString::Printf(TEXT("graph_external_boundary_pin_not_found:%s"), *Target.PinPath);
			return false;
		}

		const FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Review Restore External Boundary")));
		Blueprint->Modify();
		Graph->Modify();
		Node->Modify();
		if (!BlueprintHelperReviewRestoreNodePinsFromSnapshot(Graph, Node, *NodeSnapshot, OutError))
		{
			return false;
		}

		MarkBlueprintReviewRestoreModified(Blueprint);
		Graph->NotifyGraphChanged();
		return true;
	}

bool FBlueprintHelperReviewSnapshotRestoreService::RestoreExternalNodeFromSnapshot(
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

		UEdGraph* Graph = BlueprintHelperReviewFindGraph(Blueprint, Target.GraphName);
		if (!Graph)
		{
			OutError = FString::Printf(TEXT("graph_not_found:%s"), *Target.GraphName);
			return false;
		}

		bool bSnapshotExists = false;
		Snapshot->TryGetBoolField(TEXT("exists"), bSnapshotExists);
		if (!bSnapshotExists)
		{
			OutError = TEXT("graph_external_node_snapshot_missing");
			return false;
		}

		FString NodeGuid = Target.NodeGuid;
		if (NodeGuid.IsEmpty())
		{
			Snapshot->TryGetStringField(TEXT("node_guid"), NodeGuid);
		}
		UEdGraphNode* Node = BlueprintHelperReviewFindNodeBySnapshotGuid(Graph, NodeGuid);
		if (!Node)
		{
			OutError = FString::Printf(TEXT("graph_external_node_not_found:%s"), *NodeGuid);
			return false;
		}

		FString FieldKind = Target.PropertyPath;
		if (FieldKind.IsEmpty())
		{
			Snapshot->TryGetStringField(TEXT("field_kind"), FieldKind);
		}

		FString Value;
		Snapshot->TryGetStringField(TEXT("value"), Value);

		const FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Review Restore External Node Field")));
		Blueprint->Modify();
		Graph->Modify();
		Node->Modify();

		if (FieldKind == TEXT("pin_default"))
		{
			FString PinName = Target.PinPath;
			if (PinName.IsEmpty())
			{
				Snapshot->TryGetStringField(TEXT("pin_name"), PinName);
			}
			UEdGraphPin* Pin = BlueprintHelperReviewFindPinByName(Node, PinName);
			if (!Pin)
			{
				OutError = FString::Printf(TEXT("graph_external_node_pin_not_found:%s"), *PinName);
				return false;
			}
			Pin->Modify();
			Pin->DefaultValue = Value;
		}
		else if (FieldKind == TEXT("node_comment"))
		{
			Node->NodeComment = Value;
		}
		else
		{
			OutError = FString::Printf(TEXT("graph_external_node_field_unsupported:%s"), *FieldKind);
			return false;
		}

		MarkBlueprintReviewRestoreModified(Blueprint);
		Graph->NotifyGraphChanged();
		return true;
	}

bool FBlueprintHelperReviewSnapshotRestoreService::RestoreExternalBodyFromSnapshot(
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

		UEdGraph* Graph = BlueprintHelperReviewFindGraph(Blueprint, Target.GraphName);
		if (!Graph)
		{
			OutError = FString::Printf(TEXT("graph_not_found:%s"), *Target.GraphName);
			return false;
		}

		bool bSnapshotExists = false;
		Snapshot->TryGetBoolField(TEXT("exists"), bSnapshotExists);
		if (!bSnapshotExists)
		{
			OutError = TEXT("graph_external_body_snapshot_missing");
			return false;
		}

		FString EntryNodeGuid = Target.NodeGuid;
		if (EntryNodeGuid.IsEmpty())
		{
			Snapshot->TryGetStringField(TEXT("entry_node_guid"), EntryNodeGuid);
		}
		UEdGraphNode* EntryNode = BlueprintHelperReviewFindNodeBySnapshotGuid(Graph, EntryNodeGuid);
		if (!EntryNode)
		{
			OutError = FString::Printf(TEXT("graph_external_body_entry_not_found:%s"), *EntryNodeGuid);
			return false;
		}

		FString RestoreText;
		Snapshot->TryGetStringField(TEXT("restore_text"), RestoreText);
		const TArray<TSharedPtr<FJsonValue>>* BodyNodeValues = nullptr;
		const bool bSnapshotHasBodyNodes =
			Snapshot->TryGetArrayField(TEXT("body_node_guids"), BodyNodeValues) &&
			BodyNodeValues &&
			BodyNodeValues->Num() > 0;
		if (bSnapshotHasBodyNodes && RestoreText.IsEmpty())
		{
			OutError = TEXT("graph_external_body_restore_text_missing");
			return false;
		}

		const FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Review Restore External Body")));
		Blueprint->Modify();
		Graph->Modify();
		EntryNode->Modify();

		const FBlueprintHelperExternalBodySnapshotService SnapshotService;
		const TArray<UEdGraphNode*> CurrentBodyNodes = SnapshotService.CollectBodyNodes(Graph, EntryNode);
		for (UEdGraphNode* Node : CurrentBodyNodes)
		{
			if (Node)
			{
				Node->Modify();
			}
		}
		for (UEdGraphNode* Node : CurrentBodyNodes)
		{
			if (Node)
			{
				FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
			}
		}

		for (UEdGraphPin* Pin : EntryNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				Pin->Modify();
				Pin->BreakAllPinLinks(true);
			}
		}

		TSet<UEdGraphNode*> ImportedNodes;
		if (!RestoreText.IsEmpty() &&
			!BlueprintHelperReviewImportGraphRestoreTextWithNodes(Graph, RestoreText, ImportedNodes, OutError))
		{
			return false;
		}

		if (ImportedNodes.Num() > 0 &&
			!BlueprintHelperReviewConnectExternalBodyEntryLinks(Graph, EntryNode, Snapshot, ImportedNodes) &&
			!BlueprintHelperReviewReconnectEntryToFirstImportedBodyNode(Graph, EntryNode, ImportedNodes, OutError))
		{
			return false;
		}

		MarkBlueprintReviewRestoreModified(Blueprint);
		Graph->NotifyGraphChanged();
		return true;
	}

bool FBlueprintHelperReviewSnapshotRestoreService::RestoreSignatureFromSnapshot(
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

		const FString SignatureName = ExtractTargetName(Target);
		if (SignatureName.IsEmpty())
		{
			OutError = TEXT("missing_signature_name");
			return false;
		}

		bool bSnapshotExists = false;
		Snapshot->TryGetBoolField(TEXT("exists"), bSnapshotExists);
		if (bSnapshotExists)
		{
			OutError = FString::Printf(TEXT("signature_snapshot_restore_existing_not_supported:%s"), *SignatureName);
			return false;
		}

		const FScopedTransaction Transaction(FText::FromString(TEXT("BlueprintHelper Review Restore Signature")));
		Blueprint->Modify();

		bool bRemoved = false;
		FString RemoveError;
		if (!FBlueprintHelperSignatureMutationUtils::RemoveEventDispatcherSignatureDirect(Blueprint, SignatureName, bRemoved, RemoveError))
		{
			OutError = RemoveError.IsEmpty()
				? FString::Printf(TEXT("signature_dispatcher_remove_failed:%s"), *SignatureName)
				: RemoveError;
			return false;
		}
		if (bRemoved)
		{
			MarkBlueprintReviewRestoreModified(Blueprint);
			return true;
		}

		FBlueprintHelperRemoveSignatureRequest RemoveRequest;
		RemoveRequest.AssetPath = Target.AssetPath;
		RemoveRequest.SignatureName = SignatureName;
		RemoveRequest.GraphName = Target.GraphName;
		RemoveRequest.bRequireReferenceContext = false;
		const FString SignatureKinds[] =
		{
			TEXT("function"),
			TEXT("override_event"),
			TEXT("native_event")
		};
		for (const FString& SignatureKind : SignatureKinds)
		{
			RemoveRequest.SignatureKind = SignatureKind;
			RemoveError.Reset();
			bRemoved = false;
			if (!FBlueprintHelperSignatureMutationUtils::RemoveSignatureDirect(Blueprint, RemoveRequest, bRemoved, RemoveError))
			{
				OutError = RemoveError.IsEmpty()
					? FString::Printf(TEXT("signature_remove_failed:%s:%s"), *SignatureKind, *SignatureName)
					: RemoveError;
				return false;
			}
			if (bRemoved)
			{
				MarkBlueprintReviewRestoreModified(Blueprint);
				return true;
			}
		}

		for (UEdGraph* Graph : Blueprint->MacroGraphs)
		{
			if (Graph && Graph->GetFName() == FName(*SignatureName))
			{
				FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph, EGraphRemoveFlags::Recompile);
				MarkBlueprintReviewRestoreModified(Blueprint);
				return true;
			}
		}

		TArray<FString> CustomEventGraphNames;
		if (!Target.GraphName.IsEmpty())
		{
			CustomEventGraphNames.Add(Target.GraphName);
		}
		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			if (Graph)
			{
				CustomEventGraphNames.AddUnique(Graph->GetName());
			}
		}
		RemoveRequest.SignatureKind = TEXT("custom_event");
		for (const FString& GraphName : CustomEventGraphNames)
		{
			RemoveRequest.GraphName = GraphName;
			RemoveError.Reset();
			bRemoved = false;
			if (!FBlueprintHelperSignatureMutationUtils::RemoveSignatureDirect(Blueprint, RemoveRequest, bRemoved, RemoveError))
			{
				continue;
			}
			if (bRemoved)
			{
				MarkBlueprintReviewRestoreModified(Blueprint);
				return true;
			}
		}

		return true;
	}

bool FBlueprintHelperReviewSnapshotRestoreService::ExecuteSnapshotRestore(
		const FBlueprintHelperReviewAtomicTarget& Target,
		FString& OutError)
	{
		FBlueprintHelperReviewPerformanceScope Scope(TEXT("ReviewReject.ExecuteSnapshotRestore"));
		Scope.AddText(TEXT("target_kind"), Target.TargetKind);
		Scope.AddText(TEXT("target_key"), Target.TargetKey);
		Scope.AddText(TEXT("asset"), Target.AssetPath);
		Scope.AddBytes(TEXT("before_snapshot"), Target.BeforeSnapshotJson.Len());

		TSharedPtr<FJsonObject> Snapshot;
		{
			FBlueprintHelperReviewPerformanceScope ParseScope(TEXT("ReviewReject.ParseSnapshotJson"));
			ParseScope.AddText(TEXT("target_kind"), Target.TargetKind);
			ParseScope.AddBytes(TEXT("before_snapshot"), Target.BeforeSnapshotJson.Len());
			if (!ParseReviewSnapshotJson(Target.BeforeSnapshotJson, Snapshot, OutError))
			{
				Scope.AddText(TEXT("result"), TEXT("parse_failed"));
				Scope.AddText(TEXT("error"), OutError);
				return false;
			}
		}

		using FSnapshotRestoreHandler = TFunction<bool()>;
		struct FSnapshotRestoreRoute
		{
			EBlueprintHelperReviewTargetHandlerKind HandlerKind;
			FSnapshotRestoreHandler Handler;
		};

		const TArray<FSnapshotRestoreRoute> Routes =
		{
			{ EBlueprintHelperReviewTargetHandlerKind::GraphNode, [&Target, &Snapshot, &OutError]() { return RestoreGraphFromSnapshot(Target, Snapshot, OutError); } },
			{ EBlueprintHelperReviewTargetHandlerKind::GraphBlock, [&Target, &Snapshot, &OutError]() { return RestoreGraphFromSnapshot(Target, Snapshot, OutError); } },
			{ EBlueprintHelperReviewTargetHandlerKind::GraphExternalBoundary, [&Target, &Snapshot, &OutError]() { return RestoreExternalBoundaryFromSnapshot(Target, Snapshot, OutError); } },
			{ EBlueprintHelperReviewTargetHandlerKind::GraphExternalNode, [&Target, &Snapshot, &OutError]() { return RestoreExternalNodeFromSnapshot(Target, Snapshot, OutError); } },
			{ EBlueprintHelperReviewTargetHandlerKind::GraphExternalBody, [&Target, &Snapshot, &OutError]() { return RestoreExternalBodyFromSnapshot(Target, Snapshot, OutError); } },
			{ EBlueprintHelperReviewTargetHandlerKind::BlueprintVariable, [&Target, &Snapshot, &OutError]() { return RestoreBlueprintVariableFromSnapshot(Target, Snapshot, OutError); } },
			{ EBlueprintHelperReviewTargetHandlerKind::Component, [&Target, &Snapshot, &OutError]() { return RestoreComponentFromSnapshot(Target, Snapshot, OutError); } },
			{ EBlueprintHelperReviewTargetHandlerKind::DataTableRow, [&Target, &Snapshot, &OutError]() { return RestoreDataTableRowFromSnapshot(Target, Snapshot, OutError); } },
			{ EBlueprintHelperReviewTargetHandlerKind::StructField, [&Target, &Snapshot, &OutError]() { return RestoreStructFieldFromSnapshot(Target, Snapshot, OutError); } },
			{ EBlueprintHelperReviewTargetHandlerKind::ObjectProperty, [&Target, &Snapshot, &OutError]() { return RestoreObjectPropertyFromSnapshot(Target, Snapshot, OutError); } },
			{ EBlueprintHelperReviewTargetHandlerKind::Signature, [&Target, &Snapshot, &OutError]() { return RestoreSignatureFromSnapshot(Target, Snapshot, OutError); } },
			{ EBlueprintHelperReviewTargetHandlerKind::UMGWidget, [&Target, &Snapshot, &OutError]() { return RestoreWidgetFromSnapshot(Target, Snapshot, OutError); } },
			{ EBlueprintHelperReviewTargetHandlerKind::UMGWidgetProperty, [&Target, &Snapshot, &OutError]() { return RestoreWidgetFromSnapshot(Target, Snapshot, OutError); } }
		};

		const EBlueprintHelperReviewTargetHandlerKind HandlerKind =
			FBlueprintHelperReviewTargetKindRegistry::GetHandlerKind(Target.TargetKind);
		Scope.AddCount(TEXT("handler_kind"), static_cast<int64>(HandlerKind));
		for (const FSnapshotRestoreRoute& Route : Routes)
		{
			if (Route.HandlerKind == HandlerKind)
			{
				const bool bRestored = Route.Handler();
				Scope.AddText(TEXT("result"), bRestored ? TEXT("restored") : TEXT("failed"));
				if (!bRestored)
				{
					Scope.AddText(TEXT("error"), OutError);
				}
				return bRestored;
			}
		}

		OutError = FString::Printf(TEXT("snapshot_restore_unsupported_target_kind:%s"), *Target.TargetKind);
		Scope.AddText(TEXT("result"), TEXT("unsupported"));
		Scope.AddText(TEXT("error"), OutError);
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
		Result.TargetEvidenceId = Change.LatestEvidenceId;
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
