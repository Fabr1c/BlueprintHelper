// BlueprintHelper TaskPlan adapter - Blueprint Class Settings capability.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Structure/BlueprintHelperToolResultTypes.h"

struct FBlueprintHelperTaskRuntimeLoweredStep;

class BLUEPRINTHELPER_API FBlueprintHelperClassSettingsTaskPlanAdapter
{
public:
	static constexpr const TCHAR* CapabilityName = TEXT("blueprint_class_settings");
	static constexpr const TCHAR* RuntimeOperationName = TEXT("blueprint_class_settings");
	static constexpr const TCHAR* StrategyName = TEXT("class_settings");

	static constexpr const TCHAR* AddImplementedInterfacesOp = TEXT("add_implemented_interfaces");
	static constexpr const TCHAR* RemoveImplementedInterfacesOp = TEXT("remove_implemented_interfaces");
	static constexpr const TCHAR* SetClassDefaultPropertiesOp = TEXT("set_class_default_properties");

	static bool IsSupportedCapability(const FString& Capability);
	static bool IsSupportedOp(const FString& OpName);
	static bool IsParentClassOp(const FString& OpName);

	static bool TryBuildAdapterPayload(
		const TSharedPtr<FJsonObject>& StepObject,
		TSharedPtr<FJsonObject>& OutPayload,
		FString& OutAdapterOperation,
		FBlueprintHelperToolError& OutError);

	static bool TryLowerTaskPlanStep(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const TSharedPtr<FJsonObject>& StepObject,
		bool bDryRun,
		FBlueprintHelperTaskRuntimeLoweredStep& OutLoweredStep,
		FBlueprintHelperToolError& OutError);
};
