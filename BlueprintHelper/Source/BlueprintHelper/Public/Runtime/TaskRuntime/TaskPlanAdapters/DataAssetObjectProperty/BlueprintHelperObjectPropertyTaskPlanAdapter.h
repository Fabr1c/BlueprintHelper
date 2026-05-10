// BlueprintHelper TaskPlan adapter - UObject reflected property cluster.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

struct FBlueprintHelperTaskRuntimeLoweredStep;

struct BLUEPRINTHELPER_API FBlueprintHelperObjectPropertyTaskPlanPayload
{
	FString StepId;
	FString Capability;
	FString RuntimeOperation;
	FString AdapterOperation;
	TSharedPtr<FJsonObject> Payload;
	bool bAdapterDryRunSupported = true;
};

class BLUEPRINTHELPER_API FBlueprintHelperObjectPropertyTaskPlanAdapter
{
public:
	static constexpr const TCHAR* CapabilityObjectProperty = TEXT("object_property");
	static constexpr const TCHAR* RuntimeOperationObjectProperty = TEXT("object_property");
	static constexpr const TCHAR* StrategyPropertyEdit = TEXT("property_edit");

	static constexpr const TCHAR* OpSetObjectProperty = TEXT("set_object_property");
	static constexpr const TCHAR* OpSetObjectProperties = TEXT("set_object_properties");

	static constexpr const TCHAR* AdapterOperationSetObjectProperty = TEXT("set_object_property");
	static constexpr const TCHAR* AdapterOperationSetObjectProperties = TEXT("set_object_properties");

	static bool SupportsStep(const TSharedPtr<FJsonObject>& StepObject);

	static bool TryBuildPayloadFromTaskPlanStep(
		const TSharedPtr<FJsonObject>& StepObject,
		bool bDryRun,
		FBlueprintHelperObjectPropertyTaskPlanPayload& OutPayload,
		FBlueprintHelperToolError& OutError);

	static bool TryLowerTaskPlanStep(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const TSharedPtr<FJsonObject>& StepObject,
		bool bDryRun,
		FBlueprintHelperTaskRuntimeLoweredStep& OutLoweredStep,
		FBlueprintHelperToolError& OutError);
};
