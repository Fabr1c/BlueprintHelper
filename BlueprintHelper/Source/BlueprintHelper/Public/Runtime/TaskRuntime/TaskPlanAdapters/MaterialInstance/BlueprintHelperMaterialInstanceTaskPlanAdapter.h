// BlueprintHelper MaterialInstance TaskPlan lowering adapter.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

struct FBlueprintHelperTaskRuntimeLoweredStep;

struct BLUEPRINTHELPER_API FBlueprintHelperMaterialInstanceTaskPlanPayload
{
	FString StepId;
	FString Capability;
	FString RuntimeOperation;
	FString AdapterOperation;
	TSharedPtr<FJsonObject> Payload;
	bool bAdapterDryRunSupported = true;
};

class BLUEPRINTHELPER_API FBlueprintHelperMaterialInstanceTaskPlanAdapter
{
public:
	static constexpr const TCHAR* CapabilityMaterialInstance = TEXT("material_instance");
	static constexpr const TCHAR* RuntimeOperationMaterialInstance = TEXT("material_instance");
	static constexpr const TCHAR* StrategyMaterialInstanceEdit = TEXT("material_instance_edit");
	static constexpr const TCHAR* AdapterOperationMaterialInstanceEdit = TEXT("material_instance_edit");

	static bool TryBuildPayloadFromTaskPlanStep(
		const TSharedPtr<FJsonObject>& StepObject,
		bool bDryRun,
		FBlueprintHelperMaterialInstanceTaskPlanPayload& OutPayload,
		FBlueprintHelperToolError& OutError);

	static bool TryLowerTaskPlanStep(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const TSharedPtr<FJsonObject>& StepObject,
		bool bDryRun,
		FBlueprintHelperTaskRuntimeLoweredStep& OutLoweredStep,
		FBlueprintHelperToolError& OutError);
};
