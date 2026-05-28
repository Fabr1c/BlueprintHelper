#include "Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationCoordinator.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node.h"
#include "K2Node_ExecutionSequence.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSingletonControlFlowEvidenceProvider.h"
#include "Systems/ToolClusters/GraphWrite/Utils/GraphWriteCoreUtils.h"

FBlueprintGenerateResult FBlueprintHelperGraphWriteMutationCoordinator::ExecuteIntents(
	UEdGraph* TargetGraph,
	const TArray<FBlueprintHelperGraphWriteMutationIntent>& Intents,
	TArray<FString>& OutUnresolvedNodes)
{
	FBlueprintGenerateResult Result;
	Result.Message = TEXT("GraphWrite mutation coordinator failed.");
	OutUnresolvedNodes.Reset();

	if (!TargetGraph)
	{
		Result.Message = TEXT("target_graph_invalid");
		OutUnresolvedNodes.Add(Result.Message);
		return Result;
	}

	int32 ChangedCount = 0;
	for (const FBlueprintHelperGraphWriteMutationIntent& Intent : Intents)
	{
		FString Error;
		bool bChanged = false;
		bool bOk = false;
		switch (Intent.Kind)
		{
		case EBlueprintHelperGraphWriteMutationIntentKind::SetPinDefault:
			bOk = UGraphWriteCoreUtils::ApplySetPinDefault(TargetGraph, Intent.Target.Pin, Intent.DefaultValue, Error, bChanged);
			Result.RequestedDefaultValueCount++;
			Result.AppliedDefaultValueCount += bOk && bChanged ? 1 : 0;
			break;
		case EBlueprintHelperGraphWriteMutationIntentKind::ConnectPins:
			bOk = UGraphWriteCoreUtils::TrySchemaConnect(TargetGraph, Intent.Source.Pin, Intent.Target.Pin, Error, bChanged);
			Result.RequestedConnectionCount++;
			Result.CreatedConnectionCount += bOk && bChanged ? 1 : 0;
			break;
		case EBlueprintHelperGraphWriteMutationIntentKind::DisconnectPins:
			bOk = UGraphWriteCoreUtils::TryBreakLink(Intent.Source.Pin, Intent.Target.Pin, Error, bChanged);
			Result.RequestedConnectionCount++;
			Result.CreatedConnectionCount += bOk && bChanged ? 1 : 0;
			break;
		case EBlueprintHelperGraphWriteMutationIntentKind::ReplacePinConnection:
			bOk = UGraphWriteCoreUtils::ApplyReplaceLink(TargetGraph, Intent.Source.Pin, Intent.Target.Pin, Intent.ReplacementTarget.Pin, Error, bChanged);
			Result.RequestedConnectionCount++;
			Result.CreatedConnectionCount += bOk && bChanged ? 1 : 0;
			break;
		case EBlueprintHelperGraphWriteMutationIntentKind::AppendSemanticBody:
		case EBlueprintHelperGraphWriteMutationIntentKind::AppendSemanticBodyAfterPin:
			bOk = UGraphWriteCoreUtils::ApplyAppendSemanticBody(TargetGraph, Intent, Error, bChanged);
			Result.RequestedConnectionCount++;
			Result.CreatedConnectionCount += bOk && bChanged ? 1 : 0;
			break;
		case EBlueprintHelperGraphWriteMutationIntentKind::InsertSemanticBodyBetweenPins:
			bOk = UGraphWriteCoreUtils::ApplyInsertSemanticBody(TargetGraph, Intent, Error, bChanged);
			Result.RequestedConnectionCount += 2;
			Result.CreatedConnectionCount += bOk && bChanged ? 2 : 0;
			break;
		case EBlueprintHelperGraphWriteMutationIntentKind::BranchForkSemanticBody:
			bOk = UGraphWriteCoreUtils::ApplyBranchForkSemanticBody(TargetGraph, Intent, Error, bChanged);
			Result.RequestedConnectionCount += Intent.OriginalSuccessorPin ? 3 : 2;
			Result.CreatedConnectionCount += bOk && bChanged ? (Intent.OriginalSuccessorPin ? 3 : 2) : 0;
			Result.GeneratedNodeCount += bOk ? 1 : 0;
			Result.ExecutionStats.SpawnedNodeCount += bOk ? 1 : 0;
			break;
		default:
			Error = TEXT("unknown_mutation_intent");
			bOk = false;
			break;
		}

		if (!bOk)
		{
			const FString Message = Error.IsEmpty() ? TEXT("mutation_intent_failed") : Error;
			OutUnresolvedNodes.Add(Message);
			Result.Message = Message;
			Result.UnresolvedNodeCount = OutUnresolvedNodes.Num();
			Result.bSucceed = false;
			return Result;
		}
		ChangedCount += bChanged ? 1 : 0;
	}

	Result.bSucceed = true;
	Result.Message = ChangedCount > 0
		? FString::Printf(TEXT("GraphWrite mutation coordinator applied %d changes."), ChangedCount)
		: TEXT("GraphWrite mutation coordinator completed with no changes.");
	return Result;
}
