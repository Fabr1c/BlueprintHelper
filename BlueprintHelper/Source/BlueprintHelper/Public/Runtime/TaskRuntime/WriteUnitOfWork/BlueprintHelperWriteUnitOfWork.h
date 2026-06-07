#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/WriteContracts/BlueprintHelperAcceptedPayloadModel.h"
#include "Runtime/TaskRuntime/WriteContracts/BlueprintHelperWriteFamilyDescriptor.h"
#include "Shared/BlueprintHelperToolResultTypes.h"

enum class EBlueprintHelperWriteUnitOfWorkMode : uint8
{
	Preview,
	Execute
};

enum class EBlueprintHelperWriteUnitOfWorkStage : uint8
{
	CaptureBefore,
	BuildFamilyMutationPlan,
	ApplyMutation,
	ProjectDryRun,
	RecordOwnershipDelta,
	Commit,
	Rollback,
	BuildDiagnostics
};

struct BLUEPRINTHELPER_API FBlueprintHelperWriteUnitOfWorkRequest
{
	EBlueprintHelperWriteUnitOfWorkMode Mode = EBlueprintHelperWriteUnitOfWorkMode::Preview;
	FBlueprintHelperAcceptedPayloadModel AcceptedPayload;
	FBlueprintHelperWriteFamilyDescriptor Descriptor;
	TFunction<FBlueprintHelperToolResultBase()> CaptureBefore;
	TFunction<FBlueprintHelperToolResultBase()> BuildFamilyMutationPlan;
	TFunction<FBlueprintHelperToolResultBase()> ApplyMutation;
	TFunction<FBlueprintHelperToolResultBase()> ProjectDryRun;
	TFunction<FBlueprintHelperToolResultBase()> RecordOwnershipDelta;
	TFunction<FBlueprintHelperToolResultBase()> Commit;
	TFunction<FBlueprintHelperToolResultBase()> Rollback;
	TFunction<FBlueprintHelperToolResultBase()> BuildDiagnostics;
};

struct BLUEPRINTHELPER_API FBlueprintHelperWriteUnitOfWorkResult
{
	FBlueprintHelperToolResultBase ToolResult;
	TArray<FString> StageTrace;
	FString WriteFamily;
	FString RuntimeAdapterId;
	FString CommitState;
	FString RollbackState;
	bool bRolledBack = false;
};

class BLUEPRINTHELPER_API FBlueprintHelperWriteUnitOfWork
{
public:
	static FBlueprintHelperWriteUnitOfWorkResult Run(
		const FBlueprintHelperWriteUnitOfWorkRequest& Request);

	static FString StageToString(EBlueprintHelperWriteUnitOfWorkStage Stage);
	static void AttachUnitOfWorkData(
		FBlueprintHelperToolResultBase& ToolResult,
		const FBlueprintHelperWriteUnitOfWorkRequest& Request,
		const FBlueprintHelperWriteUnitOfWorkResult& Result);
};
