#include "Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationCoordinator.h"

#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node.h"
#include "K2Node_ExecutionSequence.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"

namespace
{
static bool IsExecPin(const UEdGraphPin* Pin)
{
	return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
}

static UEdGraphPin* FindFirstExecPin(UEdGraphNode* Node, const EEdGraphPinDirection Direction)
{
	if (!Node)
	{
		return nullptr;
	}
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->Direction == Direction && IsExecPin(Pin))
		{
			return Pin;
		}
	}
	return nullptr;
}

static void CollectExecOutputPins(UEdGraphNode* Node, TArray<UEdGraphPin*>& OutPins)
{
	OutPins.Reset();
	if (!Node)
	{
		return;
	}
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Output && IsExecPin(Pin))
		{
			OutPins.Add(Pin);
		}
	}
}

static bool TrySchemaConnect(UEdGraph* Graph, UEdGraphPin* FromPin, UEdGraphPin* ToPin, FString& OutError, bool& bOutChanged)
{
	bOutChanged = false;
	if (!Graph || !FromPin || !ToPin)
	{
		OutError = TEXT("pin_not_found");
		return false;
	}
	if (FromPin->LinkedTo.Contains(ToPin))
	{
		return true;
	}

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	const FPinConnectionResponse Response = Schema->CanCreateConnection(FromPin, ToPin);
	if (Response.Response == CONNECT_RESPONSE_DISALLOW)
	{
		OutError = FString::Printf(TEXT("pin_type_mismatch: %s"), *Response.Message.ToString());
		return false;
	}

	if (IsExecPin(FromPin) && FromPin->LinkedTo.Num() > 0)
	{
		OutError = TEXT("exec_flow_requires_merge");
		return false;
	}

	FromPin->Modify();
	ToPin->Modify();
	bOutChanged = Schema->TryCreateConnection(FromPin, ToPin);
	if (!bOutChanged)
	{
		OutError = TEXT("link_create_failed");
		return false;
	}
	Graph->NotifyGraphChanged();
	return true;
}

static bool TryBreakLink(UEdGraphPin* FromPin, UEdGraphPin* ToPin, FString& OutError, bool& bOutChanged)
{
	bOutChanged = false;
	if (!FromPin || !ToPin)
	{
		OutError = TEXT("pin_not_found");
		return false;
	}
	if (!FromPin->LinkedTo.Contains(ToPin))
	{
		return true;
	}
	FromPin->Modify();
	ToPin->Modify();
	FromPin->BreakLinkTo(ToPin);
	bOutChanged = true;
	return true;
}

static UK2Node_ExecutionSequence* SpawnSequenceNode(UEdGraph* TargetGraph, const FString& IntentId, FString& OutError)
{
	UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(UK2Node_ExecutionSequence::StaticClass());
	if (!NodeSpawner)
	{
		OutError = TEXT("sequence_node_create_failed: node spawner unavailable");
		return nullptr;
	}

	FBlueprintHelperActionNodeSpawnOptions Options;
	Options.NodeId = IntentId.IsEmpty() ? TEXT("merge_sequence") : IntentId;
	UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeNodeSpawner(
		TargetGraph,
		NodeSpawner,
		TEXT("generic_control_node:sequence"),
		FVector2D::ZeroVector,
		Options,
		OutError);
	UK2Node_ExecutionSequence* SequenceNode = Cast<UK2Node_ExecutionSequence>(SpawnedNode);
	if (!SequenceNode)
	{
		OutError = TEXT("sequence_node_create_failed");
		return nullptr;
	}
	return SequenceNode;
}

static bool EnsureSequenceOutputCount(UK2Node_ExecutionSequence* SequenceNode, const int32 DesiredCount, TArray<UEdGraphPin*>& OutPins, FString& OutError)
{
	CollectExecOutputPins(SequenceNode, OutPins);
	while (OutPins.Num() < DesiredCount)
	{
		SequenceNode->AddInputPin();
		CollectExecOutputPins(SequenceNode, OutPins);
	}
	if (OutPins.Num() < DesiredCount)
	{
		OutError = TEXT("sequence_node_create_failed: insufficient output pins");
		return false;
	}
	return true;
}

static bool ApplySetPinDefault(UEdGraph* Graph, UEdGraphPin* Pin, const FString& NewValue, FString& OutError, bool& bOutChanged)
{
	bOutChanged = false;
	if (!Graph || !Pin)
	{
		OutError = TEXT("target_pin_not_found");
		return false;
	}
	if (Pin->DefaultValue == NewValue)
	{
		return true;
	}
	Pin->Modify();
	Pin->DefaultValue = NewValue;
	bOutChanged = true;
	Graph->NotifyGraphChanged();
	return true;
}

static bool ApplyReplaceLink(UEdGraph* Graph, UEdGraphPin* FromPin, UEdGraphPin* OldToPin, UEdGraphPin* NewToPin, FString& OutError, bool& bOutChanged)
{
	bOutChanged = false;
	if (!Graph || !FromPin || !OldToPin || !NewToPin)
	{
		OutError = TEXT("pin_not_found");
		return false;
	}
	if (OldToPin == NewToPin)
	{
		return true;
	}

	bool bDisconnected = false;
	if (!TryBreakLink(FromPin, OldToPin, OutError, bDisconnected))
	{
		return false;
	}

	bool bConnected = false;
	if (!TrySchemaConnect(Graph, FromPin, NewToPin, OutError, bConnected))
	{
		FString RestoreError;
		bool bRestoreChanged = false;
		TrySchemaConnect(Graph, FromPin, OldToPin, RestoreError, bRestoreChanged);
		OutError = OutError.IsEmpty() ? TEXT("link_create_failed") : OutError;
		return false;
	}

	bOutChanged = bDisconnected || bConnected;
	return true;
}

static bool ApplyAppendSemanticBody(UEdGraph* Graph, const FBlueprintHelperGraphWriteMutationIntent& Intent, FString& OutError, bool& bOutChanged)
{
	return TrySchemaConnect(Graph, Intent.Source.Pin, FindFirstExecPin(Intent.InsertedNode, EGPD_Input), OutError, bOutChanged);
}

static bool ApplyInsertSemanticBody(UEdGraph* Graph, const FBlueprintHelperGraphWriteMutationIntent& Intent, FString& OutError, bool& bOutChanged)
{
	bOutChanged = false;
	UEdGraphPin* InsertedExecIn = FindFirstExecPin(Intent.InsertedNode, EGPD_Input);
	UEdGraphPin* InsertedExecOut = FindFirstExecPin(Intent.InsertedNode, EGPD_Output);
	if (!Intent.Source.Pin || !Intent.OriginalSuccessorPin || !InsertedExecIn || !InsertedExecOut)
	{
		OutError = TEXT("insert_between_requires_exec_pins");
		return false;
	}

	bool bDisconnected = false;
	if (!TryBreakLink(Intent.Source.Pin, Intent.OriginalSuccessorPin, OutError, bDisconnected))
	{
		return false;
	}

	bool bAnchorConnected = false;
	if (!TrySchemaConnect(Graph, Intent.Source.Pin, InsertedExecIn, OutError, bAnchorConnected))
	{
		FString RestoreError;
		bool bRestoreChanged = false;
		TrySchemaConnect(Graph, Intent.Source.Pin, Intent.OriginalSuccessorPin, RestoreError, bRestoreChanged);
		return false;
	}

	bool bSuccessorConnected = false;
	if (!TrySchemaConnect(Graph, InsertedExecOut, Intent.OriginalSuccessorPin, OutError, bSuccessorConnected))
	{
		FString RestoreError;
		bool bRestoreChanged = false;
		TryBreakLink(Intent.Source.Pin, InsertedExecIn, RestoreError, bRestoreChanged);
		TrySchemaConnect(Graph, Intent.Source.Pin, Intent.OriginalSuccessorPin, RestoreError, bRestoreChanged);
		return false;
	}

	bOutChanged = bDisconnected || bAnchorConnected || bSuccessorConnected;
	return true;
}

static bool ApplyBranchForkSemanticBody(UEdGraph* Graph, const FBlueprintHelperGraphWriteMutationIntent& Intent, FString& OutError, bool& bOutChanged)
{
	bOutChanged = false;
	if (!Graph || !Intent.Source.Pin || !Intent.InsertedNode)
	{
		OutError = TEXT("branch_fork_requires_anchor_and_inserted_node");
		return false;
	}

	UK2Node_ExecutionSequence* SequenceNode = SpawnSequenceNode(Graph, Intent.IntentId + TEXT("_merge_sequence"), OutError);
	if (!SequenceNode)
	{
		return false;
	}
	if (Intent.OutSequenceNode)
	{
		*Intent.OutSequenceNode = SequenceNode;
	}

	TArray<UEdGraphPin*> ThenPins;
	if (!EnsureSequenceOutputCount(SequenceNode, 2, ThenPins, OutError))
	{
		return false;
	}

	UEdGraphPin* SequenceExecIn = FindFirstExecPin(SequenceNode, EGPD_Input);
	UEdGraphPin* InsertedExecIn = FindFirstExecPin(Intent.InsertedNode, EGPD_Input);
	if (!SequenceExecIn || !InsertedExecIn)
	{
		OutError = TEXT("branch_fork_requires_sequence_and_inserted_exec_pins");
		return false;
	}

	bool bLocalChanged = false;
	if (Intent.OriginalSuccessorPin)
	{
		bool bDisconnected = false;
		if (!TryBreakLink(Intent.Source.Pin, Intent.OriginalSuccessorPin, OutError, bDisconnected))
		{
			return false;
		}
		bLocalChanged = bLocalChanged || bDisconnected;
	}

	bool bAnchorConnected = false;
	if (!TrySchemaConnect(Graph, Intent.Source.Pin, SequenceExecIn, OutError, bAnchorConnected))
	{
		return false;
	}
	bLocalChanged = bLocalChanged || bAnchorConnected;

	const bool bOrigFirst = Intent.SequenceOrder.Num() > 0 && Intent.SequenceOrder[0] == TEXT("original_successor");
	UEdGraphPin* OriginalThenPin = bOrigFirst ? ThenPins[0] : ThenPins[1];
	UEdGraphPin* InsertedThenPin = bOrigFirst ? ThenPins[1] : ThenPins[0];

	if (Intent.OriginalSuccessorPin)
	{
		bool bOriginalConnected = false;
		if (!TrySchemaConnect(Graph, OriginalThenPin, Intent.OriginalSuccessorPin, OutError, bOriginalConnected))
		{
			return false;
		}
		bLocalChanged = bLocalChanged || bOriginalConnected;
	}

	bool bInsertedConnected = false;
	if (!TrySchemaConnect(Graph, InsertedThenPin, InsertedExecIn, OutError, bInsertedConnected))
	{
		return false;
	}
	bOutChanged = bLocalChanged || bInsertedConnected;
	return true;
}
}

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
			bOk = ApplySetPinDefault(TargetGraph, Intent.Target.Pin, Intent.DefaultValue, Error, bChanged);
			Result.RequestedDefaultValueCount++;
			Result.AppliedDefaultValueCount += bOk && bChanged ? 1 : 0;
			break;
		case EBlueprintHelperGraphWriteMutationIntentKind::ConnectPins:
			bOk = TrySchemaConnect(TargetGraph, Intent.Source.Pin, Intent.Target.Pin, Error, bChanged);
			Result.RequestedConnectionCount++;
			Result.CreatedConnectionCount += bOk && bChanged ? 1 : 0;
			break;
		case EBlueprintHelperGraphWriteMutationIntentKind::DisconnectPins:
			bOk = TryBreakLink(Intent.Source.Pin, Intent.Target.Pin, Error, bChanged);
			Result.RequestedConnectionCount++;
			Result.CreatedConnectionCount += bOk && bChanged ? 1 : 0;
			break;
		case EBlueprintHelperGraphWriteMutationIntentKind::ReplacePinConnection:
			bOk = ApplyReplaceLink(TargetGraph, Intent.Source.Pin, Intent.Target.Pin, Intent.ReplacementTarget.Pin, Error, bChanged);
			Result.RequestedConnectionCount++;
			Result.CreatedConnectionCount += bOk && bChanged ? 1 : 0;
			break;
		case EBlueprintHelperGraphWriteMutationIntentKind::AppendSemanticBody:
		case EBlueprintHelperGraphWriteMutationIntentKind::AppendSemanticBodyAfterPin:
			bOk = ApplyAppendSemanticBody(TargetGraph, Intent, Error, bChanged);
			Result.RequestedConnectionCount++;
			Result.CreatedConnectionCount += bOk && bChanged ? 1 : 0;
			break;
		case EBlueprintHelperGraphWriteMutationIntentKind::InsertSemanticBodyBetweenPins:
			bOk = ApplyInsertSemanticBody(TargetGraph, Intent, Error, bChanged);
			Result.RequestedConnectionCount += 2;
			Result.CreatedConnectionCount += bOk && bChanged ? 2 : 0;
			break;
		case EBlueprintHelperGraphWriteMutationIntentKind::BranchForkSemanticBody:
			bOk = ApplyBranchForkSemanticBody(TargetGraph, Intent, Error, bChanged);
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

