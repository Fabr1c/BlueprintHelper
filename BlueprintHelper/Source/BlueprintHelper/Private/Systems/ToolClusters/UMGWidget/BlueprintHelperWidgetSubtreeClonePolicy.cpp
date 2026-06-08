#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetSubtreeClonePolicy.h"

#include "Blueprint/WidgetTree.h"
#include "Components/NamedSlotInterface.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Shared/BlueprintHelperWidgetVersionCompat.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

class FBlueprintHelperWidgetSubtreeClonePolicyLocalUtils
{
public:
	static UClass* ResolveWidgetClass(const FString& ClassName, FString& OutError)
	{
		if (ClassName.IsEmpty())
		{
			OutError = TEXT("widget_class_required");
			return nullptr;
		}
		if (UClass* LoadedClass = LoadObject<UClass>(nullptr, *ClassName))
		{
			if (LoadedClass->IsChildOf(UWidget::StaticClass()) && !LoadedClass->HasAnyClassFlags(CLASS_Abstract))
			{
				return LoadedClass;
			}
		}

		FString FullName = ClassName;
		if (!FullName.StartsWith(TEXT("U")))
		{
			FullName = TEXT("U") + FullName;
		}
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Candidate = *It;
			if (Candidate->IsChildOf(UWidget::StaticClass()) &&
				!Candidate->HasAnyClassFlags(CLASS_Abstract) &&
				(Candidate->GetName() == FullName || Candidate->GetName() == ClassName))
			{
				return Candidate;
			}
		}

		OutError = FString::Printf(TEXT("Could not resolve widget class: %s"), *ClassName);
		return nullptr;
	}

	static UWidget* FindWidget(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName, FString& OutError)
	{
		if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
		{
			OutError = TEXT("widget_blueprint_or_tree_missing");
			return nullptr;
		}
		UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName));
		if (!Widget)
		{
			OutError = FString::Printf(TEXT("Widget not found: %s"), *WidgetName);
		}
		return Widget;
	}

	static FString ResolveCloneName(
		UWidgetTree* Tree,
		UWidget* SourceWidget,
		const TMap<FString, FString>& NameMapping,
		FString& OutError)
	{
		const FString SourceName = SourceWidget ? SourceWidget->GetName() : FString();
		const FString* ExplicitName = NameMapping.Find(SourceName);
		const FString CandidateName = ExplicitName ? *ExplicitName : SourceName + TEXT("_Copy");
		if (CandidateName.IsEmpty())
		{
			OutError = TEXT("clone_widget_name_empty");
			return FString();
		}
		if (Tree && Tree->FindWidget(FName(*CandidateName)))
		{
			OutError = FString::Printf(TEXT("widget_name_mapping_conflict:%s"), *CandidateName);
			return FString();
		}
		return CandidateName;
	}

	static void CopyEditableScalarProperties(UObject* Source, UObject* Target)
	{
		if (!Source || !Target || Source->GetClass() != Target->GetClass())
		{
			return;
		}

		for (TFieldIterator<FProperty> It(Source->GetClass()); It; ++It)
		{
			FProperty* Property = *It;
			if (!Property ||
				!Property->HasAnyPropertyFlags(CPF_Edit) ||
				Property->HasAnyPropertyFlags(CPF_Transient) ||
				CastField<FObjectPropertyBase>(Property))
			{
				continue;
			}

			FString Value;
			const void* SourceValue = Property->ContainerPtrToValuePtr<void>(Source);
			Property->ExportText_Direct(Value, SourceValue, nullptr, Source, PPF_None);
			void* TargetValue = Property->ContainerPtrToValuePtr<void>(Target);
			Property->ImportText_Direct(*Value, TargetValue, Target, PPF_None);
		}
	}

	static UWidget* CloneSubtree(
		UWidgetBlueprint* WidgetBlueprint,
		UWidget* SourceWidget,
		const TMap<FString, FString>& NameMapping,
		FString& OutError)
	{
		UWidgetTree* Tree = WidgetBlueprint ? WidgetBlueprint->WidgetTree : nullptr;
		if (!Tree || !SourceWidget)
		{
			OutError = TEXT("clone_source_or_tree_missing");
			return nullptr;
		}

		const FString CloneName = ResolveCloneName(Tree, SourceWidget, NameMapping, OutError);
		if (CloneName.IsEmpty())
		{
			return nullptr;
		}

		UWidget* ClonedWidget = Tree->ConstructWidget<UWidget>(SourceWidget->GetClass(), FName(*CloneName));
		if (!ClonedWidget)
		{
			OutError = TEXT("clone_widget_construct_failed");
			return nullptr;
		}
		ClonedWidget->SetFlags(RF_Transactional);
		CopyEditableScalarProperties(SourceWidget, ClonedWidget);

		UPanelWidget* SourcePanel = Cast<UPanelWidget>(SourceWidget);
		UPanelWidget* ClonedPanel = Cast<UPanelWidget>(ClonedWidget);
		if (SourcePanel && ClonedPanel)
		{
			for (int32 ChildIndex = 0; ChildIndex < SourcePanel->GetChildrenCount(); ++ChildIndex)
			{
				UWidget* SourceChild = SourcePanel->GetChildAt(ChildIndex);
				UWidget* ClonedChild = CloneSubtree(WidgetBlueprint, SourceChild, NameMapping, OutError);
				if (!ClonedChild)
				{
					return nullptr;
				}
				UPanelSlot* NewSlot = ClonedPanel->AddChild(ClonedChild);
				if (SourceChild && SourceChild->Slot && NewSlot)
				{
					CopyEditableScalarProperties(SourceChild->Slot, NewSlot);
				}
			}
		}

		FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(WidgetBlueprint, ClonedWidget);
		return ClonedWidget;
	}

	static bool AttachToTarget(
		UWidgetBlueprint* WidgetBlueprint,
		UWidget* Widget,
		const FString& TargetParentName,
		const FString& SlotName,
		TOptional<int32> VirtualIndex,
		FBlueprintHelperWidgetMutationResult& OutResult)
	{
		if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree || !Widget)
		{
			OutResult.ErrorMessage = TEXT("attach_widget_target_invalid");
			return false;
		}

		UWidget* TargetParent = WidgetBlueprint->WidgetTree->FindWidget(FName(*TargetParentName));
		if (!TargetParent)
		{
			OutResult.ErrorMessage = FString::Printf(TEXT("target_parent_not_found:%s"), *TargetParentName);
			return false;
		}

		if (!SlotName.IsEmpty())
		{
			INamedSlotInterface* NamedSlotHost = Cast<INamedSlotInterface>(TargetParent);
			if (!NamedSlotHost)
			{
				OutResult.ErrorMessage = TEXT("target_parent_is_not_named_slot_host");
				return false;
			}
			const FName SlotFName(*SlotName);
			if (NamedSlotHost->GetContentForSlot(SlotFName))
			{
				OutResult.ErrorMessage = TEXT("named_slot_content_exists");
				return false;
			}
			NamedSlotHost->SetContentForSlot(SlotFName, Widget);
			return true;
		}

		UPanelWidget* TargetPanel = Cast<UPanelWidget>(TargetParent);
		if (!TargetPanel)
		{
			OutResult.ErrorMessage = TEXT("target_parent_is_not_panel");
			return false;
		}
		const int32 InsertIndex = VirtualIndex.IsSet()
			? FMath::Clamp(VirtualIndex.GetValue(), 0, TargetPanel->GetChildrenCount())
			: TargetPanel->GetChildrenCount();
		if (!TargetPanel->InsertChildAt(InsertIndex, Widget))
		{
			OutResult.ErrorMessage = TEXT("attach_widget_insert_failed");
			return false;
		}
		return true;
	}

	static FName MakeTemporaryReplacementName(UWidgetTree* Tree, const FString& WidgetName)
	{
		for (int32 Index = 0; Index < 128; ++Index)
		{
			const FString Candidate = FString::Printf(TEXT("%s_ReplacementTmp_%d"), *WidgetName, Index);
			const FName CandidateName(*Candidate);
			if (!Tree || !Tree->FindWidget(CandidateName))
			{
				return CandidateName;
			}
		}
		return NAME_None;
	}
};

bool FBlueprintHelperWidgetSubtreeClonePolicy::DuplicateSubtree(
	UWidgetBlueprint* WidgetBlueprint,
	const FBlueprintHelperDuplicateWidgetSubtreeRequest& Request,
	FBlueprintHelperWidgetMutationResult& OutResult)
{
	OutResult.bDryRun = Request.bDryRun;
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		OutResult.ErrorMessage = TEXT("widget_blueprint_or_tree_missing");
		return false;
	}

	FString Error;
	UWidget* SourceWidget = FBlueprintHelperWidgetSubtreeClonePolicyLocalUtils::FindWidget(
		WidgetBlueprint,
		Request.SourceWidgetName,
		Error);
	if (!SourceWidget)
	{
		OutResult.ErrorMessage = Error;
		return false;
	}

	const FString PlannedName = FBlueprintHelperWidgetSubtreeClonePolicyLocalUtils::ResolveCloneName(
		WidgetBlueprint->WidgetTree,
		SourceWidget,
		Request.NameMapping,
		Error);
	if (PlannedName.IsEmpty())
	{
		OutResult.ErrorMessage = Error;
		return false;
	}

	if (Request.bDryRun)
	{
		OutResult.bSuccess = true;
		OutResult.AffectedWidget = PlannedName;
		return true;
	}

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Duplicate Widget Subtree")), WidgetBlueprint);
	Mutation.Modify(WidgetBlueprint->WidgetTree);
	UWidget* ClonedRoot = FBlueprintHelperWidgetSubtreeClonePolicyLocalUtils::CloneSubtree(
		WidgetBlueprint,
		SourceWidget,
		Request.NameMapping,
		OutResult.ErrorMessage);
	if (!ClonedRoot)
	{
		Mutation.Rollback();
		return false;
	}
	if (!FBlueprintHelperWidgetSubtreeClonePolicyLocalUtils::AttachToTarget(
		WidgetBlueprint,
		ClonedRoot,
		Request.TargetParentName,
		Request.SlotName,
		Request.VirtualIndex,
		OutResult))
	{
		Mutation.Rollback();
		return false;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	Mutation.Commit();
	OutResult.bSuccess = true;
	OutResult.AffectedWidget = ClonedRoot->GetName();
	return true;
}

bool FBlueprintHelperWidgetSubtreeClonePolicy::WrapWidget(
	UWidgetBlueprint* WidgetBlueprint,
	const FBlueprintHelperWrapWidgetRequest& Request,
	FBlueprintHelperWidgetMutationResult& OutResult)
{
	OutResult.bDryRun = Request.bDryRun;
	FString Error;
	UWidget* Widget = FBlueprintHelperWidgetSubtreeClonePolicyLocalUtils::FindWidget(
		WidgetBlueprint,
		Request.WidgetName,
		Error);
	if (!Widget)
	{
		OutResult.ErrorMessage = Error;
		return false;
	}
	UClass* WrapperClass = FBlueprintHelperWidgetSubtreeClonePolicyLocalUtils::ResolveWidgetClass(
		Request.WrapperClass,
		Error);
	if (!WrapperClass || !WrapperClass->IsChildOf(UPanelWidget::StaticClass()))
	{
		OutResult.ErrorMessage = Error.IsEmpty() ? TEXT("wrapper_class_must_be_panel_widget") : Error;
		return false;
	}
	if (WidgetBlueprint->WidgetTree->FindWidget(FName(*Request.WrapperName)))
	{
		OutResult.ErrorMessage = TEXT("wrapper_name_already_exists");
		return false;
	}
	if (Request.bDryRun)
	{
		OutResult.bSuccess = true;
		OutResult.AffectedWidget = Request.WrapperName;
		return true;
	}

	int32 OldIndex = INDEX_NONE;
	UPanelWidget* OldParent = UWidgetTree::FindWidgetParent(Widget, OldIndex);
	const bool bWasRoot = WidgetBlueprint->WidgetTree->RootWidget == Widget;
	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Wrap Widget")), WidgetBlueprint);
	Mutation.Modify(WidgetBlueprint->WidgetTree);
	if (OldParent)
	{
		Mutation.Modify(OldParent);
		OldParent->RemoveChild(Widget);
	}

	UPanelWidget* Wrapper = WidgetBlueprint->WidgetTree->ConstructWidget<UPanelWidget>(
		WrapperClass,
		FName(*Request.WrapperName));
	if (!Wrapper)
	{
		OutResult.ErrorMessage = TEXT("wrapper_construct_failed");
		Mutation.Rollback();
		return false;
	}
	Wrapper->SetFlags(RF_Transactional);
	if (OldParent)
	{
		OldParent->InsertChildAt(FMath::Max(OldIndex, 0), Wrapper);
	}
	else if (bWasRoot)
	{
		WidgetBlueprint->WidgetTree->RootWidget = Wrapper;
	}
	else
	{
		OutResult.ErrorMessage = TEXT("wrap_widget_parent_not_found");
		Mutation.Rollback();
		return false;
	}
	Wrapper->AddChild(Widget);
	FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(WidgetBlueprint, Wrapper);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	Mutation.Commit();
	OutResult.bSuccess = true;
	OutResult.AffectedWidget = Wrapper->GetName();
	return true;
}

bool FBlueprintHelperWidgetSubtreeClonePolicy::ReplaceWidgetClass(
	UWidgetBlueprint* WidgetBlueprint,
	const FBlueprintHelperReplaceWidgetClassRequest& Request,
	FBlueprintHelperWidgetMutationResult& OutResult)
{
	OutResult.bDryRun = Request.bDryRun;
	FString Error;
	UWidget* OldWidget = FBlueprintHelperWidgetSubtreeClonePolicyLocalUtils::FindWidget(
		WidgetBlueprint,
		Request.WidgetName,
		Error);
	if (!OldWidget)
	{
		OutResult.ErrorMessage = Error;
		return false;
	}
	if (!Request.ExpectedWidgetClassPath.IsEmpty() &&
		!OldWidget->GetClass()->GetPathName().Equals(Request.ExpectedWidgetClassPath, ESearchCase::IgnoreCase))
	{
		OutResult.ErrorMessage = TEXT("widget_class_mismatch");
		return false;
	}

	UClass* NewClass = FBlueprintHelperWidgetSubtreeClonePolicyLocalUtils::ResolveWidgetClass(
		Request.NewWidgetClass,
		Error);
	if (!NewClass)
	{
		OutResult.ErrorMessage = Error;
		return false;
	}
	if (Request.bPreserveChildren &&
		Cast<UPanelWidget>(OldWidget) &&
		!NewClass->IsChildOf(UPanelWidget::StaticClass()))
	{
		OutResult.ErrorMessage = TEXT("replace_widget_class_preserve_children_requires_panel");
		return false;
	}
	if (Request.bDryRun)
	{
		OutResult.bSuccess = true;
		OutResult.AffectedWidget = Request.WidgetName;
		return true;
	}

	int32 OldIndex = INDEX_NONE;
	UPanelWidget* OldParent = UWidgetTree::FindWidgetParent(OldWidget, OldIndex);
	const bool bWasRoot = WidgetBlueprint->WidgetTree->RootWidget == OldWidget;
	UPanelSlot* OldSlot = OldWidget->Slot;
	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Replace Widget Class")), WidgetBlueprint);
	Mutation.Modify(WidgetBlueprint->WidgetTree);
	if (OldParent)
	{
		Mutation.Modify(OldParent);
		OldParent->RemoveChild(OldWidget);
	}

	const FName TemporaryName =
		FBlueprintHelperWidgetSubtreeClonePolicyLocalUtils::MakeTemporaryReplacementName(
			WidgetBlueprint->WidgetTree,
			Request.WidgetName);
	if (TemporaryName.IsNone())
	{
		OutResult.ErrorMessage = TEXT("replacement_widget_temp_name_unavailable");
		Mutation.Rollback();
		return false;
	}

	UWidget* NewWidget = WidgetBlueprint->WidgetTree->ConstructWidget<UWidget>(
		NewClass,
		TemporaryName);
	if (!NewWidget)
	{
		OutResult.ErrorMessage = TEXT("replacement_widget_construct_failed");
		Mutation.Rollback();
		return false;
	}
	NewWidget->SetFlags(RF_Transactional);

	if (Request.bPreserveChildren)
	{
		UPanelWidget* OldPanel = Cast<UPanelWidget>(OldWidget);
		UPanelWidget* NewPanel = Cast<UPanelWidget>(NewWidget);
		while (OldPanel && NewPanel && OldPanel->GetChildrenCount() > 0)
		{
			UWidget* Child = OldPanel->GetChildAt(0);
			OldPanel->RemoveChild(Child);
			NewPanel->AddChild(Child);
		}
	}

	if (OldParent)
	{
		const int32 InsertIndex = Request.bPreserveSlot
			? FMath::Max(OldIndex, 0)
			: OldParent->GetChildrenCount();
		UPanelSlot* NewSlot = OldParent->InsertChildAt(InsertIndex, NewWidget);
		if (!NewSlot)
		{
			OutResult.ErrorMessage = TEXT("replacement_widget_insert_failed");
			Mutation.Rollback();
			return false;
		}
		if (Request.bPreserveSlot && OldSlot)
		{
			FBlueprintHelperWidgetSubtreeClonePolicyLocalUtils::CopyEditableScalarProperties(OldSlot, NewSlot);
		}
	}
	else if (bWasRoot)
	{
		WidgetBlueprint->WidgetTree->RootWidget = NewWidget;
	}
	else
	{
		OutResult.ErrorMessage = TEXT("replace_widget_class_parent_not_found");
		Mutation.Rollback();
		return false;
	}

	FString RetireError;
	if (!FBlueprintHelperWidgetVersionCompat::RetireSourceWidget(WidgetBlueprint, OldWidget, RetireError))
	{
		OutResult.ErrorMessage = RetireError;
		Mutation.Rollback();
		return false;
	}
	if (!NewWidget->Rename(*Request.WidgetName, WidgetBlueprint->WidgetTree, REN_DontCreateRedirectors))
	{
		OutResult.ErrorMessage = TEXT("replacement_widget_rename_failed");
		Mutation.Rollback();
		return false;
	}
	FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(WidgetBlueprint, NewWidget);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	Mutation.Commit();
	OutResult.bSuccess = true;
	OutResult.AffectedWidget = NewWidget->GetName();
	return true;
}
