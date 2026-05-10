// BlueprintHelper TaskPlan adapter - Cleanup / rollback / ownership lifecycle capability.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

struct FBlueprintHelperTaskRuntimeLoweredStep;

struct BLUEPRINTHELPER_API FBlueprintHelperCleanupOwnershipTaskPlanPayload
{
	FString StepId;
	FString Capability;
	FString RuntimeOperation;
	FString AdapterOperation;
	TSharedPtr<FJsonObject> Payload;
	bool bAdapterDryRunSupported = true;
};

class BLUEPRINTHELPER_API FBlueprintHelperCleanupOwnershipTaskPlanAdapter
{
public:
	static constexpr const TCHAR* CapabilityName = TEXT("graph_cleanup_ownership");
	static constexpr const TCHAR* RuntimeOperationName = TEXT("graph_cleanup_ownership");
	static constexpr const TCHAR* StrategyOwnedBlockLifecycle = TEXT("owned_block_lifecycle");

	static constexpr const TCHAR* AdapterOperationCleanupBlueprintHelperBlock = TEXT("cleanup_blueprint_helper_block");
	static constexpr const TCHAR* AdapterOperationConvertBlueprintHelperBlockToUserOwned = TEXT("convert_blueprint_helper_block_to_user_owned");
	static constexpr const TCHAR* AdapterOperationRollbackCleanupTransaction = TEXT("rollback_cleanup_transaction");

	static bool IsSupportedCapability(const FString& Capability);
	static bool IsSupportedOp(const FString& OpName);

	static bool TryBuildPayloadFromTaskPlanStep(
		const TSharedPtr<FJsonObject>& StepObject,
		bool bDryRun,
		FBlueprintHelperCleanupOwnershipTaskPlanPayload& OutPayload,
		FBlueprintHelperToolError& OutError);

	static bool TryLowerTaskPlanStep(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const TSharedPtr<FJsonObject>& StepObject,
		bool bDryRun,
		FBlueprintHelperTaskRuntimeLoweredStep& OutLoweredStep,
		FBlueprintHelperToolError& OutError);
};
