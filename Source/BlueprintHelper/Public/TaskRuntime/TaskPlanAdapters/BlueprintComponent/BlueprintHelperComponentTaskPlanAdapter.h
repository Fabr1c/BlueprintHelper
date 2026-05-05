// BlueprintHelper TaskPlan adapter - Blueprint Component cluster.

#pragma once

#include "CoreMinimal.h"
#include "Structure/BlueprintHelperToolResultTypes.h"

class FJsonObject;

struct BLUEPRINTHELPER_API FBlueprintHelperComponentTaskPlanPayload
{
	FString Capability;
	FString RuntimeOperation;
	FString AdapterOperation;
	TSharedPtr<FJsonObject> Payload;

	// Current component service payloads do not implement true dry-run handling.
	bool bAdapterDryRunSupported = false;
};

class BLUEPRINTHELPER_API FBlueprintHelperComponentTaskPlanAdapter
{
public:
	static const TCHAR* CapabilityBlueprintComponent;
	static const TCHAR* StrategyComponentTree;

	static const TCHAR* RuntimeOperationBlueprintComponent;
	static const TCHAR* AdapterOperationAddComponent;
	static const TCHAR* AdapterOperationSetComponentProperties;
	static const TCHAR* AdapterOperationRemoveComponent;

	static const TCHAR* OpAddComponent;
	static const TCHAR* OpSetComponentProperties;
	static const TCHAR* OpRemoveComponent;

	static bool TryBuildPayloadFromTaskPlanStep(
		const TSharedPtr<FJsonObject>& StepObject,
		bool bDryRun,
		FBlueprintHelperComponentTaskPlanPayload& OutPayload,
		FBlueprintHelperToolError& OutError);
};
