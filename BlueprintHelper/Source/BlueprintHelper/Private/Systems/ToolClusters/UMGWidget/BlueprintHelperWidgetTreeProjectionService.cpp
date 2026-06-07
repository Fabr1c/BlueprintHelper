#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetTreeProjectionService.h"

#include "Blueprint/WidgetTree.h"
#include "Components/NamedSlotInterface.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Shared/BlueprintHelperWidgetVersionCompat.h"
#include "WidgetBlueprint.h"

class FBlueprintHelperWidgetTreeProjectionServiceLocalUtils
{
public:
	static bool Reject(
		const TCHAR* ErrorCode,
		const FString& ErrorMessage,
		FString& OutErrorCode,
		FString& OutErrorMessage)
	{
		OutErrorCode = ErrorCode;
		OutErrorMessage = ErrorMessage;
		return false;
	}

	static FString GetClassName(const UObject* Object)
	{
		if (!Object || !Object->GetClass())
		{
			return FString();
		}

		FString ClassName = Object->GetClass()->GetName();
		if (ClassName.StartsWith(TEXT("U")))
		{
			ClassName.RemoveFromStart(TEXT("U"));
		}
		return ClassName;
	}

	static FString GetClassPath(const UObject* Object)
	{
		return Object && Object->GetClass()
			? Object->GetClass()->GetPathName()
			: FString();
	}

	static FString GetClassPath(const UClass* Class)
	{
		return Class ? Class->GetPathName() : FString();
	}

	static bool IsWidgetVariable(UWidgetBlueprint* WidgetBlueprint, const UWidget* Widget)
	{
#if WITH_EDITORONLY_DATA
		if (!Widget)
		{
			return false;
		}

		(void)WidgetBlueprint;
		return Widget->bIsVariable;
#else
		return false;
#endif
	}

	static void AppendPropertyValue(
		const TSharedRef<FJsonObject>& OutProperties,
		UObject* OwnerObject,
		const FString& PropertyPath,
		FProperty* Property,
		const void* ValuePtr)
	{
		if (!Property || !ValuePtr)
		{
			return;
		}

		FString Value;
		Property->ExportText_Direct(Value, ValuePtr, nullptr, OwnerObject, PPF_None);
		OutProperties->SetStringField(PropertyPath, Value);

		const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		if (!StructProperty || !StructProperty->Struct)
		{
			return;
		}

		for (TFieldIterator<FProperty> It(StructProperty->Struct); It; ++It)
		{
			FProperty* ChildProperty = *It;
			if (!ChildProperty || ChildProperty->HasAnyPropertyFlags(CPF_Transient))
			{
				continue;
			}

			const void* ChildValuePtr = ChildProperty->ContainerPtrToValuePtr<void>(ValuePtr);
			AppendPropertyValue(
				OutProperties,
				OwnerObject,
				FString::Printf(TEXT("%s.%s"), *PropertyPath, *ChildProperty->GetName()),
				ChildProperty,
				ChildValuePtr);
		}
	}

	static TSharedPtr<FJsonObject> BuildSlotProperties(UPanelSlot* Slot)
	{
		if (!Slot)
		{
			return nullptr;
		}

		TSharedRef<FJsonObject> Properties = MakeShared<FJsonObject>();
		for (TFieldIterator<FProperty> It(Slot->GetClass()); It; ++It)
		{
			FProperty* Property = *It;
			if (!Property
				|| !Property->HasAnyPropertyFlags(CPF_Edit)
				|| Property->HasAnyPropertyFlags(CPF_Transient))
			{
				continue;
			}

			const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Slot);
			AppendPropertyValue(
				Properties,
				Slot,
				Property->GetName(),
				Property,
				ValuePtr);
		}

		return Properties->Values.Num() > 0
			? TSharedPtr<FJsonObject>(Properties)
			: nullptr;
	}
};

bool FBlueprintHelperWidgetTreeProjectionService::BuildWidgetTreeSummary(
	UWidgetBlueprint* WidgetBlueprint,
	FBlueprintHelperWidgetTreeSummary& OutSummary,
	FString& OutErrorCode,
	FString& OutErrorMessage)
{
	OutSummary = FBlueprintHelperWidgetTreeSummary();
	OutErrorCode.Reset();
	OutErrorMessage.Reset();

	if (!WidgetBlueprint)
	{
		return FBlueprintHelperWidgetTreeProjectionServiceLocalUtils::Reject(
			TEXT("target_not_widget_blueprint"),
			TEXT("WidgetBlueprint is required for WidgetTree projection."),
			OutErrorCode,
			OutErrorMessage);
	}

	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		return FBlueprintHelperWidgetTreeProjectionServiceLocalUtils::Reject(
			TEXT("widget_tree_not_found"),
			TEXT("WidgetBlueprint does not have a WidgetTree."),
			OutErrorCode,
			OutErrorMessage);
	}

	if (!WidgetTree->RootWidget)
	{
		return FBlueprintHelperWidgetTreeProjectionServiceLocalUtils::Reject(
			TEXT("widget_tree_root_not_found"),
			TEXT("WidgetTree does not have a root widget."),
			OutErrorCode,
			OutErrorMessage);
	}

	OutSummary.AssetClass = FBlueprintHelperWidgetTreeProjectionServiceLocalUtils::GetClassPath(
		WidgetBlueprint->GetClass());
	OutSummary.ParentClass = FBlueprintHelperWidgetTreeProjectionServiceLocalUtils::GetClassPath(
		WidgetBlueprint->ParentClass);
	OutSummary.Root = BuildWidgetItem(
		WidgetBlueprint,
		WidgetTree->RootWidget,
		FString(),
		FString(),
		0);

	BuildFlatIndex(OutSummary.Root, OutSummary.Index);
	AppendNamedSlotFacts(WidgetBlueprint, OutSummary);

	return true;
}

FBlueprintHelperWidgetTreeItem FBlueprintHelperWidgetTreeProjectionService::BuildWidgetItem(
	UWidgetBlueprint* WidgetBlueprint,
	UWidget* Widget,
	const FString& ParentName,
	const FString& SlotName,
	int32 VirtualIndex)
{
	FBlueprintHelperWidgetTreeItem Item;
	if (!Widget)
	{
		return Item;
	}

	Item.WidgetName = Widget->GetName();
	Item.WidgetClass = FBlueprintHelperWidgetTreeProjectionServiceLocalUtils::GetClassName(Widget);
	Item.WidgetClassPath = FBlueprintHelperWidgetTreeProjectionServiceLocalUtils::GetClassPath(Widget);
	Item.ParentName = ParentName;
	Item.SlotName = SlotName;
	Item.VirtualIndex = VirtualIndex;
	Item.bIsVariable = FBlueprintHelperWidgetTreeProjectionServiceLocalUtils::IsWidgetVariable(
		WidgetBlueprint,
		Widget);
	Item.bIsInherited = false;

	if (Widget->Slot)
	{
		Item.SlotClassPath = FBlueprintHelperWidgetTreeProjectionServiceLocalUtils::GetClassPath(Widget->Slot);
		Item.SlotProperties = FBlueprintHelperWidgetTreeProjectionServiceLocalUtils::BuildSlotProperties(Widget->Slot);
	}

	AppendPanelChildren(WidgetBlueprint, Widget, Item);
	return Item;
}

void FBlueprintHelperWidgetTreeProjectionService::AppendPanelChildren(
	UWidgetBlueprint* WidgetBlueprint,
	UWidget* Widget,
	FBlueprintHelperWidgetTreeItem& Item)
{
	UPanelWidget* Panel = Cast<UPanelWidget>(Widget);
	if (!Panel)
	{
		return;
	}

	const int32 ChildCount = Panel->GetChildrenCount();
	for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
	{
		UWidget* ChildWidget = Panel->GetChildAt(ChildIndex);
		if (!ChildWidget)
		{
			continue;
		}

		Item.Children.Add(BuildWidgetItem(
			WidgetBlueprint,
			ChildWidget,
			Item.WidgetName,
			FString(),
			ChildIndex));
	}
}

void FBlueprintHelperWidgetTreeProjectionService::BuildFlatIndex(
	const FBlueprintHelperWidgetTreeItem& Item,
	TMap<FString, FBlueprintHelperWidgetTreeItem>& OutIndex)
{
	if (!Item.WidgetName.IsEmpty())
	{
		OutIndex.Add(Item.WidgetName, Item);
	}

	for (const FBlueprintHelperWidgetTreeItem& Child : Item.Children)
	{
		BuildFlatIndex(Child, OutIndex);
	}
}

void FBlueprintHelperWidgetTreeProjectionService::AppendNamedSlotFacts(
	UWidgetBlueprint* WidgetBlueprint,
	FBlueprintHelperWidgetTreeSummary& Summary)
{
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		return;
	}

	TSet<FString> SeenFacts;
	WidgetBlueprint->WidgetTree->ForEachWidget([WidgetBlueprint, &SeenFacts, &Summary](UWidget* Widget)
	{
		if (!Widget)
		{
			return;
		}

		INamedSlotInterface* NamedSlotHost = Cast<INamedSlotInterface>(Widget);
		if (!NamedSlotHost)
		{
			return;
		}

		TArray<FName> SlotNames;
		NamedSlotHost->GetSlotNames(SlotNames);
		for (const FName& SlotName : SlotNames)
		{
			UWidget* ContentWidget = NamedSlotHost->GetContentForSlot(SlotName);
			if (!ContentWidget)
			{
				continue;
			}

			const FString FactKey = FString::Printf(
				TEXT("%s|%s|%s"),
				*Widget->GetName(),
				*SlotName.ToString(),
				*ContentWidget->GetName());
			if (SeenFacts.Contains(FactKey))
			{
				continue;
			}
			SeenFacts.Add(FactKey);

			FBlueprintHelperNamedSlotEntry Entry;
			Entry.HostWidgetName = Widget->GetName();
			Entry.SlotName = SlotName.ToString();
			Entry.ContentWidgetName = ContentWidget->GetName();
			Entry.VirtualIndex = 0;
			Summary.NamedSlots.Add(Entry);

			FBlueprintHelperWidgetTreeItem NamedSlotContentItem = BuildWidgetItem(
				WidgetBlueprint,
				ContentWidget,
				Entry.HostWidgetName,
				Entry.SlotName,
				0);
			BuildFlatIndex(NamedSlotContentItem, Summary.Index);
		}
	});
}
