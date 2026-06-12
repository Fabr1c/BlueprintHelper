#include "Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperSharedGraphMutationKernel.h"

#include "Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationCoordinator.h"

FBlueprintGenerateResult FBlueprintHelperSharedGraphMutationKernel::ExecuteIntents(
	UEdGraph* TargetGraph,
	const TArray<FBlueprintHelperGraphWriteMutationIntent>& Intents,
	TArray<FString>& OutUnresolvedNodes)
{
	return FBlueprintHelperGraphWriteMutationCoordinator::ExecuteIntents(
		TargetGraph,
		Intents,
		OutUnresolvedNodes);
}

FBlueprintHelperGraphWriteUnitOfWorkResult FBlueprintHelperSharedGraphMutationKernel::RunUnitOfWork(
	const FBlueprintHelperGraphWriteUnitOfWorkRequest& Request)
{
	return FBlueprintHelperGraphWriteUnitOfWork::Run(Request);
}

FBlueprintHelperToolResultBase FBlueprintHelperSharedGraphMutationKernel::RunExistingOperation(
	EBlueprintHelperGraphWriteUnitOfWorkMode Mode,
	const FString& RuntimeAdapterId,
	const FString& TaskSpecStrategy,
	EBlueprintHelperGraphBodyKind BodyKind,
	TFunction<FBlueprintHelperToolResultBase()> ExistingOperation)
{
	return FBlueprintHelperGraphWriteUnitOfWork::RunExistingOperation(
		Mode,
		RuntimeAdapterId,
		TaskSpecStrategy,
		BodyKind,
		MoveTemp(ExistingOperation));
}
