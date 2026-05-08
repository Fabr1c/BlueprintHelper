// BlueprintHelper TaskPlan adapter - UMG Widget Blueprint cluster.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

namespace BlueprintHelperWidgetTaskPlan
{
	namespace Capability
	{
		static constexpr const TCHAR* UMGWidget = TEXT("umg_widget");
	}

	namespace Strategy
	{
		static constexpr const TCHAR* WidgetTreeEdit = TEXT("widget_tree_edit");
		static constexpr const TCHAR* WidgetPropertyEdit = TEXT("widget_property_edit");
	}

	namespace Op
	{
		static constexpr const TCHAR* AddWidget = TEXT("add_widget");
		static constexpr const TCHAR* SetWidgetProperty = TEXT("set_widget_property");
		static constexpr const TCHAR* RemoveWidget = TEXT("remove_widget");
	}

	namespace AdapterOperation
	{
		static constexpr const TCHAR* AddWidget = TEXT("add_widget");
		static constexpr const TCHAR* SetWidgetProperty = TEXT("set_widget_property");
		static constexpr const TCHAR* RemoveWidget = TEXT("remove_widget");
	}
}

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
