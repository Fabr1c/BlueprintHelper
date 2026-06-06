#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyBoundaryModel.h"

enum class EBlueprintHelperGraphWriteUnitOfWorkMode : uint8
{
	Preview,
	Execute
};

enum class EBlueprintHelperGraphWriteUnitOfWorkStage : uint8
{
	CaptureBefore,
	BuildMutationPlan,
	ApplyMutation,
	ProjectPreview,
	WriteOwnershipDelta,
	Commit,
	Rollback,
	BuildDiagnostics
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphWriteUnitOfWorkRequest
{
	EBlueprintHelperGraphWriteUnitOfWorkMode Mode = EBlueprintHelperGraphWriteUnitOfWorkMode::Preview;
	FString RuntimeAdapterId;
	FBlueprintHelperGraphBodyBoundaryModel BoundaryModel;
	TFunction<FBlueprintHelperToolResultBase()> ApplyMutation;
	TFunction<FBlueprintHelperToolResultBase()> ProjectPreview;
	TFunction<void()> Rollback;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphWriteUnitOfWorkResult
{
	FBlueprintHelperToolResultBase ToolResult;
	TArray<FString> StageTrace;
	bool bRolledBack = false;
};

class BLUEPRINTHELPER_API FBlueprintHelperGraphWriteUnitOfWork
{
public:
	static FBlueprintHelperGraphWriteUnitOfWorkResult Run(
		const FBlueprintHelperGraphWriteUnitOfWorkRequest& Request);

	static FBlueprintHelperToolResultBase RunExistingOperation(
		EBlueprintHelperGraphWriteUnitOfWorkMode Mode,
		const FString& RuntimeAdapterId,
		const FString& TaskSpecStrategy,
		EBlueprintHelperGraphBodyKind BodyKind,
		TFunction<FBlueprintHelperToolResultBase()> ExistingOperation);

	static FString StageToString(EBlueprintHelperGraphWriteUnitOfWorkStage Stage);
};
