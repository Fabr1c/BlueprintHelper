// BlueprintHelper shared GraphWrite mutation kernel wrapper.

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteResultTypes.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyBoundaryModel.h"
#include "Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationIntent.h"
#include "Systems/ToolClusters/GraphWrite/UnitOfWork/BlueprintHelperGraphWriteUnitOfWork.h"

class UEdGraph;

class BLUEPRINTHELPER_API FBlueprintHelperSharedGraphMutationKernel
{
public:
	static FBlueprintGenerateResult ExecuteIntents(
		UEdGraph* TargetGraph,
		const TArray<FBlueprintHelperGraphWriteMutationIntent>& Intents,
		TArray<FString>& OutUnresolvedNodes);

	static FBlueprintHelperGraphWriteUnitOfWorkResult RunUnitOfWork(
		const FBlueprintHelperGraphWriteUnitOfWorkRequest& Request);

	static FBlueprintHelperToolResultBase RunExistingOperation(
		EBlueprintHelperGraphWriteUnitOfWorkMode Mode,
		const FString& RuntimeAdapterId,
		const FString& TaskSpecStrategy,
		EBlueprintHelperGraphBodyKind BodyKind,
		TFunction<FBlueprintHelperToolResultBase()> ExistingOperation);
};
