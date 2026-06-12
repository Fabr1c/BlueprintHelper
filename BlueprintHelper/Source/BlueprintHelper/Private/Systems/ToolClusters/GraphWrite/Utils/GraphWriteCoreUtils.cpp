// BlueprintHelper Core Utils -- GraphWrite 域通用工具函数实现

#include "Systems/ToolClusters/GraphWrite/Utils/GraphWriteCoreUtils.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_ExecutionSequence.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperSingletonControlFlowEvidenceProvider.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBuildService.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextScope.h"

// ============================================================================
// BlueprintHelperReplaceEntryResolver.cpp
// ============================================================================

bool UGraphWriteCoreUtils::NameMatches(const FString& Candidate, const FString& Expected)
{
	return !Candidate.TrimStartAndEnd().IsEmpty()
		&& Candidate.TrimStartAndEnd().Equals(Expected.TrimStartAndEnd(), ESearchCase::IgnoreCase);
}

bool UGraphWriteCoreUtils::EntryNameMatchesAny(const TArray<FString>& Candidates, const FString& EntryName)
{
	const FString CleanEntryName = EntryName.TrimStartAndEnd();
	if (CleanEntryName.IsEmpty())
	{
		return true;
	}

	for (const FString& Candidate : Candidates)
	{
		if (NameMatches(Candidate, CleanEntryName))
		{
			return true;
		}
	}
	return false;
}

// ============================================================================
// BlueprintHelperMergeBlueprintGraphService.cpp
// ============================================================================

FBlueprintHelperMergeCallableFragmentRequest UGraphWriteCoreUtils::MakeMergeCallableRequest(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FString& Query,
	const FString& FragmentId,
	const FString& SearchMode,
	const FString& AmbiguityPolicy,
	const TArray<FString>& CategoryPriority)
{
	FBlueprintHelperMergeCallableFragmentRequest CallableRequest;
	CallableRequest.Blueprint = Blueprint;
	CallableRequest.Graph = Graph;
	CallableRequest.Query = Query;
	CallableRequest.FragmentId = FragmentId;
	CallableRequest.SourceStatementId = FragmentId;
	CallableRequest.ActionContextStatementId = FragmentId;
	CallableRequest.SearchMode = SearchMode;
	CallableRequest.AmbiguityPolicy = AmbiguityPolicy;
	CallableRequest.CategoryPriority = CategoryPriority;
	CallableRequest.ContextEvidence.Add(TEXT("merge_scope"), TEXT("callable_insert"));
	return CallableRequest;
}

FString UGraphWriteCoreUtils::FormatMergeCallableFailure(
	const FBlueprintHelperMergeCallableFragmentResult& Result,
	const FString& FallbackQuery)
{
	if (!Result.Message.IsEmpty())
	{
		return Result.Code.IsEmpty()
			? Result.Message
			: FString::Printf(TEXT("%s: %s"), *Result.Code, *Result.Message);
	}

	return Result.Code.IsEmpty()
		? FallbackQuery
		: FString::Printf(TEXT("%s: %s"), *Result.Code, *FallbackQuery);
}

// ============================================================================
// BlueprintHelperMergeCallableFragmentService.cpp
// ============================================================================

bool UGraphWriteCoreUtils::BuildCallActionContextScope(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FBlueprintHelperGraphFragmentBuildRequest& BuildRequest,
	FBlueprintHelperActionContextScope& OutScope,
	FString& OutError)
{
	TArray<FString> ArgumentNames;
	BuildRequest.DefaultValues.GetKeys(ArgumentNames);

	const FString StatementId = BuildRequest.ActionContextStatementId.IsEmpty()
		? BuildRequest.FragmentId
		: BuildRequest.ActionContextStatementId;
	const FString ResolveQuery = BuildRequest.ResolvedStableId.TrimStartAndEnd().IsEmpty()
		? BuildRequest.Query
		: BuildRequest.ResolvedStableId.TrimStartAndEnd();

	TArray<FBlueprintHelperActionContextDemand> Demands;
	Demands.Add(FBlueprintHelperActionContextDemandCollector::BuildSingleDemand(
		StatementId,
		TEXT("merge_callable"),
		EBlueprintHelperActionSemanticKind::Call,
		ResolveQuery,
		BuildRequest.Target,
		BuildRequest.PropertyPath,
		BuildRequest.ExpectedReturnType,
		BuildRequest.SearchMode,
		BuildRequest.AmbiguityPolicy,
		BuildRequest.CategoryPriority,
		ArgumentNames));

	const FBlueprintHelperActionContextRevisionToken Revision =
		FBlueprintHelperActionContextScope::MakeRevision(
			Blueprint,
			Graph,
			FString::Printf(TEXT("merge_callable:%s"), *StatementId),
			FString::Printf(TEXT("query=%s;demands=%d"), *ResolveQuery, Demands.Num()));

	return FBlueprintHelperActionContextBuildService::BuildSync(
		Blueprint,
		Graph,
		Demands,
		Revision,
		OutScope,
		OutError);
}

// ============================================================================
// BlueprintHelperGraphWriteMutationCoordinator.cpp
// ============================================================================

bool UGraphWriteCoreUtils::IsExecPin(const UEdGraphPin* Pin)
{
	return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
}

UEdGraphPin* UGraphWriteCoreUtils::FindFirstExecPin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
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

void UGraphWriteCoreUtils::CollectExecOutputPins(UEdGraphNode* Node, TArray<UEdGraphPin*>& OutPins)
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

bool UGraphWriteCoreUtils::TrySchemaConnect(UEdGraph* Graph, UEdGraphPin* FromPin, UEdGraphPin* ToPin, FString& OutError, bool& bOutChanged)
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

bool UGraphWriteCoreUtils::TryBreakLink(UEdGraphPin* FromPin, UEdGraphPin* ToPin, FString& OutError, bool& bOutChanged)
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

UK2Node_ExecutionSequence* UGraphWriteCoreUtils::SpawnSequenceNode(UEdGraph* TargetGraph, const FString& IntentId, FString& OutError)
{
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	if (!Blueprint || !TargetGraph)
	{
		OutError = TEXT("missing_required_evidence: mutation branch-fork sequence requires target graph and Blueprint context.");
		return nullptr;
	}

	const FBlueprintHelperActionResolutionResult ActionResult =
		FBlueprintHelperSingletonControlFlowEvidenceProvider::ResolveCanonical(
			EBlueprintHelperSingletonControlFlowKind::Sequence,
			Blueprint,
			TargetGraph,
			IntentId.IsEmpty() ? TEXT("merge_sequence") : IntentId,
			TEXT("mutation_branch_fork"));
	if (!ActionResult.IsResolved())
	{
		OutError = ActionResult.Message.IsEmpty()
			? TEXT("spawn_or_link_failure: singleton control-flow sequence provider did not resolve.")
			: FString::Printf(TEXT("spawn_or_link_failure: %s"), *ActionResult.Message);
		return nullptr;
	}

	FBlueprintHelperActionNodeSpawnOptions Options;
	Options.NodeId = IntentId.IsEmpty() ? TEXT("merge_sequence") : IntentId;
	UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
		TargetGraph,
		ActionResult,
		FVector2D::ZeroVector,
		Options,
		OutError);
	UK2Node_ExecutionSequence* SequenceNode = Cast<UK2Node_ExecutionSequence>(SpawnedNode);
	if (!SequenceNode)
	{
		OutError = OutError.IsEmpty()
			? TEXT("spawn_or_link_failure: sequence_node_create_failed")
			: FString::Printf(TEXT("spawn_or_link_failure: %s"), *OutError);
		return nullptr;
	}
	return SequenceNode;
}

bool UGraphWriteCoreUtils::EnsureSequenceOutputCount(UK2Node_ExecutionSequence* SequenceNode, const int32 DesiredCount, TArray<UEdGraphPin*>& OutPins, FString& OutError)
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

bool UGraphWriteCoreUtils::ApplySetPinDefault(UEdGraph* Graph, UEdGraphPin* Pin, const FString& NewValue, FString& OutError, bool& bOutChanged)
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

bool UGraphWriteCoreUtils::ApplyReplaceLink(UEdGraph* Graph, UEdGraphPin* FromPin, UEdGraphPin* OldToPin, UEdGraphPin* NewToPin, FString& OutError, bool& bOutChanged)
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

bool UGraphWriteCoreUtils::ApplyReplaceLinkSource(UEdGraph* Graph, UEdGraphPin* OldFromPin, UEdGraphPin* ToPin, UEdGraphPin* NewFromPin, FString& OutError, bool& bOutChanged)
{
	bOutChanged = false;
	if (!Graph || !OldFromPin || !ToPin || !NewFromPin)
	{
		OutError = TEXT("pin_not_found");
		return false;
	}
	if (OldFromPin == NewFromPin)
	{
		return true;
	}

	bool bDisconnected = false;
	if (!TryBreakLink(OldFromPin, ToPin, OutError, bDisconnected))
	{
		return false;
	}

	bool bConnected = false;
	if (!TrySchemaConnect(Graph, NewFromPin, ToPin, OutError, bConnected))
	{
		FString RestoreError;
		bool bRestoreChanged = false;
		TrySchemaConnect(Graph, OldFromPin, ToPin, RestoreError, bRestoreChanged);
		OutError = OutError.IsEmpty() ? TEXT("link_create_failed") : OutError;
		return false;
	}

	bOutChanged = bDisconnected || bConnected;
	return true;
}

bool UGraphWriteCoreUtils::ApplyAppendSemanticBody(UEdGraph* Graph, const FBlueprintHelperGraphWriteMutationIntent& Intent, FString& OutError, bool& bOutChanged)
{
	return TrySchemaConnect(Graph, Intent.Source.Pin, FindFirstExecPin(Intent.InsertedNode, EGPD_Input), OutError, bOutChanged);
}

bool UGraphWriteCoreUtils::ApplyInsertSemanticBody(UEdGraph* Graph, const FBlueprintHelperGraphWriteMutationIntent& Intent, FString& OutError, bool& bOutChanged)
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

bool UGraphWriteCoreUtils::ApplyBranchForkSemanticBody(UEdGraph* Graph, const FBlueprintHelperGraphWriteMutationIntent& Intent, FString& OutError, bool& bOutChanged)
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
