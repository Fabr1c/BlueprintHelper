// BlueprintHelper TaskPlan adapter - UMG Widget Blueprint cluster.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FBlueprintHelperWidgetTaskPlan
{
public:
	struct Capability
	{
		static constexpr const TCHAR* UMGWidget = TEXT("umg_widget");
	};

	struct Strategy
	{
		static constexpr const TCHAR* WidgetTreeEdit = TEXT("widget_tree_edit");
		static constexpr const TCHAR* WidgetPropertyEdit = TEXT("widget_property_edit");
		static constexpr const TCHAR* WidgetBlueprintClassEdit = TEXT("widget_blueprint_class_edit");
	};

	struct Op
	{
		static constexpr const TCHAR* AddWidget = TEXT("add_widget");
		static constexpr const TCHAR* MoveWidget = TEXT("move_widget");
		static constexpr const TCHAR* SetNamedSlotContent = TEXT("set_named_slot_content");
		static constexpr const TCHAR* SetWidgetProperty = TEXT("set_widget_property");
		static constexpr const TCHAR* SetSlotProperty = TEXT("set_slot_property");
		static constexpr const TCHAR* SetWidgetAsVariable = TEXT("set_widget_as_variable");
		static constexpr const TCHAR* RemoveWidget = TEXT("remove_widget");
		static constexpr const TCHAR* RenameWidget = TEXT("rename_widget");
		static constexpr const TCHAR* RemoveRootWidget = TEXT("remove_root_widget");
		static constexpr const TCHAR* ReparentWidgetBlueprint = TEXT("reparent_widget_blueprint");
		static constexpr const TCHAR* DuplicateWidgetSubtree = TEXT("duplicate_widget_subtree");
		static constexpr const TCHAR* WrapWidget = TEXT("wrap_widget");
		static constexpr const TCHAR* ReplaceWidgetClass = TEXT("replace_widget_class");
	};

	struct AdapterOperation
	{
		static constexpr const TCHAR* AddWidget = TEXT("add_widget");
		static constexpr const TCHAR* MoveWidget = TEXT("move_widget");
		static constexpr const TCHAR* SetNamedSlotContent = TEXT("set_named_slot_content");
		static constexpr const TCHAR* SetWidgetProperty = TEXT("set_widget_property");
		static constexpr const TCHAR* SetSlotProperty = TEXT("set_slot_property");
		static constexpr const TCHAR* SetWidgetAsVariable = TEXT("set_widget_as_variable");
		static constexpr const TCHAR* RemoveWidget = TEXT("remove_widget");
		static constexpr const TCHAR* RenameWidget = TEXT("rename_widget");
		static constexpr const TCHAR* RemoveRootWidget = TEXT("remove_root_widget");
		static constexpr const TCHAR* ReparentWidgetBlueprint = TEXT("reparent_widget_blueprint");
		static constexpr const TCHAR* DuplicateWidgetSubtree = TEXT("duplicate_widget_subtree");
		static constexpr const TCHAR* WrapWidget = TEXT("wrap_widget");
		static constexpr const TCHAR* ReplaceWidgetClass = TEXT("replace_widget_class");
	};
};

struct BLUEPRINTHELPER_API FBlueprintHelperWidgetTaskPlanLoweredOp
{
	FString AdapterOperation;
	TSharedPtr<FJsonObject> Payload;
};

struct BLUEPRINTHELPER_API FBlueprintHelperWidgetTaskPlanLoweredStep
{
	FString StepId;
	FString Capability;
	FString RuntimeOperation;
	TArray<FBlueprintHelperWidgetTaskPlanLoweredOp> Ops;

	bool bAdapterDryRunSupported = true;
};

struct BLUEPRINTHELPER_API FBlueprintHelperWidgetTaskPlanPayload
{
	FString Capability;
	FString RuntimeOperation;
	FString AdapterOperation;
	TSharedPtr<FJsonObject> Payload;
	bool bAdapterDryRunSupported = true;
};

class BLUEPRINTHELPER_API FBlueprintHelperWidgetTaskPlanAdapter
{
public:
	static bool SupportsStep(const TSharedPtr<FJsonObject>& StepObject);

	static bool TryLowerTaskPlanStep(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const TSharedPtr<FJsonObject>& StepObject,
		bool bDryRun,
		FBlueprintHelperWidgetTaskPlanLoweredStep& OutLoweredStep,
		FBlueprintHelperToolError& OutError);

	static bool TryBuildPayloadFromTaskPlanStep(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const TSharedPtr<FJsonObject>& StepObject,
		bool bDryRun,
		FBlueprintHelperWidgetTaskPlanPayload& OutPayload,
		FBlueprintHelperToolError& OutError);
};
