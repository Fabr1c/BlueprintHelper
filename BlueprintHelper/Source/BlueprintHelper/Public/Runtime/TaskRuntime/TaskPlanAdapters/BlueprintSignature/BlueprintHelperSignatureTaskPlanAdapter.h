// BlueprintHelper TaskPlan adapter - Blueprint Function/Event signature capability.

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FJsonObject;
struct FBlueprintHelperTaskRuntimeLoweredStep;

struct BLUEPRINTHELPER_API FBlueprintHelperSignatureTaskPlanPayload
{
	FString Capability;
	FString RuntimeOperation;
	FString AdapterOperation;
	TSharedPtr<FJsonObject> Payload;
	bool bAdapterDryRunSupported = true;
};

class BLUEPRINTHELPER_API FBlueprintHelperSignatureTaskPlanAdapter
{
public:
	static constexpr const TCHAR* CapabilityName = TEXT("blueprint_signature");
	static constexpr const TCHAR* RuntimeOperationName = TEXT("blueprint_signature");
	static constexpr const TCHAR* StrategyFunctionSignature = TEXT("function_signature");
	static constexpr const TCHAR* StrategyCustomEventSignature = TEXT("custom_event_signature");
	static constexpr const TCHAR* StrategyMacroSignature = TEXT("macro_signature");
	static constexpr const TCHAR* StrategyEventDispatcherSignature = TEXT("event_dispatcher_signature");
	static constexpr const TCHAR* StrategyOverrideEventSignature = TEXT("override_event_signature");
	static constexpr const TCHAR* AdapterOperationEnsureFunction = TEXT("ensure_function");
	static constexpr const TCHAR* AdapterOperationEnsureCustomEvent = TEXT("ensure_custom_event");
	static constexpr const TCHAR* AdapterOperationEnsureMacro = TEXT("ensure_macro");
	static constexpr const TCHAR* AdapterOperationRemoveSignature = TEXT("remove_signature");
	static constexpr const TCHAR* AdapterOperationEnsureEventDispatcher = TEXT("ensure_event_dispatcher");
	static constexpr const TCHAR* AdapterOperationEnsureOverrideEvent = TEXT("ensure_override_event");

	static bool TryLowerTaskPlanStep(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const TSharedPtr<FJsonObject>& StepObject,
		bool bDryRun,
		FBlueprintHelperTaskRuntimeLoweredStep& OutLoweredStep,
		FBlueprintHelperToolError& OutError);
};
