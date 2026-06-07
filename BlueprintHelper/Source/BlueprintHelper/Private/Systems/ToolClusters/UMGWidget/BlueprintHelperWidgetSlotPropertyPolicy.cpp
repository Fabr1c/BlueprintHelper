#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetSlotPropertyPolicy.h"

#include "Blueprint/WidgetTree.h"
#include "Components/PanelSlot.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Shared/BlueprintHelperServiceTypes.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/ToolClusters/ObjectProperty/BlueprintHelperPropertyReflectionService.h"
#include "WidgetBlueprint.h"

class FBlueprintHelperWidgetSlotPropertyPolicyLocalUtils
{
public:
	static void SetFailure(
		FBlueprintHelperWidgetMutationResult& OutResult,
		const FString& Code,
		const FString& Message)
	{
		OutResult.ErrorMessage = FString::Printf(TEXT("%s:%s"), *Code, *Message);
	}

	static bool DidImportConsumeAllText(const TCHAR* ImportEnd)
	{
		if (!ImportEnd)
		{
			return false;
		}

		while (*ImportEnd != TEXT('\0'))
		{
			if (!FChar::IsWhitespace(*ImportEnd))
			{
				return false;
			}
			++ImportEnd;
		}
		return true;
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
};

bool FBlueprintHelperWidgetSlotPropertyPolicy::Apply(
	UWidgetBlueprint* WidgetBlueprint,
	const FBlueprintHelperSetSlotPropertyRequest& Request,
	FBlueprintHelperWidgetMutationResult& OutResult)
{
	OutResult.bDryRun = Request.bDryRun;
	OutResult.AffectedWidget = Request.WidgetName;

	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		FBlueprintHelperWidgetSlotPropertyPolicyLocalUtils::SetFailure(
			OutResult,
			TEXT("widget_blueprint_not_found"),
			TEXT("WidgetBlueprint or WidgetTree is null."));
		return false;
	}

	UWidget* Widget = WidgetBlueprint->WidgetTree->FindWidget(FName(*Request.WidgetName));
	if (!Widget)
	{
		FBlueprintHelperWidgetSlotPropertyPolicyLocalUtils::SetFailure(
			OutResult,
			TEXT("widget_not_found"),
			FString::Printf(TEXT("Widget not found: %s"), *Request.WidgetName));
		return false;
	}

	UPanelSlot* Slot = Widget->Slot;
	if (!Slot)
	{
		FBlueprintHelperWidgetSlotPropertyPolicyLocalUtils::SetFailure(
			OutResult,
			TEXT("widget_slot_missing"),
			FString::Printf(TEXT("Widget has no slot: %s"), *Request.WidgetName));
		return false;
	}

	if (!FBlueprintHelperWidgetSlotPropertyPolicyLocalUtils::MatchesExpectedClass(
		Slot->GetClass(),
		Request.ExpectedSlotClassPath))
	{
		FBlueprintHelperWidgetSlotPropertyPolicyLocalUtils::SetFailure(
			OutResult,
			TEXT("slot_class_mismatch"),
			FString::Printf(
				TEXT("Expected slot class %s but found %s."),
				*Request.ExpectedSlotClassPath,
				*Slot->GetClass()->GetPathName()));
		return false;
	}

	FProperty* Property = nullptr;
	void* ValuePtr = nullptr;
	FString ExpectedType;
	FString ResolveErrorCode;
	FString ResolveErrorMessage;
	if (!FBlueprintHelperPropertyReflectionService::ResolvePropertyPath(
		Slot,
		Request.PropertyPath,
		Property,
		ValuePtr,
		ExpectedType,
		ResolveErrorCode,
		ResolveErrorMessage))
	{
		FBlueprintHelperWidgetSlotPropertyPolicyLocalUtils::SetFailure(
			OutResult,
			TEXT("slot_property_not_found"),
			ResolveErrorMessage);
		return false;
	}

	if (!FBlueprintHelperEditablePropertyPolicy::AllowsWrite(Property))
	{
		FBlueprintHelperWidgetSlotPropertyPolicyLocalUtils::SetFailure(
			OutResult,
			TEXT("slot_property_not_writable"),
			FString::Printf(
				TEXT("Slot property is not safely writable: %s. Flags: %s"),
				*Request.PropertyPath,
				*FBlueprintHelperEditablePropertyPolicy::BuildFlagsSummary(Property->PropertyFlags)));
		return false;
	}

	if (Request.bDryRun)
	{
		FString BeforeValue;
		Property->ExportText_Direct(BeforeValue, ValuePtr, nullptr, Slot, PPF_None);
		void* TempValue = FMemory_Alloca(Property->GetSize());
		Property->InitializeValue(TempValue);
		const TCHAR* ImportResult = Property->ImportText_Direct(*Request.Value, TempValue, Slot, PPF_None);
		Property->DestroyValue(TempValue);
		if (!FBlueprintHelperWidgetSlotPropertyPolicyLocalUtils::DidImportConsumeAllText(ImportResult))
		{
			FBlueprintHelperWidgetSlotPropertyPolicyLocalUtils::SetFailure(
				OutResult,
				TEXT("slot_property_import_failed"),
				FString::Printf(
					TEXT("Cannot import slot property %s value %s."),
					*Request.PropertyPath,
					*Request.Value));
			return false;
		}

		OutResult.ReadbackContext = MakeShared<FJsonObject>();
		OutResult.ReadbackContext->SetStringField(TEXT("target_kind"), TEXT("slot_property"));
		OutResult.ReadbackContext->SetStringField(TEXT("widget_name"), Request.WidgetName);
		OutResult.ReadbackContext->SetStringField(TEXT("slot_class_path"), Slot->GetClass()->GetPathName());
		OutResult.ReadbackContext->SetStringField(TEXT("property_path"), Request.PropertyPath);
		OutResult.ReadbackContext->SetStringField(TEXT("before_value"), BeforeValue);
		OutResult.ReadbackContext->SetStringField(TEXT("after_value"), Request.Value);
		OutResult.bSuccess = true;
		return true;
	}

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Set Widget Slot Property")),
		WidgetBlueprint);
	Mutation.Modify(Slot);

	FString OldValue;
	Property->ExportText_Direct(OldValue, ValuePtr, nullptr, Slot, PPF_None);
	const TCHAR* ImportResult = Property->ImportText_Direct(*Request.Value, ValuePtr, Slot, PPF_None);
	if (!FBlueprintHelperWidgetSlotPropertyPolicyLocalUtils::DidImportConsumeAllText(ImportResult))
	{
		Property->ImportText_Direct(*OldValue, ValuePtr, Slot, PPF_None);
		Mutation.Rollback();
		FBlueprintHelperWidgetSlotPropertyPolicyLocalUtils::SetFailure(
			OutResult,
			TEXT("slot_property_import_failed"),
			FString::Printf(
				TEXT("Cannot import slot property %s value %s."),
				*Request.PropertyPath,
				*Request.Value));
		return false;
	}

	FString NewValue;
	Property->ExportText_Direct(NewValue, ValuePtr, nullptr, Slot, PPF_None);
	OutResult.ReadbackContext = MakeShared<FJsonObject>();
	OutResult.ReadbackContext->SetStringField(TEXT("target_kind"), TEXT("slot_property"));
	OutResult.ReadbackContext->SetStringField(TEXT("widget_name"), Request.WidgetName);
	OutResult.ReadbackContext->SetStringField(TEXT("slot_class_path"), Slot->GetClass()->GetPathName());
	OutResult.ReadbackContext->SetStringField(TEXT("property_path"), Request.PropertyPath);
	OutResult.ReadbackContext->SetStringField(TEXT("before_value"), OldValue);
	OutResult.ReadbackContext->SetStringField(TEXT("after_value"), NewValue);

	Slot->SynchronizeProperties();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	Mutation.Commit();

	OutResult.bSuccess = true;
	return true;
}
