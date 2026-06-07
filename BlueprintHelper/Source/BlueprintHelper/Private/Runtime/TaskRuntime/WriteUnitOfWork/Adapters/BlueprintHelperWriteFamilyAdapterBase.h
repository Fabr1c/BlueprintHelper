#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/WriteUnitOfWork/BlueprintHelperWriteFamilyAdapter.h"

class FBlueprintHelperWriteFamilyAdapterBase : public IBlueprintHelperWriteFamilyAdapter
{
public:
	explicit FBlueprintHelperWriteFamilyAdapterBase(const FString& InWriteFamily);

	FString GetWriteFamily() const override;
	FString GetRuntimeAdapterId() const override;

	bool BuildPreflight(
		const FBlueprintHelperAcceptedPayloadModel& AcceptedPayload,
		FBlueprintHelperWriteUnitOfWorkRequest& OutRequest,
		FBlueprintHelperToolError& OutError) const override;
	bool BuildMutationPlan(
		const FBlueprintHelperAcceptedPayloadModel& AcceptedPayload,
		FBlueprintHelperWriteUnitOfWorkRequest& OutRequest,
		FBlueprintHelperToolError& OutError) const override;
	bool BuildDryRunProjection(
		const FBlueprintHelperAcceptedPayloadModel& AcceptedPayload,
		FBlueprintHelperWriteUnitOfWorkRequest& OutRequest,
		FBlueprintHelperToolError& OutError) const override;
	bool BuildReviewAndReadback(
		const FBlueprintHelperAcceptedPayloadModel& AcceptedPayload,
		FBlueprintHelperWriteUnitOfWorkRequest& OutRequest,
		FBlueprintHelperToolError& OutError) const override;

protected:
	const FBlueprintHelperWriteFamilyDescriptor& GetDescriptor() const;

private:
	bool ValidateDescriptor(FBlueprintHelperToolError& OutError) const;
	bool ValidateAcceptedPayload(
		const FBlueprintHelperAcceptedPayloadModel& AcceptedPayload,
		FBlueprintHelperToolError& OutError) const;

	FBlueprintHelperWriteFamilyDescriptor Descriptor;
	bool bDescriptorResolved = false;
};
