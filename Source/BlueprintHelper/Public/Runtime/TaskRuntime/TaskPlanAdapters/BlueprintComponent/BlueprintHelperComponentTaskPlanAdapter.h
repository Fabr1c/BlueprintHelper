// BlueprintHelper TaskPlan adapter - Blueprint Component cluster.

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FJsonObject;

struct BLUEPRINTHELPER_API FBlueprintHelperComponentTaskPlanPayload
{
	FString Capability;
	FString RuntimeOperation;
	FString AdapterOperation;
	TSharedPtr<FJsonObject> Payload;

	bool bAdapterDryRunSupported = true;
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
