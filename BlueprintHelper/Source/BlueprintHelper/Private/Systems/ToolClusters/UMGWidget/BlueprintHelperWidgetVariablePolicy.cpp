#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetVariablePolicy.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Shared/BlueprintHelperWidgetVersionCompat.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "WidgetBlueprint.h"

class FBlueprintHelperWidgetVariablePolicyLocalUtils
{
public:
	static void SetFailure(
		FBlueprintHelperWidgetMutationResult& OutResult,
		const FString& Code,
		const FString& Message)
	{
		OutResult.ErrorMessage = FString::Printf(TEXT("%s:%s"), *Code, *Message);
	}

	static bool MatchesExpectedClass(const UClass* ActualClass, const FString& ExpectedClassPath)
	{
		if (!ActualClass || ExpectedClassPath.IsEmpty())
		{
			return true;
		}

		return ActualClass->GetPathName().Equals(ExpectedClassPath, ESearchCase::IgnoreCase)
			|| ActualClass->GetName().Equals(ExpectedClassPath, ESearchCase::IgnoreCase);
	}

	static bool HasSourceGuid(UWidgetBlueprint* WidgetBlueprint, const UWidget* Widget)
	{
		return FBlueprintHelperWidgetVersionCompat::HasWidgetSourceGuid(WidgetBlueprint, Widget);
	}

	static void ApplyVariableState(UWidgetBlueprint* WidgetBlueprint, UWidget* Widget, bool bIsVariable)
	{
		FBlueprintHelperWidgetVersionCompat::SetWidgetVariableState(WidgetBlueprint, Widget, bIsVariable);
	}
};

bool FBlueprintHelperWidgetVariablePolicy::Apply(
	UWidgetBlueprint* WidgetBlueprint,
	const FBlueprintHelperSetWidgetAsVariableRequest& Request,
	FBlueprintHelperWidgetMutationResult& OutResult)
{
	OutResult.bDryRun = Request.bDryRun;
	OutResult.AffectedWidget = Request.WidgetName;

	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		FBlueprintHelperWidgetVariablePolicyLocalUtils::SetFailure(
			OutResult,
			TEXT("widget_blueprint_not_found"),
			TEXT("WidgetBlueprint or WidgetTree is null."));
		return false;
	}

	UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(FName(*Request.WidgetName));
	if (!Widget)
	{
		FBlueprintHelperWidgetVariablePolicyLocalUtils::SetFailure(
			OutResult,
			TEXT("widget_not_found"),
			FString::Printf(TEXT("Widget not found: %s"), *Request.WidgetName));
		return false;
	}

	if (!FBlueprintHelperWidgetVariablePolicyLocalUtils::MatchesExpectedClass(
		Widget->GetClass(),
		Request.ExpectedWidgetClassPath))
	{
		FBlueprintHelperWidgetVariablePolicyLocalUtils::SetFailure(
			OutResult,
			TEXT("widget_class_mismatch"),
			FString::Printf(
				TEXT("Expected widget class %s but found %s."),
				*Request.ExpectedWidgetClassPath,
				*Widget->GetClass()->GetPathName()));
		return false;
	}

	const bool bBeforeIsVariable = Widget->bIsVariable;
	const bool bBeforeGuidValid = FBlueprintHelperWidgetVariablePolicyLocalUtils::HasSourceGuid(
		WidgetBlueprint,
		Widget);

	if (Request.bDryRun)
	{
		OutResult.ReadbackContext = MakeShared<FJsonObject>();
		OutResult.ReadbackContext->SetStringField(TEXT("target_kind"), TEXT("widget_variable"));
		OutResult.ReadbackContext->SetStringField(TEXT("widget_name"), Request.WidgetName);
		OutResult.ReadbackContext->SetBoolField(TEXT("before_is_variable"), bBeforeIsVariable);
		OutResult.ReadbackContext->SetBoolField(TEXT("after_is_variable"), Request.bIsVariable);
		OutResult.ReadbackContext->SetStringField(TEXT("variable_guid_state"), bBeforeGuidValid ? TEXT("valid") : TEXT("pending"));
		OutResult.bSuccess = true;
		return true;
	}

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Set Widget Variable State")),
		WidgetBlueprint);
	Mutation.Modify(Widget);

	FBlueprintHelperWidgetVariablePolicyLocalUtils::ApplyVariableState(
		WidgetBlueprint,
		Widget,
		Request.bIsVariable);

	const bool bAfterGuidValid = FBlueprintHelperWidgetVariablePolicyLocalUtils::HasSourceGuid(
		WidgetBlueprint,
		Widget);
	if (Widget->bIsVariable != Request.bIsVariable || !bAfterGuidValid)
	{
		FBlueprintHelperWidgetVariablePolicyLocalUtils::ApplyVariableState(
			WidgetBlueprint,
			Widget,
			bBeforeIsVariable);
		Widget->bIsVariable = bBeforeIsVariable;
		Mutation.Rollback();
		FBlueprintHelperWidgetVariablePolicyLocalUtils::SetFailure(
			OutResult,
			TEXT("widget_variable_guid_update_failed"),
			FString::Printf(
				TEXT("Widget variable GUID state did not match requested state for %s."),
				*Request.WidgetName));
		return false;
	}

	OutResult.ReadbackContext = MakeShared<FJsonObject>();
	OutResult.ReadbackContext->SetStringField(TEXT("target_kind"), TEXT("widget_variable"));
	OutResult.ReadbackContext->SetStringField(TEXT("widget_name"), Request.WidgetName);
	OutResult.ReadbackContext->SetBoolField(TEXT("before_is_variable"), bBeforeIsVariable);
	OutResult.ReadbackContext->SetBoolField(TEXT("after_is_variable"), Widget->bIsVariable);
	OutResult.ReadbackContext->SetStringField(TEXT("variable_guid_state"), bAfterGuidValid ? TEXT("valid") : TEXT("invalid"));

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	Mutation.Commit();

	OutResult.bSuccess = true;
	return true;
}
