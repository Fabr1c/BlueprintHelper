// BlueprintHelper TaskPlan adapter - Blueprint Variable cluster.

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class FJsonObject;

struct BLUEPRINTHELPER_API FBlueprintHelperBlueprintVariableTaskPlanPayload
{
	FString Capability;
	FString RuntimeOperation;
	FString AdapterOperation;
	TSharedPtr<FJsonObject> Payload;

	bool bAdapterDryRunSupported = false;
};

class BLUEPRINTHELPER_API FBlueprintHelperBlueprintVariableTaskPlanAdapter
{
public:
	static const TCHAR* CapabilityBlueprintVariable;
	static const TCHAR* RuntimeOperationBlueprintVariable;

	static const TCHAR* StrategyMemberVariables;
	static const TCHAR* StrategyMemberDefaults;
	static const TCHAR* StrategyLocalVariables;

	static const TCHAR* AdapterOperationAddMemberVariables;
	static const TCHAR* AdapterOperationVariableBatch;

	static const TCHAR* OpEnsureMemberVariable;
	static const TCHAR* OpSetMemberVariableProperties;
	static const TCHAR* OpRemoveMemberVariable;
	static const TCHAR* OpSetMemberDefault;
	static const TCHAR* OpEnsureLocalVariable;
	static const TCHAR* OpSetLocalVariableProperties;
	static const TCHAR* OpRemoveLocalVariable;

	static bool TryBuildPayloadFromTaskPlanStep(
		const TSharedPtr<FJsonObject>& StepObject,
		bool bDryRun,
		FBlueprintHelperBlueprintVariableTaskPlanPayload& OutPayload,
		FBlueprintHelperToolError& OutError);
};
