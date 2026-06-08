#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetRootRemovalPolicy.h"

#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Shared/BlueprintHelperWidgetVersionCompat.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "UObject/UObjectIterator.h"
#include "WidgetBlueprint.h"

class FBlueprintHelperWidgetRootRemovalPolicyLocalUtils
{
public:
	static UClass* ResolveWidgetClass(const FString& ClassName, FString& OutError)
	{
		if (ClassName.IsEmpty())
		{
			OutError = TEXT("replacement_widget_class_required");
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

		OutError = FString::Printf(TEXT("Could not resolve replacement widget class: %s"), *ClassName);
		return nullptr;
	}

	static bool ValidateRequest(
		UWidgetBlueprint* WidgetBlueprint,
		const FBlueprintHelperRemoveRootWidgetRequest& Request,
		UWidget*& OutRootWidget,
		FBlueprintHelperWidgetMutationResult& OutResult)
	{
		if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
		{
			OutResult.ErrorMessage = TEXT("widget_blueprint_or_tree_missing");
			return false;
		}

		OutRootWidget = WidgetBlueprint->WidgetTree->RootWidget;
		if (!OutRootWidget)
		{
			OutResult.ErrorMessage = TEXT("root_widget_missing");
			return false;
		}

		if (!Request.RootWidgetName.IsEmpty() &&
			!OutRootWidget->GetName().Equals(Request.RootWidgetName, ESearchCase::IgnoreCase))
		{
			OutResult.ErrorMessage = TEXT("root_widget_name_mismatch");
			return false;
		}

		if (!Request.ExpectedRootClassPath.IsEmpty() &&
			!OutRootWidget->GetClass()->GetPathName().Equals(Request.ExpectedRootClassPath, ESearchCase::IgnoreCase))
		{
			OutResult.ErrorMessage = TEXT("root_widget_class_mismatch");
			return false;
		}

		if (Request.ReplacementPolicy.IsEmpty())
		{
			OutResult.ErrorMessage = TEXT("root_removal_policy_required");
			return false;
		}
		return true;
	}

	static FName MakeTemporaryReplacementName(UWidgetTree* Tree, const FString& RootWidgetName)
	{
		for (int32 Index = 0; Index < 128; ++Index)
		{
			const FString Candidate = FString::Printf(TEXT("%s_RootReplacementTmp_%d"), *RootWidgetName, Index);
			const FName CandidateName(*Candidate);
			if (!Tree || !Tree->FindWidget(CandidateName))
			{
				return CandidateName;
			}
		}
		return NAME_None;
	}
};

bool FBlueprintHelperWidgetRootRemovalPolicy::Apply(
	UWidgetBlueprint* WidgetBlueprint,
	const FBlueprintHelperRemoveRootWidgetRequest& Request,
	FBlueprintHelperWidgetMutationResult& OutResult)
{
	OutResult.bDryRun = Request.bDryRun;

	UWidget* RootWidget = nullptr;
	if (!FBlueprintHelperWidgetRootRemovalPolicyLocalUtils::ValidateRequest(
		WidgetBlueprint,
		Request,
		RootWidget,
		OutResult))
	{
		return false;
	}

	UWidgetTree* Tree = WidgetBlueprint->WidgetTree;
	const FString Policy = Request.ReplacementPolicy.ToLower();
	if (Policy == TEXT("promote_single_child"))
	{
		UPanelWidget* RootPanel = Cast<UPanelWidget>(RootWidget);
		if (!RootPanel || RootPanel->GetChildrenCount() != 1)
		{
			OutResult.ErrorMessage = TEXT("root_promote_single_child_requires_exactly_one_child");
			return false;
		}

		UWidget* PromotedChild = RootPanel->GetChildAt(0);
		if (!PromotedChild)
		{
			OutResult.ErrorMessage = TEXT("root_promote_child_missing");
			return false;
		}

		if (!Request.bDryRun)
		{
			FBlueprintHelperScopedAssetMutation Mutation(
				FText::FromString(TEXT("BlueprintHelper Remove Root Widget")), WidgetBlueprint);
			Mutation.Modify(Tree);
			Mutation.Modify(RootPanel);
			RootPanel->RemoveChild(PromotedChild);
			Tree->RootWidget = PromotedChild;
			FString RetireError;
			if (!FBlueprintHelperWidgetVersionCompat::RetireSourceWidget(WidgetBlueprint, RootWidget, RetireError))
			{
				OutResult.ErrorMessage = RetireError;
				Mutation.Rollback();
				return false;
			}
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
			Mutation.Commit();
		}

		OutResult.bSuccess = true;
		OutResult.AffectedWidget = PromotedChild->GetName();
		return true;
	}

	if (Policy == TEXT("replace_with_empty_root"))
	{
		FString ClassError;
		UClass* ReplacementClass =
			FBlueprintHelperWidgetRootRemovalPolicyLocalUtils::ResolveWidgetClass(Request.ReplacementWidgetClass, ClassError);
		if (!ReplacementClass)
		{
			OutResult.ErrorMessage = ClassError;
			return false;
		}

		const FName ReplacementName = Request.ReplacementWidgetName.IsEmpty()
			? NAME_None
			: FName(*Request.ReplacementWidgetName);
		const bool bReuseRootName =
			!ReplacementName.IsNone() &&
			RootWidget &&
			RootWidget->GetFName() == ReplacementName;
		if (!ReplacementName.IsNone() && Tree->FindWidget(ReplacementName) && !bReuseRootName)
		{
			OutResult.ErrorMessage = TEXT("replacement_widget_name_already_exists");
			return false;
		}

		if (!Request.bDryRun)
		{
			FBlueprintHelperScopedAssetMutation Mutation(
				FText::FromString(TEXT("BlueprintHelper Replace Root Widget")), WidgetBlueprint);
			Mutation.Modify(Tree);
			const FName ConstructName = bReuseRootName
				? FBlueprintHelperWidgetRootRemovalPolicyLocalUtils::MakeTemporaryReplacementName(Tree, Request.ReplacementWidgetName)
				: ReplacementName;
			if (bReuseRootName && ConstructName.IsNone())
			{
				OutResult.ErrorMessage = TEXT("replacement_root_temp_name_unavailable");
				Mutation.Rollback();
				return false;
			}
			UWidget* Replacement = Tree->ConstructWidget<UWidget>(ReplacementClass, ConstructName);
			if (!Replacement)
			{
				OutResult.ErrorMessage = TEXT("replacement_root_construct_failed");
				Mutation.Rollback();
				return false;
			}
			Replacement->SetFlags(RF_Transactional);
			Tree->RootWidget = Replacement;
			FString RetireError;
			if (!FBlueprintHelperWidgetVersionCompat::RetireSourceWidget(WidgetBlueprint, RootWidget, RetireError))
			{
				OutResult.ErrorMessage = RetireError;
				Mutation.Rollback();
				return false;
			}
			if (bReuseRootName &&
				!Replacement->Rename(*Request.ReplacementWidgetName, Tree, REN_DontCreateRedirectors))
			{
				OutResult.ErrorMessage = TEXT("replacement_root_rename_failed");
				Mutation.Rollback();
				return false;
			}
			FBlueprintHelperWidgetVersionCompat::RegisterWidgetVariable(WidgetBlueprint, Replacement);
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
			Mutation.Commit();
			OutResult.AffectedWidget = Replacement->GetName();
		}
		else
		{
			OutResult.AffectedWidget = Request.ReplacementWidgetName;
		}

		OutResult.bSuccess = true;
		return true;
	}

	if (Policy == TEXT("remove_empty_root"))
	{
		TArray<UWidget*> Children;
		UWidgetTree::GetChildWidgets(RootWidget, Children);
		if (Children.Num() > 0)
		{
			OutResult.ErrorMessage = TEXT("root_remove_empty_requires_no_children");
			return false;
		}

		if (!Request.bDryRun)
		{
			FBlueprintHelperScopedAssetMutation Mutation(
				FText::FromString(TEXT("BlueprintHelper Remove Empty Root Widget")), WidgetBlueprint);
			Mutation.Modify(Tree);
			Tree->RootWidget = nullptr;
			FString RetireError;
			if (!FBlueprintHelperWidgetVersionCompat::RetireSourceWidget(WidgetBlueprint, RootWidget, RetireError))
			{
				OutResult.ErrorMessage = RetireError;
				Mutation.Rollback();
				return false;
			}
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
			Mutation.Commit();
		}

		OutResult.bSuccess = true;
		OutResult.AffectedWidget = Request.RootWidgetName;
		return true;
	}

	OutResult.ErrorMessage = TEXT("unsupported_root_removal_policy");
	return false;
}
