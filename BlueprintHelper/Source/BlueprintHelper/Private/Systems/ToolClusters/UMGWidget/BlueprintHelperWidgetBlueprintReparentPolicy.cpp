#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetBlueprintReparentPolicy.h"

#include "Blueprint/UserWidget.h"
#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassSettingsService.h"
#include "WidgetBlueprint.h"

class FBlueprintHelperWidgetBlueprintReparentPolicyLocalUtils
{
public:
	static UClass* ResolveUserWidgetParentClass(const FString& ClassPath, FString& OutError)
	{
		if (ClassPath.IsEmpty())
		{
			OutError = TEXT("new_parent_class_required");
			return nullptr;
		}

		TArray<FString> CandidatePaths;
		CandidatePaths.Add(ClassPath);
		if (!ClassPath.StartsWith(TEXT("/")) && !ClassPath.Contains(TEXT(".")))
		{
			CandidatePaths.Add(FString::Printf(TEXT("/Script/UMG.%s"), *ClassPath));
		}

		for (const FString& CandidatePath : CandidatePaths)
		{
			if (UClass* Class = LoadObject<UClass>(nullptr, *CandidatePath))
			{
				if (Class->IsChildOf(UUserWidget::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
				{
					return Class;
				}
			}
		}

		OutError = FString::Printf(TEXT("new_parent_class must resolve to a concrete UUserWidget-derived class: %s"), *ClassPath);
		return nullptr;
	}

	static FString ReadToolResultError(const FBlueprintHelperToolResultBase& ToolResult)
	{
		if (ToolResult.Error.IsSet())
		{
			return ToolResult.Error->Code.IsEmpty()
				? ToolResult.Error->Message
				: FString::Printf(TEXT("%s:%s"), *ToolResult.Error->Code, *ToolResult.Error->Message);
		}
		return TEXT("widget_blueprint_reparent_failed");
	}
};

bool FBlueprintHelperWidgetBlueprintReparentPolicy::Apply(
	UWidgetBlueprint* WidgetBlueprint,
	const FBlueprintHelperReparentWidgetBlueprintRequest& Request,
	const FBlueprintHelperClassSettingsService* ClassSettingsService,
	FBlueprintHelperWidgetMutationResult& OutResult)
{
	OutResult.bDryRun = Request.bDryRun;
	if (!WidgetBlueprint)
	{
		OutResult.ErrorMessage = TEXT("widget_blueprint_missing");
		return false;
	}
	if (!ClassSettingsService)
	{
		OutResult.ErrorMessage = TEXT("class_settings_service_unavailable");
		return false;
	}

	const FString CurrentParent = WidgetBlueprint->ParentClass
		? WidgetBlueprint->ParentClass->GetPathName()
		: FString();
	if (!Request.ExpectedParentClass.IsEmpty() &&
		!CurrentParent.Equals(Request.ExpectedParentClass, ESearchCase::IgnoreCase))
	{
		OutResult.ErrorMessage = TEXT("widget_blueprint_parent_class_mismatch");
		return false;
	}

	FString ParentError;
	if (!FBlueprintHelperWidgetBlueprintReparentPolicyLocalUtils::ResolveUserWidgetParentClass(
		Request.NewParentClass,
		ParentError))
	{
		OutResult.ErrorMessage = ParentError;
		return false;
	}

	const FBlueprintHelperToolResultBase ToolResult = ClassSettingsService->ReparentBlueprint(
		Request.AssetPath,
		Request.NewParentClass,
		Request.bDryRun);
	if (!ToolResult.bOk)
	{
		OutResult.ErrorMessage =
			FBlueprintHelperWidgetBlueprintReparentPolicyLocalUtils::ReadToolResultError(ToolResult);
		return false;
	}

	OutResult.bSuccess = true;
	OutResult.AffectedWidget = WidgetBlueprint->GetName();
	OutResult.ReadbackContext = ToolResult.Data;
	return true;
}
