#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/WriteContracts/BlueprintHelperAcceptedPayloadModel.h"
#include "Runtime/TaskRuntime/WriteUnitOfWork/BlueprintHelperWriteUnitOfWork.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

class BLUEPRINTHELPER_API IBlueprintHelperWriteFamilyAdapter
{
public:
	virtual ~IBlueprintHelperWriteFamilyAdapter() = default;
	virtual FString GetWriteFamily() const = 0;
	virtual FString GetRuntimeAdapterId() const = 0;
	virtual bool BuildPreflight(
		const FBlueprintHelperAcceptedPayloadModel& AcceptedPayload,
		FBlueprintHelperWriteUnitOfWorkRequest& OutRequest,
		FBlueprintHelperToolError& OutError) const = 0;
	virtual bool BuildMutationPlan(
		const FBlueprintHelperAcceptedPayloadModel& AcceptedPayload,
		FBlueprintHelperWriteUnitOfWorkRequest& OutRequest,
		FBlueprintHelperToolError& OutError) const = 0;
	virtual bool BuildDryRunProjection(
		const FBlueprintHelperAcceptedPayloadModel& AcceptedPayload,
		FBlueprintHelperWriteUnitOfWorkRequest& OutRequest,
		FBlueprintHelperToolError& OutError) const = 0;
	virtual bool BuildReviewAndReadback(
		const FBlueprintHelperAcceptedPayloadModel& AcceptedPayload,
		FBlueprintHelperWriteUnitOfWorkRequest& OutRequest,
		FBlueprintHelperToolError& OutError) const = 0;
};
