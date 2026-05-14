#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"

#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphDefaultValueApplier.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphExistingNodeMapper.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphJsonParser.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLinker.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLocalVariableService.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeSpawner.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeUtility.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintMultiGraphGenerationPipeline.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphComposer.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDag.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDagBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"
#include "Systems/ToolClusters/GraphWrite/NodeHandlers/BlueprintNodeHandler.h"
#include "Systems/ToolClusters/GraphWrite/OperationHandlers/BlueprintOperationHandler.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectIterator.h"
#include "EdGraphNode_Comment.h"

namespace
{
static bool TryReadStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, FString& OutValue)
{
	return Object.IsValid() && Object->TryGetStringField(FieldName, OutValue) && !OutValue.IsEmpty();
}

static void ReadFragmentEndpointRef(
	const TSharedPtr<FJsonObject>& Object,
	FBlueprintHelperGraphFragmentEndpointRef& OutEndpoint)
{
	if (!Object.IsValid())
	{
		return;
	}

	if (!TryReadStringField(Object, TEXT("fragment_id"), OutEndpoint.FragmentId))
	{
		TryReadStringField(Object, TEXT("fragment"), OutEndpoint.FragmentId);
	}
	if (!TryReadStringField(Object, TEXT("port_id"), OutEndpoint.PortId))
	{
		TryReadStringField(Object, TEXT("port"), OutEndpoint.PortId);
	}
	TryReadStringField(Object, TEXT("pin_name"), OutEndpoint.PinName);
	TryReadStringField(Object, TEXT("type"), OutEndpoint.Type);
}

static void ReadFragmentDataEdgesFromObject(
	const TSharedPtr<FJsonObject>& Object,
	TArray<FBlueprintHelperGraphFragmentDataEdge>& OutDataEdges)
{
	if (!Object.IsValid())
	{
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* DataEdgeValues = nullptr;
	if (!Object->TryGetArrayField(TEXT("data_edges"), DataEdgeValues) || !DataEdgeValues)
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& EdgeValue : *DataEdgeValues)
	{
		const TSharedPtr<FJsonObject> EdgeObject = EdgeValue.IsValid() ? EdgeValue->AsObject() : nullptr;
		if (!EdgeObject.IsValid())
		{
			continue;
		}

		FBlueprintHelperGraphFragmentDataEdge Edge;
		TryReadStringField(EdgeObject, TEXT("edge_id"), Edge.EdgeId);
		TryReadStringField(EdgeObject, TEXT("symbol_id"), Edge.SymbolId);
		TryReadStringField(EdgeObject, TEXT("path"), Edge.Path);

		const TSharedPtr<FJsonObject>* FromObject = nullptr;
		if (EdgeObject->TryGetObjectField(TEXT("from"), FromObject) && FromObject)
		{
			ReadFragmentEndpointRef(*FromObject, Edge.From);
		}

		const TSharedPtr<FJsonObject>* ToObject = nullptr;
		if (EdgeObject->TryGetObjectField(TEXT("to"), ToObject) && ToObject)
		{
			ReadFragmentEndpointRef(*ToObject, Edge.To);
		}

		if (Edge.From.IsValid() && Edge.To.IsValid())
		{
			OutDataEdges.Add(MoveTemp(Edge));
		}
	}
}

static void AppendSemanticFragmentDataEdges(
	UEdGraph* TargetGraph,
	const TSharedPtr<FJsonObject>& Object,
	const TArray<FBlueprintHelperNodeFragment>& Fragments,
	TArray<FBlueprintHelperGraphFragmentDataEdge>& OutDataEdges)
{
	const TSharedPtr<FJsonObject>* LogicSpecObject = nullptr;
	if (!Object.IsValid() || !Object->TryGetObjectField(TEXT("logic_spec"), LogicSpecObject) || !LogicSpecObject)
	{
		return;
	}

	UBlueprint* Blueprint = TargetGraph ? FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph) : nullptr;
	FBlueprintHelperGraphSemanticIR SemanticIR;
	if (!FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(*LogicSpecObject, Blueprint, SemanticIR))
	{
		return;
	}

	FBlueprintHelperGraphFragmentDag FragmentDag;
	if (!FBlueprintHelperGraphFragmentDagBuilder::BuildFromSemanticIR(SemanticIR, FragmentDag))
	{
		return;
	}

	TSet<FString> GeneratedFragmentIds;
	for (const FBlueprintHelperNodeFragment& Fragment : Fragments)
	{
		if (!Fragment.FragmentId.IsEmpty())
		{
			GeneratedFragmentIds.Add(Fragment.FragmentId);
		}
	}

	for (const FBlueprintHelperGraphFragmentDataEdge& DataEdge : FragmentDag.DataEdges)
	{
		if (GeneratedFragmentIds.Contains(DataEdge.From.FragmentId) && GeneratedFragmentIds.Contains(DataEdge.To.FragmentId))
		{
			OutDataEdges.Add(DataEdge);
		}
	}
}

static TArray<FBlueprintHelperGraphFragmentDataEdge> CollectFragmentDataEdges(
	UEdGraph* TargetGraph,
	const TSharedPtr<FJsonObject>& Object,
	const TArray<FBlueprintHelperNodeFragment>& Fragments)
{
	TArray<FBlueprintHelperGraphFragmentDataEdge> DataEdges;
	ReadFragmentDataEdgesFromObject(Object, DataEdges);

	const TSharedPtr<FJsonObject>* FragmentDagObject = nullptr;
	if (Object.IsValid() && Object->TryGetObjectField(TEXT("fragment_dag"), FragmentDagObject) && FragmentDagObject)
	{
		ReadFragmentDataEdgesFromObject(*FragmentDagObject, DataEdges);
	}

	const TSharedPtr<FJsonObject>* FragmentDebugObject = nullptr;
	if (Object.IsValid() && Object->TryGetObjectField(TEXT("fragment_debug"), FragmentDebugObject) && FragmentDebugObject)
	{
		const TSharedPtr<FJsonObject>* DebugDagObject = nullptr;
		if ((*FragmentDebugObject)->TryGetObjectField(TEXT("dag"), DebugDagObject) && DebugDagObject)
		{
			ReadFragmentDataEdgesFromObject(*DebugDagObject, DataEdges);
		}
	}

	AppendSemanticFragmentDataEdges(TargetGraph, Object, Fragments, DataEdges);
	return DataEdges;
}

static void AddFragmentPinAlias(
	FBlueprintHelperNodeFragment& Fragment,
	TMap<FString, FBlueprintHelperFragmentPinRef>& DirectionMap,
	const FString& Alias,
	const FBlueprintHelperFragmentPinRef& SourcePinRef)
{
	if (Alias.IsEmpty() || DirectionMap.Contains(Alias))
	{
		return;
	}

	FBlueprintHelperFragmentPinRef AliasPinRef = SourcePinRef;
	AliasPinRef.PinName = Alias;
	DirectionMap.Add(Alias, AliasPinRef);
	if (!Fragment.PinBindings.Contains(Alias))
	{
		Fragment.PinBindings.Add(Alias, AliasPinRef);
	}
}

static FBlueprintHelperNodeFragment BuildDataOnlyFragment(const FString& NodeId, UK2Node* Node)
{
	FBlueprintHelperNodeFragment Fragment;
	if (!Node)
	{
		return Fragment;
	}

	Fragment.FragmentId = NodeId;
	Fragment.SourceStatementId = NodeId;
	Fragment.PrimaryNode = Node;
	Fragment.Nodes.Add(Node);
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin)
		{
			continue;
		}

		const FString PinName = Pin->PinName.ToString();
		if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			FBlueprintHelperFragmentPinRef PinRef{ NodeId, PinName, TEXT("exec"), Pin };
			Fragment.PinBindings.Add(PinName, PinRef);
			if (Pin->Direction == EGPD_Input && !Fragment.ExecEntryPin)
			{
				Fragment.ExecEntryPin = Pin;
				AddFragmentPinAlias(Fragment, Fragment.PinBindings, TEXT("execute"), PinRef);
			}
			else if (Pin->Direction == EGPD_Output)
			{
				if (PinName.Equals(TEXT("then"), ESearchCase::IgnoreCase) && !Fragment.ExecExitPin)
				{
					Fragment.ExecExitPin = Pin;
				}
				AddFragmentPinAlias(Fragment, Fragment.PinBindings, PinName.ToLower(), PinRef);
			}
			continue;
		}

		FBlueprintHelperFragmentPinRef PinRef{ NodeId, PinName, Pin->PinType.PinCategory.ToString(), Pin };
		Fragment.PinBindings.Add(PinName, PinRef);
		if (Pin->Direction == EGPD_Input)
		{
			Fragment.DataInputs.Add(PinName, PinRef);
			AddFragmentPinAlias(Fragment, Fragment.DataInputs, PinName.ToLower(), PinRef);
			if (PinName.Equals(TEXT("A"), ESearchCase::IgnoreCase))
			{
				AddFragmentPinAlias(Fragment, Fragment.DataInputs, TEXT("left"), PinRef);
			}
			if (PinName.Equals(TEXT("B"), ESearchCase::IgnoreCase))
			{
				AddFragmentPinAlias(Fragment, Fragment.DataInputs, TEXT("right"), PinRef);
			}
			if (PinName.Equals(TEXT("Condition"), ESearchCase::IgnoreCase) || PinName.Equals(TEXT("Index"), ESearchCase::IgnoreCase))
			{
				AddFragmentPinAlias(Fragment, Fragment.DataInputs, TEXT("condition"), PinRef);
			}
			if (!Fragment.DataInputs.Contains(TEXT("value")))
			{
				AddFragmentPinAlias(Fragment, Fragment.DataInputs, TEXT("value"), PinRef);
			}
		}
		else if (Pin->Direction == EGPD_Output)
		{
			Fragment.DataOutputs.Add(PinName, PinRef);
			if (!Fragment.DataOutputs.Contains(TEXT("value")))
			{
				Fragment.DataOutputs.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ NodeId, TEXT("value"), Pin->PinType.PinCategory.ToString(), Pin });
				Fragment.PinBindings.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ NodeId, TEXT("value"), Pin->PinType.PinCategory.ToString(), Pin });
			}
			if (!Fragment.DataOutputs.Contains(TEXT("result")))
			{
				Fragment.DataOutputs.Add(TEXT("result"), FBlueprintHelperFragmentPinRef{ NodeId, TEXT("result"), Pin->PinType.PinCategory.ToString(), Pin });
				Fragment.PinBindings.Add(TEXT("result"), FBlueprintHelperFragmentPinRef{ NodeId, TEXT("result"), Pin->PinType.PinCategory.ToString(), Pin });
			}
			if (!Fragment.DataOutputs.Contains(TEXT("return")))
			{
				Fragment.DataOutputs.Add(TEXT("return"), FBlueprintHelperFragmentPinRef{ NodeId, TEXT("return"), Pin->PinType.PinCategory.ToString(), Pin });
				Fragment.PinBindings.Add(TEXT("return"), FBlueprintHelperFragmentPinRef{ NodeId, TEXT("return"), Pin->PinType.PinCategory.ToString(), Pin });
			}
		}
	}
	return Fragment;
}

struct FSemanticStatementExecFlow
{
	TArray<UEdGraphPin*> Entries;
	TArray<UEdGraphPin*> Exits;
	bool bPreservePreviousExits = false;
};

static FString GetSemanticStatementId(const FBlueprintHelperGraphStatementIR& Statement)
{
	return !Statement.StatementId.IsEmpty() ? Statement.StatementId : Statement.Path;
}

static FString GetSemanticExpressionId(const FBlueprintHelperGraphExpressionIR& Expression)
{
	return !Expression.ExpressionId.IsEmpty() ? Expression.ExpressionId : Expression.Path;
}

static void AddSemanticUnresolved(
	TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes,
	const FString& DisplayText,
	const FString& Reason)
{
	TSharedPtr<FUnresolvedNodeItem> UnresolvedItem = MakeShared<FUnresolvedNodeItem>();
	UnresolvedItem->DisplayText = DisplayText;
	UnresolvedItem->Reason = Reason;
	OutUnresolvedNodes.Add(UnresolvedItem);
}

static bool ConnectSemanticExecPins(
	UEdGraph* TargetGraph,
	UEdGraphPin* FromPin,
	UEdGraphPin* ToPin,
	TArray<FBlueprintGeneratorDiagnostic>& ConnectionDiagnostics,
	int32& CreatedConnectionCount)
{
	if (!TargetGraph || !FromPin || !ToPin)
	{
		return false;
	}

	if (FromPin->LinkedTo.Contains(ToPin))
	{
		return true;
	}

	const UEdGraphSchema* Schema = TargetGraph->GetSchema();
	if (!Schema)
	{
		ConnectionDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(
			TEXT("semantic_exec_connection_rejected"),
			TEXT("semantic_ir"),
			TEXT("execute"),
			TEXT("Graph schema is invalid.")));
		return false;
	}

	const FPinConnectionResponse ConnectionResponse = Schema->CanCreateConnection(FromPin, ToPin);
	if (Schema->TryCreateConnection(FromPin, ToPin))
	{
		++CreatedConnectionCount;
		return true;
	}

	ConnectionDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(
		TEXT("semantic_exec_connection_rejected"),
		TEXT("semantic_ir"),
		FromPin->PinName.ToString(),
		ConnectionResponse.Message.IsEmpty() ? TEXT("Schema rejected semantic exec connection.") : ConnectionResponse.Message.ToString()));
	return false;
}

static void AddSemanticFragment(
	const FBlueprintHelperNodeFragment& Fragment,
	TArray<FBlueprintHelperNodeFragment>& GeneratedFragments,
	TSet<FString>& GeneratedFragmentIds,
	int32& GeneratedNodeCount)
{
	if (Fragment.FragmentId.IsEmpty() || GeneratedFragmentIds.Contains(Fragment.FragmentId))
	{
		return;
	}

	GeneratedFragmentIds.Add(Fragment.FragmentId);
	GeneratedNodeCount += FMath::Max(1, Fragment.Nodes.Num());
	GeneratedFragments.Add(Fragment);
}

static void BuildSemanticExpressionFragments(
	UEdGraph* TargetGraph,
	const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression,
	TArray<FBlueprintHelperNodeFragment>& GeneratedFragments,
	TSet<FString>& GeneratedFragmentIds,
	TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes,
	int32& GeneratedNodeCount);

static void BuildSemanticExpressionMapFragments(
	UEdGraph* TargetGraph,
	const TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Expressions,
	TArray<FBlueprintHelperNodeFragment>& GeneratedFragments,
	TSet<FString>& GeneratedFragmentIds,
	TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes,
	int32& GeneratedNodeCount)
{
	for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Pair : Expressions)
	{
		BuildSemanticExpressionFragments(TargetGraph, Pair.Value, GeneratedFragments, GeneratedFragmentIds, OutUnresolvedNodes, GeneratedNodeCount);
	}
}

static void BuildSemanticExpressionFragments(
	UEdGraph* TargetGraph,
	const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression,
	TArray<FBlueprintHelperNodeFragment>& GeneratedFragments,
	TSet<FString>& GeneratedFragmentIds,
	TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes,
	int32& GeneratedNodeCount)
{
	if (!Expression.IsValid())
	{
		return;
	}

	BuildSemanticExpressionMapFragments(TargetGraph, Expression->Args, GeneratedFragments, GeneratedFragmentIds, OutUnresolvedNodes, GeneratedNodeCount);
	for (const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Option : Expression->Options)
	{
		BuildSemanticExpressionFragments(TargetGraph, Option, GeneratedFragments, GeneratedFragmentIds, OutUnresolvedNodes, GeneratedNodeCount);
	}
	BuildSemanticExpressionFragments(TargetGraph, Expression->Left, GeneratedFragments, GeneratedFragmentIds, OutUnresolvedNodes, GeneratedNodeCount);
	BuildSemanticExpressionFragments(TargetGraph, Expression->Right, GeneratedFragments, GeneratedFragmentIds, OutUnresolvedNodes, GeneratedNodeCount);

	if (Expression->Kind == EBlueprintHelperGraphExpressionKind::Literal
		|| Expression->Kind == EBlueprintHelperGraphExpressionKind::Ref)
	{
		return;
	}

	const FString ExpressionId = GetSemanticExpressionId(*Expression);
	if (GeneratedFragmentIds.Contains(ExpressionId))
	{
		return;
	}

	FBlueprintHelperNodeFragment Fragment;
	FString Error;
	if (!FBlueprintHelperGraphStatementBuilder::BuildExpressionFragment(TargetGraph, *Expression, Fragment, Error))
	{
		AddSemanticUnresolved(
			OutUnresolvedNodes,
			FString::Printf(TEXT("Expression %s"), *ExpressionId),
			Error.IsEmpty() ? TEXT("Semantic expression node creation failed.") : Error);
		return;
	}

	AddSemanticFragment(Fragment, GeneratedFragments, GeneratedFragmentIds, GeneratedNodeCount);
}

static void FillLiteralArgsAsDefaults(const TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Args, TMap<FString, FString>& OutDefaultValues)
{
	for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Pair : Args)
	{
		if (Pair.Value.IsValid() && Pair.Value->Kind == EBlueprintHelperGraphExpressionKind::Literal)
		{
			OutDefaultValues.Add(Pair.Key, Pair.Value->LiteralValue);
		}
	}
}

static bool SpawnSemanticStatementFragment(
	UEdGraph* TargetGraph,
	const TSharedPtr<FBlueprintHelperGraphStatementIR>& Statement,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	if (!Statement.IsValid())
	{
		OutError = TEXT("Semantic statement is invalid.");
		return false;
	}

	const FString StatementId = GetSemanticStatementId(*Statement);
	if (Statement->Kind == EBlueprintHelperGraphStatementKind::Call)
	{
		FParsedNode NodeData;
		NodeData.Id = StatementId;
		NodeData.NodeType = EParsedBlueprintNodeType::CallFunction;
		NodeData.SourceType = TEXT("K2Node_CallFunction");
		NodeData.FunctionName = !Statement->Target.IsEmpty() ? Statement->Target : Statement->Name;
		FillLiteralArgsAsDefaults(Statement->Args, NodeData.DefaultValues);
		return FBlueprintHelperGraphStatementBuilder::BuildCallFunctionFragment(TargetGraph, NodeData, OutFragment, OutError);
	}

	if (Statement->Kind == EBlueprintHelperGraphStatementKind::Set)
	{
		const FString VariableName = !Statement->ResolvedTarget.Member.IsEmpty()
			? Statement->ResolvedTarget.Member
			: Statement->Target;
		FParsedNode NodeData;
		NodeData.Id = StatementId;
		NodeData.NodeType = EParsedBlueprintNodeType::VariableSet;
		NodeData.SourceType = TEXT("K2Node_VariableSet");
		NodeData.VariableReference.ScopeType = TEXT("member");
		NodeData.VariableReference.VariableName = VariableName;
		NodeData.VariableReference.bSelfContext = true;
		if (Statement->Value.IsValid() && Statement->Value->Kind == EBlueprintHelperGraphExpressionKind::Literal)
		{
			NodeData.DefaultValues.Add(VariableName, Statement->Value->LiteralValue);
			NodeData.DefaultValues.Add(TEXT("value"), Statement->Value->LiteralValue);
		}
		return FBlueprintHelperGraphStatementBuilder::BuildVariableSetFragment(TargetGraph, NodeData, OutFragment, OutError);
	}

	if (Statement->Kind == EBlueprintHelperGraphStatementKind::Branch)
	{
		FParsedNode NodeData;
		NodeData.Id = StatementId;
		NodeData.NodeType = EParsedBlueprintNodeType::Branch;
		NodeData.SourceType = TEXT("K2Node_IfThenElse");
		if (Statement->Condition.IsValid() && Statement->Condition->Kind == EBlueprintHelperGraphExpressionKind::Literal)
		{
			NodeData.DefaultValues.Add(TEXT("Condition"), Statement->Condition->LiteralValue);
		}

		IBlueprintNodeHandler* Handler = FBlueprintNodeHandlerRegistry::Get().FindHandler(NodeData.NodeType);
		UK2Node* SpawnedNode = Handler ? Handler->Spawn(TargetGraph, NodeData, OutError) : nullptr;
		if (!SpawnedNode)
		{
			if (OutError.IsEmpty())
			{
				OutError = TEXT("Branch node handler is not available.");
			}
			return false;
		}

		OutFragment = BuildDataOnlyFragment(StatementId, SpawnedNode);
		OutFragment.SourceStatementId = StatementId;
		OutFragment.ReviewTargets.Add(StatementId);
		return true;
	}

	OutError = FString::Printf(TEXT("Semantic statement kind is not node-backed: %s."), *Statement->PatternName);
	return false;
}

static FSemanticStatementExecFlow BuildSemanticStatementArray(
	UEdGraph* TargetGraph,
	const TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>>& Statements,
	TArray<UEdGraphPin*> IncomingExits,
	TArray<FBlueprintHelperNodeFragment>& GeneratedFragments,
	TSet<FString>& GeneratedFragmentIds,
	TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes,
	TArray<FBlueprintGeneratorDiagnostic>& ConnectionDiagnostics,
	int32& GeneratedNodeCount,
	int32& CreatedConnectionCount);

static FSemanticStatementExecFlow BuildSemanticStatement(
	UEdGraph* TargetGraph,
	const TSharedPtr<FBlueprintHelperGraphStatementIR>& Statement,
	TArray<FBlueprintHelperNodeFragment>& GeneratedFragments,
	TSet<FString>& GeneratedFragmentIds,
	TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes,
	TArray<FBlueprintGeneratorDiagnostic>& ConnectionDiagnostics,
	int32& GeneratedNodeCount,
	int32& CreatedConnectionCount)
{
	FSemanticStatementExecFlow Flow;
	if (!Statement.IsValid())
	{
		return Flow;
	}

	BuildSemanticExpressionMapFragments(TargetGraph, Statement->Args, GeneratedFragments, GeneratedFragmentIds, OutUnresolvedNodes, GeneratedNodeCount);
	BuildSemanticExpressionFragments(TargetGraph, Statement->Value, GeneratedFragments, GeneratedFragmentIds, OutUnresolvedNodes, GeneratedNodeCount);
	BuildSemanticExpressionFragments(TargetGraph, Statement->Condition, GeneratedFragments, GeneratedFragmentIds, OutUnresolvedNodes, GeneratedNodeCount);

	if (Statement->Kind == EBlueprintHelperGraphStatementKind::Let)
	{
		Flow.bPreservePreviousExits = true;
		return Flow;
	}
	if (Statement->Kind == EBlueprintHelperGraphStatementKind::Return)
	{
		return Flow;
	}

	FBlueprintHelperNodeFragment StatementFragment;
	FString Error;
	if (!SpawnSemanticStatementFragment(TargetGraph, Statement, StatementFragment, Error))
	{
		AddSemanticUnresolved(
			OutUnresolvedNodes,
			FString::Printf(TEXT("Statement %s"), *GetSemanticStatementId(*Statement)),
			Error.IsEmpty() ? TEXT("Semantic statement node creation failed.") : Error);
		return Flow;
	}

	AddSemanticFragment(StatementFragment, GeneratedFragments, GeneratedFragmentIds, GeneratedNodeCount);
	if (StatementFragment.ExecEntryPin)
	{
		Flow.Entries.Add(StatementFragment.ExecEntryPin);
	}

	if (Statement->Kind == EBlueprintHelperGraphStatementKind::Branch)
	{
		UEdGraphPin* ThenPin = FBlueprintGraphNodeUtility::FindPinByAlias(StatementFragment.PrimaryNode, TEXT("then"));
		UEdGraphPin* ElsePin = FBlueprintGraphNodeUtility::FindPinByAlias(StatementFragment.PrimaryNode, TEXT("else"));
		TArray<UEdGraphPin*> ThenIncoming;
		TArray<UEdGraphPin*> ElseIncoming;
		if (ThenPin)
		{
			ThenIncoming.Add(ThenPin);
		}
		if (ElsePin)
		{
			ElseIncoming.Add(ElsePin);
		}

		FSemanticStatementExecFlow ThenFlow = BuildSemanticStatementArray(
			TargetGraph,
			Statement->ThenStatements,
			ThenIncoming,
			GeneratedFragments,
			GeneratedFragmentIds,
			OutUnresolvedNodes,
			ConnectionDiagnostics,
			GeneratedNodeCount,
			CreatedConnectionCount);
		FSemanticStatementExecFlow ElseFlow = BuildSemanticStatementArray(
			TargetGraph,
			Statement->ElseStatements,
			ElseIncoming,
			GeneratedFragments,
			GeneratedFragmentIds,
			OutUnresolvedNodes,
			ConnectionDiagnostics,
			GeneratedNodeCount,
			CreatedConnectionCount);

		Flow.Exits.Append(ThenFlow.Exits.Num() > 0 ? ThenFlow.Exits : ThenIncoming);
		Flow.Exits.Append(ElseFlow.Exits.Num() > 0 ? ElseFlow.Exits : ElseIncoming);
		return Flow;
	}

	if (StatementFragment.ExecExitPin)
	{
		Flow.Exits.Add(StatementFragment.ExecExitPin);
	}
	return Flow;
}

static FSemanticStatementExecFlow BuildSemanticStatementArray(
	UEdGraph* TargetGraph,
	const TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>>& Statements,
	TArray<UEdGraphPin*> IncomingExits,
	TArray<FBlueprintHelperNodeFragment>& GeneratedFragments,
	TSet<FString>& GeneratedFragmentIds,
	TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes,
	TArray<FBlueprintGeneratorDiagnostic>& ConnectionDiagnostics,
	int32& GeneratedNodeCount,
	int32& CreatedConnectionCount)
{
	FSemanticStatementExecFlow SequenceFlow;
	TArray<UEdGraphPin*> PendingExits = MoveTemp(IncomingExits);

	for (const TSharedPtr<FBlueprintHelperGraphStatementIR>& Statement : Statements)
	{
		FSemanticStatementExecFlow CurrentFlow = BuildSemanticStatement(
			TargetGraph,
			Statement,
			GeneratedFragments,
			GeneratedFragmentIds,
			OutUnresolvedNodes,
			ConnectionDiagnostics,
			GeneratedNodeCount,
			CreatedConnectionCount);

		if (CurrentFlow.Entries.Num() > 0)
		{
			for (UEdGraphPin* FromPin : PendingExits)
			{
				for (UEdGraphPin* ToPin : CurrentFlow.Entries)
				{
					ConnectSemanticExecPins(TargetGraph, FromPin, ToPin, ConnectionDiagnostics, CreatedConnectionCount);
				}
			}
			if (SequenceFlow.Entries.Num() == 0)
			{
				SequenceFlow.Entries.Append(CurrentFlow.Entries);
			}
			PendingExits = CurrentFlow.Exits;
		}
		else if (!CurrentFlow.bPreservePreviousExits)
		{
			PendingExits = CurrentFlow.Exits;
		}
	}

	SequenceFlow.Exits = PendingExits;
	return SequenceFlow;
}

static TArray<FBlueprintHelperGraphFragmentDataEdge> FilterSemanticDataEdges(
	const FBlueprintHelperGraphFragmentDag& FragmentDag,
	const TSet<FString>& GeneratedFragmentIds)
{
	TArray<FBlueprintHelperGraphFragmentDataEdge> DataEdges;
	for (const FBlueprintHelperGraphFragmentDataEdge& DataEdge : FragmentDag.DataEdges)
	{
		if (GeneratedFragmentIds.Contains(DataEdge.From.FragmentId) && GeneratedFragmentIds.Contains(DataEdge.To.FragmentId))
		{
			DataEdges.Add(DataEdge);
		}
	}
	return DataEdges;
}

static FBlueprintGenerateResult GenerateSemanticGraphFromJsonObject(
	UEdGraph* TargetGraph,
	const TSharedPtr<FJsonObject>& JsonObject,
	TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes)
{
	FBlueprintGenerateResult Result;
	Result.Message = TEXT("生成失败。");
	OutUnresolvedNodes.Empty();

	const TSharedPtr<FJsonObject>* LogicSpecObject = nullptr;
	if (!JsonObject.IsValid() || !JsonObject->TryGetObjectField(TEXT("logic_spec"), LogicSpecObject) || !LogicSpecObject)
	{
		Result.Message = TEXT("GraphWrite 现在只接受 logic_spec/SemanticIR；nodes/links 旧节点创建路径已禁用。");
		return Result;
	}

	UBlueprint* Blueprint = TargetGraph ? FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph) : nullptr;
	FBlueprintHelperGraphSemanticIR SemanticIR;
	if (!FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(*LogicSpecObject, Blueprint, SemanticIR))
	{
		for (const FBlueprintHelperGraphSemanticDiagnostic& Diagnostic : SemanticIR.Diagnostics)
		{
			AddSemanticUnresolved(OutUnresolvedNodes, Diagnostic.Path, Diagnostic.Message);
		}
		Result.UnresolvedNodeCount = OutUnresolvedNodes.Num();
		Result.Message = TEXT("SemanticIR 解析或语义校验失败。");
		return Result;
	}

	FBlueprintHelperGraphFragmentDag FragmentDag;
	if (!FBlueprintHelperGraphFragmentDagBuilder::BuildFromSemanticIR(SemanticIR, FragmentDag))
	{
		for (const FBlueprintHelperGraphFragmentDiagnostic& Diagnostic : FragmentDag.Diagnostics)
		{
			AddSemanticUnresolved(OutUnresolvedNodes, Diagnostic.Path, Diagnostic.Message);
		}
		Result.UnresolvedNodeCount = OutUnresolvedNodes.Num();
		Result.Message = TEXT("SemanticIR 到 FragmentDag 构建失败。");
		return Result;
	}

	const FScopedTransaction Transaction(FText::FromString(TEXT("Generate Blueprint from SemanticIR")));
	TargetGraph->Modify();

	TArray<FBlueprintHelperNodeFragment> GeneratedFragments;
	TSet<FString> GeneratedFragmentIds;
	TArray<FBlueprintGeneratorDiagnostic> ConnectionDiagnostics;
	int32 GeneratedNodeCount = 0;
	int32 CreatedConnectionCount = 0;
	TArray<UEdGraphPin*> InitialExits;

	const TSharedPtr<FJsonObject>* EntryObject = nullptr;
	if ((*LogicSpecObject)->TryGetObjectField(TEXT("entry"), EntryObject) && EntryObject && EntryObject->IsValid())
	{
		FString EntryName;
		(*EntryObject)->TryGetStringField(TEXT("name"), EntryName);
		FString EntryId;
		(*EntryObject)->TryGetStringField(TEXT("id"), EntryId);
		if (EntryId.IsEmpty())
		{
			EntryId = EntryName.IsEmpty() ? TEXT("semantic_entry") : EntryName + TEXT("_entry");
		}

		FParsedNode EntryNodeData;
		EntryNodeData.Id = EntryId;
		EntryNodeData.NodeType = EParsedBlueprintNodeType::CustomEvent;
		EntryNodeData.SourceType = TEXT("K2Node_CustomEvent");
		EntryNodeData.EventReference.EventName = EntryName;
		FString EntryError;
		IBlueprintNodeHandler* EntryHandler = FBlueprintNodeHandlerRegistry::Get().FindHandler(EntryNodeData.NodeType);
		UK2Node* EntryNode = EntryHandler ? EntryHandler->Spawn(TargetGraph, EntryNodeData, EntryError) : nullptr;
		if (EntryNode)
		{
			FBlueprintHelperNodeFragment EntryFragment = BuildDataOnlyFragment(EntryId, EntryNode);
			AddSemanticFragment(EntryFragment, GeneratedFragments, GeneratedFragmentIds, GeneratedNodeCount);
			if (EntryFragment.ExecExitPin)
			{
				InitialExits.Add(EntryFragment.ExecExitPin);
			}
		}
		else
		{
			AddSemanticUnresolved(OutUnresolvedNodes, EntryId, EntryError.IsEmpty() ? TEXT("Semantic entry node creation failed.") : EntryError);
		}
	}

	BuildSemanticStatementArray(
		TargetGraph,
		SemanticIR.Statements,
		InitialExits,
		GeneratedFragments,
		GeneratedFragmentIds,
		OutUnresolvedNodes,
		ConnectionDiagnostics,
		GeneratedNodeCount,
		CreatedConnectionCount);

	const TArray<FBlueprintHelperGraphFragmentDataEdge> DataEdges = FilterSemanticDataEdges(FragmentDag, GeneratedFragmentIds);
	CreatedConnectionCount += FBlueprintGraphLinker::ConnectFragmentDataEdges(
		TargetGraph,
		GeneratedFragments,
		DataEdges,
		ConnectionDiagnostics);

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	Result.GeneratedNodeCount = GeneratedNodeCount;
	Result.RequestedConnectionCount = DataEdges.Num();
	Result.CreatedConnectionCount = CreatedConnectionCount;
	Result.ConnectionDiagnostics = ConnectionDiagnostics;
	Result.UnresolvedNodeCount = OutUnresolvedNodes.Num();
	Result.bSucceed = Result.UnresolvedNodeCount == 0 && GeneratedNodeCount > 0;
	Result.Message = Result.bSucceed
		? FString::Printf(TEXT("SemanticIR 生成完成：成功 %d 个节点，建立 %d 条连线。"), GeneratedNodeCount, CreatedConnectionCount)
		: FString::Printf(TEXT("SemanticIR 生成完成但存在 %d 个未处理项。"), Result.UnresolvedNodeCount);
	return Result;
}
}

FBlueprintGenerateResult FBlueprintGraphGenerationPipeline::GenerateBlueprintFromJson(UEdGraph* TargetGraph, const FString& JsonString,
	TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes)
{
	FBlueprintGenerateResult Result;
	Result.Message = TEXT("生成失败。") ;

	if (!TargetGraph)
	{
		Result.Message = TEXT("目标蓝图图表无效。");
		return Result;
	}

	OutUnresolvedNodes.Empty();
	const FString TrimmedJsonString = JsonString.TrimStartAndEnd();
	if (TrimmedJsonString.IsEmpty())
	{
		Result.Message = TEXT("JSON 文本为空，请先执行蓝图转 JSON 或粘贴符合规则的 JSON。");
		return Result;
	}

	if (!TrimmedJsonString.StartsWith(TEXT("{")))
	{
		Result.Message = TEXT("主文本区不是有效 JSON，请先点击“从蓝图文本/剪贴板转换为JSON”。");
		return Result;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(TrimmedJsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		Result.Message = FString::Printf(TEXT("JSON 解析失败：%s"), *Reader->GetErrorMessage());
		return Result;
	}

	// v2.1 多图 JSON 需要走 Blueprint 级入口，否则 graphs 数组中的节点不会被分发到对应图表。
	if (JsonObject->HasField(TEXT("graphs")))
	{
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
		if (!Blueprint)
		{
			Result.Message = TEXT("无法从目标图表获取蓝图对象，graphs 数组无法执行。");
			return Result;
		}

		return FBlueprintMultiGraphGenerationPipeline::GenerateMultiGraphFromJson(Blueprint, TrimmedJsonString, OutUnresolvedNodes);
	}

	// === Schema 2.0：蓝图级操作 ===
	const TArray<TSharedPtr<FJsonValue>>* BlueprintOpsArray = nullptr;
	if (JsonObject->TryGetArrayField(TEXT("blueprint_operations"), BlueprintOpsArray) && BlueprintOpsArray && BlueprintOpsArray->Num() > 0)
	{
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
		if (!Blueprint)
		{
			Result.Message = TEXT("无法从目标图表获取蓝图对象，blueprint_operations 无法执行。");
			return Result;
		}

		for (const TSharedPtr<FJsonValue>& OpValue : *BlueprintOpsArray)
		{
			const TSharedPtr<FJsonObject> OpObject = OpValue->AsObject();
			if (!OpObject.IsValid())
			{
				continue;
			}

			FString OpName;
			OpObject->TryGetStringField(TEXT("op"), OpName);
			if (OpName.IsEmpty())
			{
				continue;
			}

			IBlueprintOperationHandler* OpHandler = FBlueprintOperationHandlerRegistry::Get().FindHandler(OpName);
			if (!OpHandler)
			{
				TSharedPtr<FUnresolvedNodeItem> UnresolvedItem = MakeShared<FUnresolvedNodeItem>();
				UnresolvedItem->DisplayText = FString::Printf(TEXT("BlueprintOp: %s"), *OpName);
				UnresolvedItem->Reason = FString::Printf(TEXT("未识别的蓝图级操作：%s"), *OpName);
				OutUnresolvedNodes.Add(UnresolvedItem);
				continue;
			}

			FString OpError;
			if (!OpHandler->Execute(Blueprint, OpObject, OpError))
			{
				TSharedPtr<FUnresolvedNodeItem> UnresolvedItem = MakeShared<FUnresolvedNodeItem>();
				UnresolvedItem->DisplayText = FString::Printf(TEXT("BlueprintOp: %s"), *OpName);
				UnresolvedItem->Reason = OpError;
				OutUnresolvedNodes.Add(UnresolvedItem);
			}
		}

		// 蓝图级操作完成后编译骨架，确保后续节点可引用新创建的变量/函数/分发器
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}

	const TSharedPtr<FJsonObject>* LogicSpecObject = nullptr;
	if (JsonObject->TryGetObjectField(TEXT("logic_spec"), LogicSpecObject) && LogicSpecObject)
	{
		return GenerateSemanticGraphFromJsonObject(TargetGraph, JsonObject, OutUnresolvedNodes);
	}

	Result.Message = TEXT("GraphWrite 现在只接受 logic_spec/SemanticIR；nodes/links 旧节点创建路径已禁用。");
	return Result;

	TArray<FParsedNode> ParsedNodes;
	TArray<FParsedLink> ParsedLinks;
	TArray<FParsedLocalVariableDeclaration> ParsedLocalVariableDeclarations;
	TArray<FBlueprintGeneratorDiagnostic> DefaultValueDiagnostics;
	TArray<FBlueprintGeneratorDiagnostic> PinTypeDiagnostics;
	TArray<FBlueprintGeneratorDiagnostic> ConnectionDiagnostics;
	FBlueprintGraphJsonParser::ResolveLocalVariableDeclarations(JsonObject, ParsedLocalVariableDeclarations);

	const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
	if (JsonObject->TryGetArrayField(TEXT("nodes"), NodesArray) && NodesArray)
	{
		for (const TSharedPtr<FJsonValue>& NodeValue : *NodesArray)
		{
			const TSharedPtr<FJsonObject> NodeObject = NodeValue->AsObject();
			if (!NodeObject.IsValid())
			{
				continue;
			}

			FParsedNode ParsedNode;
			ParsedNode.Id = NodeObject->GetStringField(TEXT("id"));
			NodeObject->TryGetStringField(TEXT("type"), ParsedNode.SourceType);
			if (ParsedNode.SourceType.IsEmpty())
			{
				NodeObject->TryGetStringField(TEXT("kind"), ParsedNode.SourceType);
			}
			ParsedNode.SourceType = FBlueprintGraphNodeUtility::NormalizeNodeTypeName(ParsedNode.SourceType);
			ParsedNode.NodeType = FBlueprintGraphJsonParser::ResolveNodeType(NodeObject);
			ParsedNode.FunctionName = FBlueprintGraphJsonParser::ResolveNodeFunctionName(NodeObject);
			ParsedNode.X = NodeObject->HasField(TEXT("x")) ? static_cast<float>(NodeObject->GetNumberField(TEXT("x"))) : 0.0f;
			ParsedNode.Y = NodeObject->HasField(TEXT("y")) ? static_cast<float>(NodeObject->GetNumberField(TEXT("y"))) : 0.0f;
			ParsedNode.VariableReference = FBlueprintGraphJsonParser::ResolveVariableReference(NodeObject);
			ParsedNode.MacroReference = FBlueprintGraphJsonParser::ResolveMacroReference(NodeObject);
			ParsedNode.EventReference = FBlueprintGraphJsonParser::ResolveEventReference(NodeObject);
			ParsedNode.DelegateReference = FBlueprintGraphJsonParser::ResolveDelegateReference(NodeObject);
			ParsedNode.ContainerReference = FBlueprintGraphJsonParser::ResolveContainerReference(NodeObject);
			ParsedNode.StructReference = FBlueprintGraphJsonParser::ResolveStructReference(NodeObject);
			ParsedNode.CastReference = FBlueprintGraphJsonParser::ResolveCastReference(NodeObject);
			ParsedNode.SpawnReference = FBlueprintGraphJsonParser::ResolveSpawnReference(NodeObject);
			ParsedNode.FormatTextReference = FBlueprintGraphJsonParser::ResolveFormatTextReference(NodeObject);
			ParsedNode.TimelineReference = FBlueprintGraphJsonParser::ResolveTimelineReference(NodeObject);
			ParsedNode.LiteralReference = FBlueprintGraphJsonParser::ResolveLiteralReference(NodeObject);
			ParsedNode.ComponentBoundEventReference = FBlueprintGraphJsonParser::ResolveComponentBoundEventReference(NodeObject);
			ParsedNode.CommentReference = FBlueprintGraphJsonParser::ResolveCommentReference(NodeObject);
			ParsedNode.EnhancedInputActionReference = FBlueprintGraphJsonParser::ResolveEnhancedInputActionReference(NodeObject);
			ParsedNode.SwitchReference = FBlueprintGraphJsonParser::ResolveSwitchReference(NodeObject);
			ParsedNode.SelectReference = FBlueprintGraphJsonParser::ResolveSelectReference(NodeObject);

			const TSharedPtr<FJsonObject>* PositionObject = nullptr;
			if (NodeObject->TryGetObjectField(TEXT("position"), PositionObject) && PositionObject && PositionObject->IsValid())
			{
				ParsedNode.X = (*PositionObject)->HasField(TEXT("x")) ? static_cast<float>((*PositionObject)->GetNumberField(TEXT("x"))) : ParsedNode.X;
				ParsedNode.Y = (*PositionObject)->HasField(TEXT("y")) ? static_cast<float>((*PositionObject)->GetNumberField(TEXT("y"))) : ParsedNode.Y;
			}

			const TSharedPtr<FJsonObject>* InputsObject = nullptr;
			if (NodeObject->TryGetObjectField(TEXT("inputs"), InputsObject) && InputsObject && InputsObject->IsValid())
			{
				for (const auto& Pair : (*InputsObject)->Values)
				{
					ParsedNode.DefaultValues.Add(Pair.Key, FBlueprintGraphJsonParser::ConvertJsonValueToString(Pair.Value));
				}
			}

			const TSharedPtr<FJsonObject>* DefaultValuesObject = nullptr;
			if (NodeObject->TryGetObjectField(TEXT("default_values"), DefaultValuesObject) && DefaultValuesObject && DefaultValuesObject->IsValid())
			{
				for (const auto& Pair : (*DefaultValuesObject)->Values)
				{
					ParsedNode.DefaultValues.FindOrAdd(Pair.Key) = FBlueprintGraphJsonParser::ConvertJsonValueToString(Pair.Value);
				}
			}

			const TSharedPtr<FJsonValue>* ValueField = NodeObject->Values.Find(TEXT("value"));
			if (ValueField && ValueField->IsValid() && !ParsedNode.VariableReference.VariableName.IsEmpty()
				&& !ParsedNode.DefaultValues.Contains(ParsedNode.VariableReference.VariableName))
			{
				ParsedNode.DefaultValues.Add(
					ParsedNode.VariableReference.VariableName,
					FBlueprintGraphJsonParser::ConvertJsonValueToString(*ValueField));
			}

			ParsedNodes.Add(ParsedNode);
		}
	}
	else
	{
		Result.Message = TEXT("JSON 中缺少 nodes 数组。");
		return Result;
	}

	const TArray<TSharedPtr<FJsonValue>>* LinksArray = nullptr;
	if (JsonObject->TryGetArrayField(TEXT("links"), LinksArray) && LinksArray)
	{
		for (const TSharedPtr<FJsonValue>& LinkValue : *LinksArray)
		{
			const TSharedPtr<FJsonObject> LinkObject = LinkValue->AsObject();
			if (!LinkObject.IsValid())
			{
				continue;
			}

			FParsedLink ParsedLink;
			LinkObject->TryGetStringField(TEXT("from_id"), ParsedLink.FromId);
			LinkObject->TryGetStringField(TEXT("from_pin"), ParsedLink.FromPin);
			LinkObject->TryGetStringField(TEXT("to_id"), ParsedLink.ToId);
			LinkObject->TryGetStringField(TEXT("to_pin"), ParsedLink.ToPin);

			auto ResolveEndpoint = [](const FString& Endpoint, FString& OutNodeId, FString& OutPinName)
			{
				if (!OutNodeId.IsEmpty() || !OutPinName.IsEmpty() || Endpoint.IsEmpty())
				{
					return;
				}

				FString NodeId;
				FString PinName;
				if (Endpoint.Split(TEXT("."), &NodeId, &PinName, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
				{
					OutNodeId = NodeId;
					OutPinName = PinName;
				}
			};

			FString FromEndpoint;
			FString ToEndpoint;
			LinkObject->TryGetStringField(TEXT("from"), FromEndpoint);
			LinkObject->TryGetStringField(TEXT("to"), ToEndpoint);
			ResolveEndpoint(FromEndpoint, ParsedLink.FromId, ParsedLink.FromPin);
			ResolveEndpoint(ToEndpoint, ParsedLink.ToId, ParsedLink.ToPin);
			ParsedLinks.Add(ParsedLink);
		}
	}

	int32 RequestedDefaultValueCount = 0;
	int32 RequestedPinTypeCount = 0;
	for (const FParsedLocalVariableDeclaration& Declaration : ParsedLocalVariableDeclarations)
	{
		if (Declaration.PinType.IsValid())
		{
			++RequestedPinTypeCount;
		}
	}
	for (const FParsedNode& ParsedNode : ParsedNodes)
	{
		RequestedDefaultValueCount += ParsedNode.DefaultValues.Num();
		RequestedPinTypeCount += FBlueprintGraphNodeUtility::CountRequestedPinTypes(ParsedNode);
	}

	const FScopedTransaction Transaction(FText::FromString(TEXT("Generate Blueprint from JSON")));
	TargetGraph->Modify();

	for (const FParsedLocalVariableDeclaration& Declaration : ParsedLocalVariableDeclarations)
	{
		if (!Declaration.bEnsureExists)
		{
			continue;
		}

		FString EnsureErrorMessage;
		if (!FBlueprintGraphLocalVariableService::EnsureLocalVariableExists(TargetGraph, Declaration, EnsureErrorMessage))
		{
			if (FBlueprintGraphNodeUtility::IsInvalidPinTypeFailure(EnsureErrorMessage))
			{
				PinTypeDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(
					TEXT("invalid_pin_type"),
					Declaration.Name,
					Declaration.Name,
					EnsureErrorMessage));
			}

			TSharedPtr<FUnresolvedNodeItem> UnresolvedItem = MakeShared<FUnresolvedNodeItem>();
			UnresolvedItem->DisplayText = FString::Printf(TEXT("LocalVariable %s"), *Declaration.Name);
			UnresolvedItem->Reason = EnsureErrorMessage;
			OutUnresolvedNodes.Add(UnresolvedItem);
		}
	}

	TMap<FString, UK2Node*> IdToSpawnedNode;
	TArray<FBlueprintHelperNodeFragment> GeneratedFragments;
	int32 GeneratedNodeCount = 0;
	for (const FParsedNode& ParsedNode : ParsedNodes)
	{
		// v2.9 — 跳过虚拟入口/结果节点（导出不生成它们，但 AI 可能手动写入；导入时从图表中自动发现）
		if (ParsedNode.Id == TEXT("__function_entry__") || ParsedNode.Id == TEXT("__function_result__")
			|| ParsedNode.SourceType.Equals(TEXT("K2Node_FunctionEntry"), ESearchCase::IgnoreCase)
			|| ParsedNode.SourceType.Equals(TEXT("K2Node_FunctionResult"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		// v2.3 — Comment 节点特殊处理（UEdGraphNode_Comment 不是 UK2Node）
		if (ParsedNode.NodeType == EParsedBlueprintNodeType::Comment)
		{
			UEdGraphNode_Comment* CommentNode = NewObject<UEdGraphNode_Comment>(TargetGraph);
			TargetGraph->AddNode(CommentNode, true, false);
			CommentNode->CreateNewGuid();
			CommentNode->NodePosX = static_cast<int32>(ParsedNode.X);
			CommentNode->NodePosY = static_cast<int32>(ParsedNode.Y);
			CommentNode->NodeComment = ParsedNode.CommentReference.CommentText;
			CommentNode->FontSize = ParsedNode.CommentReference.FontSize;
			CommentNode->NodeWidth = static_cast<int32>(ParsedNode.CommentReference.Width);
			CommentNode->NodeHeight = static_cast<int32>(ParsedNode.CommentReference.Height);
			if (!ParsedNode.CommentReference.CommentColor.IsEmpty())
			{
				FLinearColor Color;
				if (Color.InitFromString(ParsedNode.CommentReference.CommentColor))
				{
					CommentNode->CommentColor = Color;
				}
			}
			++GeneratedNodeCount;
			continue;
		}

		UK2Node* SpawnedNode = nullptr;
		FString SpawnErrorMessage;
		FBlueprintHelperNodeFragment SpawnedFragment;
		bool bSpawnedFragment = false;

		if (ParsedNode.NodeType == EParsedBlueprintNodeType::CallFunction)
		{
			bSpawnedFragment = FBlueprintHelperGraphStatementBuilder::BuildCallFunctionFragment(
				TargetGraph,
				ParsedNode,
				SpawnedFragment,
				SpawnErrorMessage);
			SpawnedNode = bSpawnedFragment ? SpawnedFragment.PrimaryNode : nullptr;
		}
		else if (ParsedNode.NodeType == EParsedBlueprintNodeType::VariableSet)
		{
			bSpawnedFragment = FBlueprintHelperGraphStatementBuilder::BuildVariableSetFragment(
				TargetGraph,
				ParsedNode,
				SpawnedFragment,
				SpawnErrorMessage);
			SpawnedNode = bSpawnedFragment ? SpawnedFragment.PrimaryNode : nullptr;
		}
		else
		{
			IBlueprintNodeHandler* Handler = FBlueprintNodeHandlerRegistry::Get().FindHandler(ParsedNode.NodeType);
			if (Handler)
			{
				SpawnedNode = Handler->Spawn(TargetGraph, ParsedNode, SpawnErrorMessage);
			}
			else
			{
				SpawnErrorMessage = ParsedNode.SourceType.IsEmpty()
					? TEXT("Unknown node type and no function/variable/macro description is available.")
					: FString::Printf(TEXT("Unknown node type: %s"), *ParsedNode.SourceType);
			}
		}

		if (SpawnedNode)		{
			IdToSpawnedNode.Add(ParsedNode.Id, SpawnedNode);
			if (bSpawnedFragment)
			{
				GeneratedFragments.Add(MoveTemp(SpawnedFragment));
			}
			else
			{
				FBlueprintHelperNodeFragment DataOnlyFragment = BuildDataOnlyFragment(ParsedNode.Id, SpawnedNode);
				if (DataOnlyFragment.DataInputs.Num() > 0 || DataOnlyFragment.DataOutputs.Num() > 0)
				{
					GeneratedFragments.Add(MoveTemp(DataOnlyFragment));
				}
			}
			++GeneratedNodeCount;
			continue;
		}

		if (FBlueprintGraphNodeUtility::IsInvalidPinTypeFailure(SpawnErrorMessage))
		{
			PinTypeDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(
				TEXT("invalid_pin_type"),
				ParsedNode.Id,
				FBlueprintGraphNodeUtility::FindDiagnosticPinName(ParsedNode, SpawnErrorMessage),
				SpawnErrorMessage));
		}

		TSharedPtr<FUnresolvedNodeItem> UnresolvedItem = MakeShared<FUnresolvedNodeItem>();
		UnresolvedItem->NodeData = ParsedNode;
		UnresolvedItem->DisplayText = ParsedNode.FunctionName.IsEmpty()
			? FString::Printf(TEXT("%s (%s)"), *ParsedNode.SourceType, *ParsedNode.Id)
			: FString::Printf(TEXT("%s (%s)"), *ParsedNode.FunctionName, *ParsedNode.Id);
		UnresolvedItem->Reason = SpawnErrorMessage.IsEmpty() ? TEXT("不支持的节点类型或配置不完整。") : SpawnErrorMessage;
		OutUnresolvedNodes.Add(UnresolvedItem);
	}

	// 将图中已有的 FunctionEntry / FunctionResult 注入 ID 映射，以便连线恢复
	for (UEdGraphNode* ExistingNode : TargetGraph->Nodes)
	{
		if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(ExistingNode))
		{
			IdToSpawnedNode.FindOrAdd(TEXT("__function_entry__"), Entry);
		}
		else if (UK2Node_FunctionResult* ResultNode = Cast<UK2Node_FunctionResult>(ExistingNode))
		{
			IdToSpawnedNode.FindOrAdd(TEXT("__function_result__"), ResultNode);
		}
	}

	// v2.9 — existing_node_refs：允许增量导入引用图中已有节点
	const TArray<TSharedPtr<FJsonValue>>* ExistingRefsArray = nullptr;
	if (JsonObject->TryGetArrayField(TEXT("existing_node_refs"), ExistingRefsArray) && ExistingRefsArray)
	{
		for (const TSharedPtr<FJsonValue>& RefValue : *ExistingRefsArray)
		{
			const TSharedPtr<FJsonObject> RefObject = RefValue->AsObject();
			if (!RefObject.IsValid()) continue;

			FString RefId;
			RefObject->TryGetStringField(TEXT("id"), RefId);
			if (RefId.IsEmpty()) continue;

			FString MatchTitle;
			RefObject->TryGetStringField(TEXT("node_title"), MatchTitle);
			FString MatchGuid;
			RefObject->TryGetStringField(TEXT("node_guid"), MatchGuid);

			for (UEdGraphNode* RefCandidate : TargetGraph->Nodes)
			{
				UK2Node* K2Existing = Cast<UK2Node>(RefCandidate);
				if (!K2Existing) continue;
				if (IdToSpawnedNode.FindKey(K2Existing)) continue; // 已经被映射

				bool bMatched = false;
				if (!MatchGuid.IsEmpty())
				{
					bMatched = RefCandidate->NodeGuid.ToString(EGuidFormats::Digits) == MatchGuid;
				}
				else if (!MatchTitle.IsEmpty())
				{
					const FString Title = RefCandidate->GetNodeTitle(ENodeTitleType::ListView).ToString();
					bMatched = Title.Contains(MatchTitle);
				}

				if (bMatched)
				{
					IdToSpawnedNode.Add(RefId, K2Existing);
					break;
				}
			}
		}
	}

	// v2.9 — 先 Reconstruct 新生成的节点以确保引脚完整，再连线（避免连线后 Reconstruct 破坏连接）
	for (const auto& Pair : IdToSpawnedNode)
	{
		if (Pair.Value)
		{
			TargetGraph->GetSchema()->ReconstructNode(*Pair.Value);
		}
	}

	int32 AppliedDefaultValueCount = 0;
	for (const FParsedNode& ParsedNode : ParsedNodes)
	{
		UK2Node** SpawnedNodePtr = IdToSpawnedNode.Find(ParsedNode.Id);
		if (!SpawnedNodePtr || !*SpawnedNodePtr)
		{
			continue;
		}

		TArray<FBlueprintGeneratorDiagnostic> NodeDiagnostics = FBlueprintGraphDefaultValueApplier::ApplyDefaultValues(*SpawnedNodePtr, ParsedNode.DefaultValues, ParsedNode.Id);
		AppliedDefaultValueCount += FMath::Max(0, ParsedNode.DefaultValues.Num() - NodeDiagnostics.Num());
		DefaultValueDiagnostics.Append(NodeDiagnostics);
	}

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	int32 CreatedConnectionCount = 0;
	CreatedConnectionCount += FBlueprintGraphLinker::ConnectFragmentDataEdges(
		TargetGraph,
		GeneratedFragments,
		CollectFragmentDataEdges(TargetGraph, JsonObject, GeneratedFragments),
		ConnectionDiagnostics);
	const bool bHasExplicitExecLinks = ParsedLinks.ContainsByPredicate([](const FParsedLink& ParsedLink)
	{
		return ParsedLink.FromPin.Equals(TEXT("then"), ESearchCase::IgnoreCase)
			|| ParsedLink.ToPin.Equals(TEXT("execute"), ESearchCase::IgnoreCase);
	});
	if (!bHasExplicitExecLinks && GeneratedFragments.Num() > 1)
	{
		const FBlueprintHelperGraphComposeResult ComposeResult =
			FBlueprintHelperGraphComposer::ConnectLinearExecChain(TargetGraph, GeneratedFragments);
		CreatedConnectionCount += ComposeResult.CreatedExecConnectionCount;
		for (const FString& ComposeDiagnostic : ComposeResult.Diagnostics)
		{
			ConnectionDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(
				TEXT("composer_exec_connection_rejected"),
				TEXT("graph_composer"),
				TEXT("execute"),
				ComposeDiagnostic));
		}
	}
	for (const FParsedLink& ParsedLink : ParsedLinks)
	{
		if (!Schema)
		{
			ConnectionDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(
				TEXT("link_connection_rejected"),
				ParsedLink.FromId,
				ParsedLink.FromPin,
				TEXT("连线创建失败：K2 Schema 无效。")));
			continue;
		}

		UK2Node** FromNodePtr = IdToSpawnedNode.Find(ParsedLink.FromId);
		UK2Node** ToNodePtr = IdToSpawnedNode.Find(ParsedLink.ToId);
		if (!FromNodePtr || !*FromNodePtr)
		{
			ConnectionDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(
				TEXT("link_node_not_found"),
				ParsedLink.FromId,
				ParsedLink.FromPin,
				FString::Printf(TEXT("连线来源节点未找到：%s。"), *ParsedLink.FromId)));
			continue;
		}
		if (!ToNodePtr || !*ToNodePtr)
		{
			ConnectionDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(
				TEXT("link_node_not_found"),
				ParsedLink.ToId,
				ParsedLink.ToPin,
				FString::Printf(TEXT("连线目标节点未找到：%s。"), *ParsedLink.ToId)));
			continue;
		}

		UK2Node* FromNode = *FromNodePtr;
		UK2Node* ToNode = *ToNodePtr;
		UEdGraphPin* FromPin = FBlueprintGraphNodeUtility::FindPinByAlias(FromNode, ParsedLink.FromPin);
		UEdGraphPin* ToPin = FBlueprintGraphNodeUtility::FindPinByAlias(ToNode, ParsedLink.ToPin);
		if (!FromPin)
		{
			ConnectionDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(
				TEXT("link_pin_not_found"),
				ParsedLink.FromId,
				ParsedLink.FromPin,
				FString::Printf(TEXT("连线来源引脚未找到：%s.%s。"), *ParsedLink.FromId, *ParsedLink.FromPin)));
			continue;
		}
		if (!ToPin)
		{
			ConnectionDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(
				TEXT("link_pin_not_found"),
				ParsedLink.ToId,
				ParsedLink.ToPin,
				FString::Printf(TEXT("连线目标引脚未找到：%s.%s。"), *ParsedLink.ToId, *ParsedLink.ToPin)));
			continue;
		}

		const FPinConnectionResponse ConnectionResponse = Schema->CanCreateConnection(FromPin, ToPin);
		if (Schema->TryCreateConnection(FromPin, ToPin))
		{
			++CreatedConnectionCount;
		}
		else
		{
			ConnectionDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(
				TEXT("link_connection_rejected"),
				ParsedLink.FromId,
				ParsedLink.FromPin,
				ConnectionResponse.Message.IsEmpty()
					? FString::Printf(TEXT("Schema 拒绝连线：%s.%s -> %s.%s。"),
						*ParsedLink.FromId, *ParsedLink.FromPin, *ParsedLink.ToId, *ParsedLink.ToPin)
					: ConnectionResponse.Message.ToString()));
		}
	}

	TargetGraph->NotifyGraphChanged();

	Result.bSucceed = GeneratedNodeCount > 0 || CreatedConnectionCount > 0;
	Result.GeneratedNodeCount = GeneratedNodeCount;
	Result.RequestedDefaultValueCount = RequestedDefaultValueCount;
	Result.AppliedDefaultValueCount = AppliedDefaultValueCount;
	Result.DefaultValueDiagnostics = MoveTemp(DefaultValueDiagnostics);
	Result.RequestedPinTypeCount = RequestedPinTypeCount;
	Result.ResolvedPinTypeCount = FMath::Max(0, RequestedPinTypeCount - PinTypeDiagnostics.Num());
	Result.PinTypeDiagnostics = MoveTemp(PinTypeDiagnostics);
	Result.RequestedConnectionCount = ParsedLinks.Num();
	Result.CreatedConnectionCount = CreatedConnectionCount;
	Result.ConnectionDiagnostics = MoveTemp(ConnectionDiagnostics);
	Result.UnresolvedNodeCount = OutUnresolvedNodes.Num();
	if (Result.bSucceed)
	{
		Result.Message = FString::Printf(TEXT("生成完成：成功 %d 个节点，建立 %d 条连线，未匹配 %d 个。"), Result.GeneratedNodeCount, CreatedConnectionCount, Result.UnresolvedNodeCount);
	}
	else if (Result.UnresolvedNodeCount > 0)
	{
		Result.Message = FString::Printf(TEXT("未生成任何节点：共有 %d 个节点未匹配，请检查 JSON 类型与描述字段。"), Result.UnresolvedNodeCount);
	}
	else
	{
		Result.Message = TEXT("未生成任何节点，请检查 JSON 内容是否符合规则。");
	}
	return Result;
}

FBlueprintGenerateResult FBlueprintGraphGenerationPipeline::GenerateNodesAndLinksForGraph(
	UEdGraph* TargetGraph, const TSharedPtr<FJsonObject>& GraphJsonObject,
	TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes)
{
	FBlueprintGenerateResult Result;
	Result.Message = TEXT("生成失败。");

	if (!TargetGraph || !GraphJsonObject.IsValid())
	{
		Result.Message = TEXT("目标图表或 JSON 对象无效。");
		return Result;
	}

	const TSharedPtr<FJsonObject>* LogicSpecObject = nullptr;
	if (GraphJsonObject->TryGetObjectField(TEXT("logic_spec"), LogicSpecObject) && LogicSpecObject)
	{
		return GenerateSemanticGraphFromJsonObject(TargetGraph, GraphJsonObject, OutUnresolvedNodes);
	}

	Result.Message = TEXT("GraphWrite 现在只接受 logic_spec/SemanticIR；nodes/links 旧节点创建路径已禁用。");
	return Result;

	// 解析本地变量声明
	TArray<FParsedLocalVariableDeclaration> ParsedLocalVariableDeclarations;
	TArray<FBlueprintGeneratorDiagnostic> DefaultValueDiagnostics;
	TArray<FBlueprintGeneratorDiagnostic> PinTypeDiagnostics;
	TArray<FBlueprintGeneratorDiagnostic> ConnectionDiagnostics;
	FBlueprintGraphJsonParser::ResolveLocalVariableDeclarations(GraphJsonObject, ParsedLocalVariableDeclarations);

	// 解析节点
	TArray<FParsedNode> ParsedNodes;
	const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
	if (GraphJsonObject->TryGetArrayField(TEXT("nodes"), NodesArray) && NodesArray)
	{
		for (const TSharedPtr<FJsonValue>& NodeValue : *NodesArray)
		{
			const TSharedPtr<FJsonObject> NodeObject = NodeValue->AsObject();
			if (!NodeObject.IsValid())
			{
				continue;
			}

			FParsedNode ParsedNode;
			ParsedNode.Id = NodeObject->GetStringField(TEXT("id"));
			NodeObject->TryGetStringField(TEXT("type"), ParsedNode.SourceType);
			ParsedNode.SourceType = FBlueprintGraphNodeUtility::NormalizeNodeTypeName(ParsedNode.SourceType);
			ParsedNode.NodeType = FBlueprintGraphJsonParser::ResolveNodeType(NodeObject);
			ParsedNode.FunctionName = FBlueprintGraphJsonParser::ResolveNodeFunctionName(NodeObject);
			ParsedNode.X = NodeObject->HasField(TEXT("x")) ? static_cast<float>(NodeObject->GetNumberField(TEXT("x"))) : 0.0f;
			ParsedNode.Y = NodeObject->HasField(TEXT("y")) ? static_cast<float>(NodeObject->GetNumberField(TEXT("y"))) : 0.0f;
			ParsedNode.VariableReference = FBlueprintGraphJsonParser::ResolveVariableReference(NodeObject);
			ParsedNode.MacroReference = FBlueprintGraphJsonParser::ResolveMacroReference(NodeObject);
			ParsedNode.EventReference = FBlueprintGraphJsonParser::ResolveEventReference(NodeObject);
			ParsedNode.DelegateReference = FBlueprintGraphJsonParser::ResolveDelegateReference(NodeObject);
			ParsedNode.ContainerReference = FBlueprintGraphJsonParser::ResolveContainerReference(NodeObject);
			ParsedNode.StructReference = FBlueprintGraphJsonParser::ResolveStructReference(NodeObject);
			ParsedNode.CastReference = FBlueprintGraphJsonParser::ResolveCastReference(NodeObject);
			ParsedNode.SpawnReference = FBlueprintGraphJsonParser::ResolveSpawnReference(NodeObject);
			ParsedNode.FormatTextReference = FBlueprintGraphJsonParser::ResolveFormatTextReference(NodeObject);
			ParsedNode.TimelineReference = FBlueprintGraphJsonParser::ResolveTimelineReference(NodeObject);
			ParsedNode.LiteralReference = FBlueprintGraphJsonParser::ResolveLiteralReference(NodeObject);
			ParsedNode.ComponentBoundEventReference = FBlueprintGraphJsonParser::ResolveComponentBoundEventReference(NodeObject);
			ParsedNode.CommentReference = FBlueprintGraphJsonParser::ResolveCommentReference(NodeObject);
			ParsedNode.EnhancedInputActionReference = FBlueprintGraphJsonParser::ResolveEnhancedInputActionReference(NodeObject);
			ParsedNode.SwitchReference = FBlueprintGraphJsonParser::ResolveSwitchReference(NodeObject);
			ParsedNode.SelectReference = FBlueprintGraphJsonParser::ResolveSelectReference(NodeObject);

			const TSharedPtr<FJsonObject>* PositionObject = nullptr;
			if (NodeObject->TryGetObjectField(TEXT("position"), PositionObject) && PositionObject && PositionObject->IsValid())
			{
				ParsedNode.X = (*PositionObject)->HasField(TEXT("x")) ? static_cast<float>((*PositionObject)->GetNumberField(TEXT("x"))) : ParsedNode.X;
				ParsedNode.Y = (*PositionObject)->HasField(TEXT("y")) ? static_cast<float>((*PositionObject)->GetNumberField(TEXT("y"))) : ParsedNode.Y;
			}

			const TSharedPtr<FJsonObject>* InputsObject = nullptr;
			if (NodeObject->TryGetObjectField(TEXT("inputs"), InputsObject) && InputsObject && InputsObject->IsValid())
			{
				for (const auto& Pair : (*InputsObject)->Values)
				{
					ParsedNode.DefaultValues.Add(Pair.Key, FBlueprintGraphJsonParser::ConvertJsonValueToString(Pair.Value));
				}
			}

			const TSharedPtr<FJsonObject>* DefaultValuesObject = nullptr;
			if (NodeObject->TryGetObjectField(TEXT("default_values"), DefaultValuesObject) && DefaultValuesObject && DefaultValuesObject->IsValid())
			{
				for (const auto& Pair : (*DefaultValuesObject)->Values)
				{
					ParsedNode.DefaultValues.FindOrAdd(Pair.Key) = FBlueprintGraphJsonParser::ConvertJsonValueToString(Pair.Value);
				}
			}

			ParsedNodes.Add(ParsedNode);
		}
	}

	// 解析连线
	TArray<FParsedLink> ParsedLinks;
	const TArray<TSharedPtr<FJsonValue>>* LinksArray = nullptr;
	if (GraphJsonObject->TryGetArrayField(TEXT("links"), LinksArray) && LinksArray)
	{
		for (const TSharedPtr<FJsonValue>& LinkValue : *LinksArray)
		{
			const TSharedPtr<FJsonObject> LinkObject = LinkValue->AsObject();
			if (!LinkObject.IsValid())
			{
				continue;
			}

			FParsedLink ParsedLink;
			ParsedLink.FromId = LinkObject->GetStringField(TEXT("from_id"));
			ParsedLink.FromPin = LinkObject->GetStringField(TEXT("from_pin"));
			ParsedLink.ToId = LinkObject->GetStringField(TEXT("to_id"));
			ParsedLink.ToPin = LinkObject->GetStringField(TEXT("to_pin"));
			ParsedLinks.Add(ParsedLink);
		}
	}

	int32 RequestedDefaultValueCount = 0;
	int32 RequestedPinTypeCount = 0;
	for (const FParsedLocalVariableDeclaration& Declaration : ParsedLocalVariableDeclarations)
	{
		if (Declaration.PinType.IsValid())
		{
			++RequestedPinTypeCount;
		}
	}
	for (const FParsedNode& ParsedNode : ParsedNodes)
	{
		RequestedDefaultValueCount += ParsedNode.DefaultValues.Num();
		RequestedPinTypeCount += FBlueprintGraphNodeUtility::CountRequestedPinTypes(ParsedNode);
	}

	if (ParsedNodes.Num() == 0)
	{
		Result.bSucceed = true;
		Result.Message = TEXT("图表无节点数据，跳过。");
		Result.RequestedDefaultValueCount = RequestedDefaultValueCount;
		Result.RequestedPinTypeCount = RequestedPinTypeCount;
		Result.ResolvedPinTypeCount = RequestedPinTypeCount;
		Result.RequestedConnectionCount = ParsedLinks.Num();
		return Result;
	}

	const FScopedTransaction Transaction(FText::FromString(TEXT("Generate Graph Nodes from JSON")));
	TargetGraph->Modify();

	for (const FParsedLocalVariableDeclaration& Declaration : ParsedLocalVariableDeclarations)
	{
		if (!Declaration.bEnsureExists)
		{
			continue;
		}
		FString EnsureErrorMessage;
		if (!FBlueprintGraphLocalVariableService::EnsureLocalVariableExists(TargetGraph, Declaration, EnsureErrorMessage))
		{
			if (FBlueprintGraphNodeUtility::IsInvalidPinTypeFailure(EnsureErrorMessage))
			{
				PinTypeDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(
					TEXT("invalid_pin_type"),
					Declaration.Name,
					Declaration.Name,
					EnsureErrorMessage));
			}

			TSharedPtr<FUnresolvedNodeItem> UnresolvedItem = MakeShared<FUnresolvedNodeItem>();
			UnresolvedItem->DisplayText = FString::Printf(TEXT("LocalVariable %s"), *Declaration.Name);
			UnresolvedItem->Reason = EnsureErrorMessage;
			OutUnresolvedNodes.Add(UnresolvedItem);
		}
	}

	TMap<FString, UK2Node*> IdToSpawnedNode;
	TArray<FBlueprintHelperNodeFragment> GeneratedFragments;
	int32 GeneratedNodeCount = 0;
	for (const FParsedNode& ParsedNode : ParsedNodes)
	{
		// v2.9 — 跳过虚拟入口/结果节点（导出不生成它们，但 AI 可能手动写入；导入时从图表中自动发现）
		if (ParsedNode.Id == TEXT("__function_entry__") || ParsedNode.Id == TEXT("__function_result__")
			|| ParsedNode.SourceType.Equals(TEXT("K2Node_FunctionEntry"), ESearchCase::IgnoreCase)
			|| ParsedNode.SourceType.Equals(TEXT("K2Node_FunctionResult"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		// v2.3 — Comment 节点特殊处理（UEdGraphNode_Comment 不是 UK2Node）
		if (ParsedNode.NodeType == EParsedBlueprintNodeType::Comment)
		{
			UEdGraphNode_Comment* CommentNode = NewObject<UEdGraphNode_Comment>(TargetGraph);
			TargetGraph->AddNode(CommentNode, true, false);
			CommentNode->CreateNewGuid();
			CommentNode->NodePosX = static_cast<int32>(ParsedNode.X);
			CommentNode->NodePosY = static_cast<int32>(ParsedNode.Y);
			CommentNode->NodeComment = ParsedNode.CommentReference.CommentText;
			CommentNode->FontSize = ParsedNode.CommentReference.FontSize;
			CommentNode->NodeWidth = static_cast<int32>(ParsedNode.CommentReference.Width);
			CommentNode->NodeHeight = static_cast<int32>(ParsedNode.CommentReference.Height);
			if (!ParsedNode.CommentReference.CommentColor.IsEmpty())
			{
				FLinearColor Color;
				if (Color.InitFromString(ParsedNode.CommentReference.CommentColor))
				{
					CommentNode->CommentColor = Color;
				}
			}
			++GeneratedNodeCount;
			continue;
		}

		UK2Node* SpawnedNode = nullptr;
		FString SpawnErrorMessage;
		FBlueprintHelperNodeFragment SpawnedFragment;
		bool bSpawnedFragment = false;

		if (ParsedNode.NodeType == EParsedBlueprintNodeType::CallFunction)
		{
			bSpawnedFragment = FBlueprintHelperGraphStatementBuilder::BuildCallFunctionFragment(
				TargetGraph,
				ParsedNode,
				SpawnedFragment,
				SpawnErrorMessage);
			SpawnedNode = bSpawnedFragment ? SpawnedFragment.PrimaryNode : nullptr;
		}
		else if (ParsedNode.NodeType == EParsedBlueprintNodeType::VariableSet)
		{
			bSpawnedFragment = FBlueprintHelperGraphStatementBuilder::BuildVariableSetFragment(
				TargetGraph,
				ParsedNode,
				SpawnedFragment,
				SpawnErrorMessage);
			SpawnedNode = bSpawnedFragment ? SpawnedFragment.PrimaryNode : nullptr;
		}
		else
		{
			IBlueprintNodeHandler* Handler = FBlueprintNodeHandlerRegistry::Get().FindHandler(ParsedNode.NodeType);
			if (Handler)
			{
				SpawnedNode = Handler->Spawn(TargetGraph, ParsedNode, SpawnErrorMessage);
			}
			else
			{
				SpawnErrorMessage = ParsedNode.SourceType.IsEmpty()
					? TEXT("Unknown node type and no function/variable/macro description is available.")
					: FString::Printf(TEXT("Unknown node type: %s"), *ParsedNode.SourceType);
			}
		}

		if (SpawnedNode)		{
			IdToSpawnedNode.Add(ParsedNode.Id, SpawnedNode);
			if (bSpawnedFragment)
			{
				GeneratedFragments.Add(MoveTemp(SpawnedFragment));
			}
			else
			{
				FBlueprintHelperNodeFragment DataOnlyFragment = BuildDataOnlyFragment(ParsedNode.Id, SpawnedNode);
				if (DataOnlyFragment.DataInputs.Num() > 0 || DataOnlyFragment.DataOutputs.Num() > 0)
				{
					GeneratedFragments.Add(MoveTemp(DataOnlyFragment));
				}
			}
			++GeneratedNodeCount;
			continue;
		}

		if (FBlueprintGraphNodeUtility::IsInvalidPinTypeFailure(SpawnErrorMessage))
		{
			PinTypeDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(
				TEXT("invalid_pin_type"),
				ParsedNode.Id,
				FBlueprintGraphNodeUtility::FindDiagnosticPinName(ParsedNode, SpawnErrorMessage),
				SpawnErrorMessage));
		}

		TSharedPtr<FUnresolvedNodeItem> UnresolvedItem = MakeShared<FUnresolvedNodeItem>();
		UnresolvedItem->NodeData = ParsedNode;
		UnresolvedItem->DisplayText = ParsedNode.FunctionName.IsEmpty()
			? FString::Printf(TEXT("%s (%s)"), *ParsedNode.SourceType, *ParsedNode.Id)
			: FString::Printf(TEXT("%s (%s)"), *ParsedNode.FunctionName, *ParsedNode.Id);
		UnresolvedItem->Reason = SpawnErrorMessage.IsEmpty() ? TEXT("不支持的节点类型或配置不完整。") : SpawnErrorMessage;
		OutUnresolvedNodes.Add(UnresolvedItem);
	}

	// 将图中已有的 FunctionEntry / FunctionResult 注入 ID 映射，以便连线恢复
	for (UEdGraphNode* ExistingNode : TargetGraph->Nodes)
	{
		if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(ExistingNode))
		{
			IdToSpawnedNode.FindOrAdd(TEXT("__function_entry__"), Entry);
		}
		else if (UK2Node_FunctionResult* ResultNode = Cast<UK2Node_FunctionResult>(ExistingNode))
		{
			IdToSpawnedNode.FindOrAdd(TEXT("__function_result__"), ResultNode);
		}
	}

	// v2.9 — existing_node_refs：允许增量导入引用图中已有节点
	const TArray<TSharedPtr<FJsonValue>>* ExistingRefsArray = nullptr;
	if (GraphJsonObject->TryGetArrayField(TEXT("existing_node_refs"), ExistingRefsArray) && ExistingRefsArray)
	{
		for (const TSharedPtr<FJsonValue>& RefValue : *ExistingRefsArray)
		{
			const TSharedPtr<FJsonObject> RefObject = RefValue->AsObject();
			if (!RefObject.IsValid()) continue;

			FString RefId;
			RefObject->TryGetStringField(TEXT("id"), RefId);
			if (RefId.IsEmpty()) continue;

			FString MatchTitle;
			RefObject->TryGetStringField(TEXT("node_title"), MatchTitle);
			FString MatchGuid;
			RefObject->TryGetStringField(TEXT("node_guid"), MatchGuid);

			for (UEdGraphNode* RefCandidate : TargetGraph->Nodes)
			{
				UK2Node* K2Existing = Cast<UK2Node>(RefCandidate);
				if (!K2Existing) continue;
				if (IdToSpawnedNode.FindKey(K2Existing)) continue; // 已经被映射

				bool bMatched = false;
				if (!MatchGuid.IsEmpty())
				{
					bMatched = RefCandidate->NodeGuid.ToString(EGuidFormats::Digits) == MatchGuid;
				}
				else if (!MatchTitle.IsEmpty())
				{
					const FString Title = RefCandidate->GetNodeTitle(ENodeTitleType::ListView).ToString();
					bMatched = Title.Contains(MatchTitle);
				}

				if (bMatched)
				{
					IdToSpawnedNode.Add(RefId, K2Existing);
					break;
				}
			}
		}
	}

	// v2.9 — 先 Reconstruct 新生成的节点以确保引脚完整，再连线（避免连线后 Reconstruct 破坏连接）
	for (const auto& Pair : IdToSpawnedNode)
	{
		if (Pair.Value)
		{
			TargetGraph->GetSchema()->ReconstructNode(*Pair.Value);
		}
	}

	int32 AppliedDefaultValueCount = 0;
	for (const FParsedNode& ParsedNode : ParsedNodes)
	{
		UK2Node** SpawnedNodePtr = IdToSpawnedNode.Find(ParsedNode.Id);
		if (!SpawnedNodePtr || !*SpawnedNodePtr)
		{
			continue;
		}

		TArray<FBlueprintGeneratorDiagnostic> NodeDiagnostics = FBlueprintGraphDefaultValueApplier::ApplyDefaultValues(*SpawnedNodePtr, ParsedNode.DefaultValues, ParsedNode.Id);
		AppliedDefaultValueCount += FMath::Max(0, ParsedNode.DefaultValues.Num() - NodeDiagnostics.Num());
		DefaultValueDiagnostics.Append(NodeDiagnostics);
	}

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	int32 CreatedConnectionCount = 0;
	CreatedConnectionCount += FBlueprintGraphLinker::ConnectFragmentDataEdges(
		TargetGraph,
		GeneratedFragments,
		CollectFragmentDataEdges(TargetGraph, GraphJsonObject, GeneratedFragments),
		ConnectionDiagnostics);
	const bool bHasExplicitExecLinks = ParsedLinks.ContainsByPredicate([](const FParsedLink& ParsedLink)
	{
		return ParsedLink.FromPin.Equals(TEXT("then"), ESearchCase::IgnoreCase)
			|| ParsedLink.ToPin.Equals(TEXT("execute"), ESearchCase::IgnoreCase);
	});
	if (!bHasExplicitExecLinks && GeneratedFragments.Num() > 1)
	{
		const FBlueprintHelperGraphComposeResult ComposeResult =
			FBlueprintHelperGraphComposer::ConnectLinearExecChain(TargetGraph, GeneratedFragments);
		CreatedConnectionCount += ComposeResult.CreatedExecConnectionCount;
		for (const FString& ComposeDiagnostic : ComposeResult.Diagnostics)
		{
			ConnectionDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(
				TEXT("composer_exec_connection_rejected"),
				TEXT("graph_composer"),
				TEXT("execute"),
				ComposeDiagnostic));
		}
	}
	for (const FParsedLink& ParsedLink : ParsedLinks)
	{
		if (!Schema)
		{
			ConnectionDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(
				TEXT("link_connection_rejected"),
				ParsedLink.FromId,
				ParsedLink.FromPin,
				TEXT("连线创建失败：K2 Schema 无效。")));
			continue;
		}

		UK2Node** FromNodePtr = IdToSpawnedNode.Find(ParsedLink.FromId);
		UK2Node** ToNodePtr = IdToSpawnedNode.Find(ParsedLink.ToId);
		if (!FromNodePtr || !*FromNodePtr)
		{
			ConnectionDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(
				TEXT("link_node_not_found"),
				ParsedLink.FromId,
				ParsedLink.FromPin,
				FString::Printf(TEXT("连线来源节点未找到：%s。"), *ParsedLink.FromId)));
			continue;
		}
		if (!ToNodePtr || !*ToNodePtr)
		{
			ConnectionDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(
				TEXT("link_node_not_found"),
				ParsedLink.ToId,
				ParsedLink.ToPin,
				FString::Printf(TEXT("连线目标节点未找到：%s。"), *ParsedLink.ToId)));
			continue;
		}

		UK2Node* FromNode = *FromNodePtr;
		UK2Node* ToNode = *ToNodePtr;
		UEdGraphPin* FromPin = FBlueprintGraphNodeUtility::FindPinByAlias(FromNode, ParsedLink.FromPin);
		UEdGraphPin* ToPin = FBlueprintGraphNodeUtility::FindPinByAlias(ToNode, ParsedLink.ToPin);
		if (!FromPin)
		{
			ConnectionDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(
				TEXT("link_pin_not_found"),
				ParsedLink.FromId,
				ParsedLink.FromPin,
				FString::Printf(TEXT("连线来源引脚未找到：%s.%s。"), *ParsedLink.FromId, *ParsedLink.FromPin)));
			continue;
		}
		if (!ToPin)
		{
			ConnectionDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(
				TEXT("link_pin_not_found"),
				ParsedLink.ToId,
				ParsedLink.ToPin,
				FString::Printf(TEXT("连线目标引脚未找到：%s.%s。"), *ParsedLink.ToId, *ParsedLink.ToPin)));
			continue;
		}

		const FPinConnectionResponse ConnectionResponse = Schema->CanCreateConnection(FromPin, ToPin);
		if (Schema->TryCreateConnection(FromPin, ToPin))
		{
			++CreatedConnectionCount;
		}
		else
		{
			ConnectionDiagnostics.Add(FBlueprintGraphNodeUtility::MakeGeneratorDiagnostic(
				TEXT("link_connection_rejected"),
				ParsedLink.FromId,
				ParsedLink.FromPin,
				ConnectionResponse.Message.IsEmpty()
					? FString::Printf(TEXT("Schema 拒绝连线：%s.%s -> %s.%s。"),
						*ParsedLink.FromId, *ParsedLink.FromPin, *ParsedLink.ToId, *ParsedLink.ToPin)
					: ConnectionResponse.Message.ToString()));
		}
	}

	TargetGraph->NotifyGraphChanged();

	Result.bSucceed = GeneratedNodeCount > 0 || CreatedConnectionCount > 0;
	Result.GeneratedNodeCount = GeneratedNodeCount;
	Result.RequestedDefaultValueCount = RequestedDefaultValueCount;
	Result.AppliedDefaultValueCount = AppliedDefaultValueCount;
	Result.DefaultValueDiagnostics = MoveTemp(DefaultValueDiagnostics);
	Result.RequestedPinTypeCount = RequestedPinTypeCount;
	Result.ResolvedPinTypeCount = FMath::Max(0, RequestedPinTypeCount - PinTypeDiagnostics.Num());
	Result.PinTypeDiagnostics = MoveTemp(PinTypeDiagnostics);
	Result.RequestedConnectionCount = ParsedLinks.Num();
	Result.CreatedConnectionCount = CreatedConnectionCount;
	Result.ConnectionDiagnostics = MoveTemp(ConnectionDiagnostics);
	Result.UnresolvedNodeCount = OutUnresolvedNodes.Num();
	if (Result.bSucceed)
	{
		Result.Message = FString::Printf(TEXT("生成完成：成功 %d 个节点，建立 %d 条连线，未匹配 %d 个。"), Result.GeneratedNodeCount, CreatedConnectionCount, Result.UnresolvedNodeCount);
	}
	return Result;
}

UEdGraph* FBlueprintGraphGenerationPipeline::FindGraphByName(UBlueprint* Blueprint, const FString& GraphName)
{
	if (!Blueprint || GraphName.IsEmpty())
	{
		return nullptr;
	}

	// EventGraph：搜索 UbergraphPages
	if (GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
	{
		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			if (Graph)
			{
				return Graph;
			}
		}
		return nullptr;
	}

	// 也在 UbergraphPages 中按精确名称搜索
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph && Graph->GetFName() == FName(*GraphName))
		{
			return Graph;
		}
	}

	// 函数图
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetFName() == FName(*GraphName))
		{
			return Graph;
		}
	}

	// 宏图
	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (Graph && Graph->GetFName() == FName(*GraphName))
		{
			return Graph;
		}
	}

	// 委托签名图
	for (UEdGraph* Graph : Blueprint->DelegateSignatureGraphs)
	{
		if (Graph && Graph->GetFName() == FName(*GraphName))
		{
			return Graph;
		}
	}

	return nullptr;
}
