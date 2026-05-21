#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBuildService.h"

#include "Async/Async.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.h"

bool FBlueprintHelperActionContextBuildService::BuildSync(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const TArray<FBlueprintHelperActionContextDemand>& Demands,
	const FBlueprintHelperActionContextRevisionToken& Revision,
	FBlueprintHelperActionContextScope& OutScope,
	FString& OutError)
{
	return FBlueprintHelperActionContextScope::Build(
		Blueprint,
		Graph,
		Demands,
		Revision,
		OutScope,
		OutError);
}

void FBlueprintHelperActionContextBuildService::BuildAsyncFromSnapshot(
	FBlueprintHelperActionContextSnapshot Snapshot,
	TArray<FBlueprintHelperActionContextDemand> Demands,
	FBuildComplete Completion)
{
	Async(
		EAsyncExecution::ThreadPool,
		[Snapshot = MoveTemp(Snapshot), Demands = MoveTemp(Demands), Completion = MoveTemp(Completion)]() mutable
		{
			FBlueprintHelperResolvedActionContextBundle Bundle =
				FBlueprintHelperActionContextInferenceService::Infer(Snapshot, Demands);
			TSharedPtr<FBlueprintHelperActionContextScope, ESPMode::ThreadSafe> Scope =
				MakeShared<FBlueprintHelperActionContextScope, ESPMode::ThreadSafe>(
					FBlueprintHelperActionContextScope::FromResolved(MoveTemp(Snapshot), MoveTemp(Bundle)));

			AsyncTask(
				ENamedThreads::GameThread,
				[Scope, Completion = MoveTemp(Completion)]() mutable
				{
					if (Completion)
					{
						Completion(Scope, FString());
					}
				});
		});
}
