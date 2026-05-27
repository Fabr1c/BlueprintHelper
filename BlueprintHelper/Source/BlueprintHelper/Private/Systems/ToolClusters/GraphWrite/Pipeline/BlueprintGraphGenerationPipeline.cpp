#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"

#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphDefaultValueApplier.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphExistingNodeMapper.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphJsonParser.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLinker.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLocalVariableService.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeUtility.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphWriteContext.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintMultiGraphGenerationPipeline.h"
#include "Systems/ToolClusters/BlueprintHelperToolClusterConfigResolver.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBuildService.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextScope.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFragmentBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphComposer.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDag.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDagBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuilderRegistry.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphEventReferenceUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementTypeUtils.h"
#include "BlueprintNodeSpawner.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "HAL/PlatformTime.h"
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

static double GraphWriteElapsedMs(double StartSeconds)
{
	return (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
}

static bool ShouldReconstructExistingNodes(const TSharedPtr<FJsonObject>& Object)
{
	const TSharedPtr<FJsonObject>* OptionsObject = nullptr;
	bool bReconstructExistingNodes =
		FBlueprintHelperToolClusterConfigResolver::LoadGraphWritePolicy().bReconstructExistingNodes;
	if (Object.IsValid()
		&& Object->TryGetObjectField(TEXT("options"), OptionsObject)
		&& OptionsObject
		&& OptionsObject->IsValid())
	{
		(*OptionsObject)->TryGetBoolField(TEXT("reconstruct_existing_nodes"), bReconstructExistingNodes);
	}
	return bReconstructExistingNodes;
}

static bool IsDryRunPayload(const TSharedPtr<FJsonObject>& Object)
{
	const TSharedPtr<FJsonObject>* OptionsObject = nullptr;
	bool bDryRun = false;
	if (Object.IsValid()
		&& Object->TryGetObjectField(TEXT("options"), OptionsObject)
		&& OptionsObject
		&& OptionsObject->IsValid())
	{
		(*OptionsObject)->TryGetBoolField(TEXT("dry_run"), bDryRun);
	}
	return bDryRun;
}

static UK2Node_CustomEvent* FindExistingCustomEventNode(UEdGraph* Graph, const FString& EventName)
{
	if (!Graph || EventName.IsEmpty())
	{
		return nullptr;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node);
		if (CustomEvent && CustomEvent->CustomFunctionName.ToString().Equals(EventName, ESearchCase::IgnoreCase))
		{
			return CustomEvent;
		}
	}
	return nullptr;
}

static UK2Node_CustomEvent* CreateDryRunSignatureDependencyCustomEventNode(UEdGraph* Graph, const FString& EventName)
{
	if (!Graph || EventName.IsEmpty())
	{
		return nullptr;
	}

	UBlueprintNodeSpawner* EventSpawner = UBlueprintNodeSpawner::Create(UK2Node_CustomEvent::StaticClass());
	FBlueprintHelperActionNodeSpawnOptions SpawnOptions;
	SpawnOptions.NodeId = EventName;
	SpawnOptions.bReconstructAfterSpawn = false;
	SpawnOptions.NodeConfigurationHook = [&EventName](UK2Node& SpawnedNode, const FBlueprintHelperActionNodeSpawnContext& Context, FString& OutError)
	{
		UK2Node_CustomEvent* EventNode = Cast<UK2Node_CustomEvent>(&SpawnedNode);
		if (!EventNode)
		{
			OutError = TEXT("dry-run signature dependency failed: spawned node is not UK2Node_CustomEvent.");
			return false;
		}

		EventNode->CustomFunctionName = FName(*EventName);
		if (EventNode->Pins.Num() == 0)
		{
			EventNode->AllocateDefaultPins();
		}
		if (Context.TargetGraph && Context.TargetGraph->GetSchema())
		{
			Context.TargetGraph->GetSchema()->ReconstructNode(*EventNode);
		}
		return true;
	};

	FString SpawnError;
	UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeNodeSpawner(
		Graph,
		EventSpawner,
		TEXT("dry_run_custom_event_node_spawner"),
		FVector2D::ZeroVector,
		SpawnOptions,
		SpawnError);
	UK2Node_CustomEvent* EventNode = Cast<UK2Node_CustomEvent>(SpawnedNode);
	if (!EventNode)
	{
		return nullptr;
	}

	EventNode->NodePosX = 0;
	EventNode->NodePosY = 0;
	return EventNode;
}

static void ReadFragmentEndpointRef(
	const TSharedPtr<FJsonObject>& Object,
	FBlueprintHelperGraphFragmentEndpointRef& OutEndpoint)
{
	OutEndpoint = FBlueprintHelperGraphFragmentEndpointRef::FromJson(Object);
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
	return FBlueprintHelperGraphStatementTypeUtils::MakeStatementFragmentId(Statement);
}

static FString GetSemanticStatementContextId(const FBlueprintHelperGraphStatementIR& Statement)
{
	if (!Statement.StatementId.IsEmpty())
	{
		return Statement.StatementId;
	}
	return FString::Printf(TEXT("statement:%s"), *Statement.Path);
}

static FString GetSemanticExpressionId(const FBlueprintHelperGraphExpressionIR& Expression)
{
	return FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
}

static void AddSemanticUnresolved(
	TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes,
	const FString& DisplayText,
	const FString& Reason,
	const TArray<FBlueprintHelperCandidateFunctionGroup>& CandidateFunctions = TArray<FBlueprintHelperCandidateFunctionGroup>())
{
	TSharedPtr<FUnresolvedNodeItem> UnresolvedItem = MakeShared<FUnresolvedNodeItem>();
	UnresolvedItem->DisplayText = DisplayText;
	UnresolvedItem->Reason = Reason;
	UnresolvedItem->CandidateFunctions = CandidateFunctions;
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
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression,
	TArray<FBlueprintHelperNodeFragment>& GeneratedFragments,
	TSet<FString>& GeneratedFragmentIds,
	TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes,
	int32& GeneratedNodeCount);

static void BuildSemanticExpressionMapFragments(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Expressions,
	TArray<FBlueprintHelperNodeFragment>& GeneratedFragments,
	TSet<FString>& GeneratedFragmentIds,
	TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes,
	int32& GeneratedNodeCount)
{
	for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Pair : Expressions)
	{
		BuildSemanticExpressionFragments(TargetGraph, ActionContextScope, Pair.Value, GeneratedFragments, GeneratedFragmentIds, OutUnresolvedNodes, GeneratedNodeCount);
	}
}

static void BuildSemanticExpressionFragments(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionContextScope* ActionContextScope,
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

	BuildSemanticExpressionFragments(TargetGraph, ActionContextScope, Expression->TargetObject, GeneratedFragments, GeneratedFragmentIds, OutUnresolvedNodes, GeneratedNodeCount);
	BuildSemanticExpressionMapFragments(TargetGraph, ActionContextScope, Expression->Args, GeneratedFragments, GeneratedFragmentIds, OutUnresolvedNodes, GeneratedNodeCount);
	for (const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Option : Expression->Options)
	{
		BuildSemanticExpressionFragments(TargetGraph, ActionContextScope, Option, GeneratedFragments, GeneratedFragmentIds, OutUnresolvedNodes, GeneratedNodeCount);
	}
	BuildSemanticExpressionFragments(TargetGraph, ActionContextScope, Expression->Left, GeneratedFragments, GeneratedFragmentIds, OutUnresolvedNodes, GeneratedNodeCount);
	BuildSemanticExpressionFragments(TargetGraph, ActionContextScope, Expression->Right, GeneratedFragments, GeneratedFragmentIds, OutUnresolvedNodes, GeneratedNodeCount);

	if (Expression->Kind == EBlueprintHelperGraphExpressionKind::Literal
		|| (Expression->Kind == EBlueprintHelperGraphExpressionKind::Field
			&& Expression->ResolvedTarget.Kind == EBlueprintHelperGraphTargetKind::Temporary))
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
	TArray<FBlueprintHelperCandidateFunctionGroup> CandidateFunctions;
	if (!FBlueprintHelperGraphFragmentBuilderRegistry::TryBuildExpression(TargetGraph, ActionContextScope, *Expression, Fragment, Error, &CandidateFunctions))
	{
		AddSemanticUnresolved(
			OutUnresolvedNodes,
			FString::Printf(TEXT("Expression %s"), *ExpressionId),
			Error.IsEmpty() ? TEXT("Semantic expression node creation failed.") : Error,
			CandidateFunctions);
		return;
	}

	AddSemanticFragment(Fragment, GeneratedFragments, GeneratedFragmentIds, GeneratedNodeCount);
}

static void FillCallArgsAsDefaultsAndTypes(
	const TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Args,
	TMap<FString, FString>& OutDefaultValues,
	TMap<FString, FString>& OutArgumentTypes)
{
	for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Pair : Args)
	{
		if (!Pair.Value.IsValid())
		{
			continue;
		}
		if (!Pair.Value->Type.IsEmpty())
		{
			OutArgumentTypes.Add(Pair.Key, Pair.Value->Type);
		}
		if (Pair.Value->Kind == EBlueprintHelperGraphExpressionKind::Literal)
		{
			OutDefaultValues.Add(Pair.Key, Pair.Value->LiteralValue);
		}
	}
}

static FBlueprintHelperCallFunctionPinType MakeCallFunctionPinTypeFromEdGraphPin(const UEdGraphPin* Pin)
{
	FBlueprintHelperCallFunctionPinType Result;
	if (!Pin)
	{
		return Result;
	}

	Result.Category = Pin->PinType.PinCategory.ToString();
	Result.SubCategory = Pin->PinType.PinSubCategory.ToString();
	if (Pin->PinType.PinSubCategoryObject.IsValid())
	{
		Result.ObjectPath = Pin->PinType.PinSubCategoryObject->GetPathName();
	}
	if (Pin->PinType.ContainerType == EPinContainerType::Array)
	{
		Result.ContainerType = TEXT("array");
	}
	else if (Pin->PinType.ContainerType == EPinContainerType::Set)
	{
		Result.ContainerType = TEXT("set");
	}
	else if (Pin->PinType.ContainerType == EPinContainerType::Map)
	{
		Result.ContainerType = TEXT("map");
	}
	Result.bIsReference = Pin->PinType.bIsReference;
	Result.bIsConst = Pin->PinType.bIsConst;
	return Result;
}

static FBlueprintHelperCallFunctionPinType MakeCallFunctionPinTypeFromDagRef(const FBlueprintHelperGraphFragmentPinTypeRef& PinType)
{
	FBlueprintHelperCallFunctionPinType Result;
	Result.Category = PinType.Category;
	Result.SubCategory = PinType.SubCategory;
	Result.ObjectPath = PinType.ObjectPath;
	Result.ContainerType = PinType.ContainerType;
	Result.bIsReference = PinType.bIsReference;
	Result.bIsConst = PinType.bIsConst;
	return Result;
}

static UEdGraphPin* FindFragmentPinByKey(
	const TMap<FString, FBlueprintHelperFragmentPinRef>& Pins,
	const FString& Key)
{
	if (Key.IsEmpty())
	{
		return nullptr;
	}
	if (const FBlueprintHelperFragmentPinRef* PinRef = Pins.Find(Key))
	{
		return PinRef->Pin;
	}
	for (const TPair<FString, FBlueprintHelperFragmentPinRef>& Pair : Pins)
	{
		if (Pair.Key.Equals(Key, ESearchCase::IgnoreCase))
		{
			return Pair.Value.Pin;
		}
	}
	return nullptr;
}

static const FBlueprintHelperNodeFragment* FindGeneratedFragment(
	const TArray<FBlueprintHelperNodeFragment>& GeneratedFragments,
	const FString& FragmentId)
{
	for (const FBlueprintHelperNodeFragment& Fragment : GeneratedFragments)
	{
		if (Fragment.FragmentId.Equals(FragmentId, ESearchCase::CaseSensitive))
		{
			return &Fragment;
		}
	}
	return nullptr;
}

static FBlueprintHelperCallFunctionPinType ResolveSemanticDataEdgeSourcePinType(
	const FBlueprintHelperGraphFragmentDataEdge& DataEdge,
	const TArray<FBlueprintHelperNodeFragment>& GeneratedFragments)
{
	if (const FBlueprintHelperNodeFragment* SourceFragment = FindGeneratedFragment(GeneratedFragments, DataEdge.From.FragmentId))
	{
		UEdGraphPin* SourcePin = FindFragmentPinByKey(SourceFragment->DataOutputs, DataEdge.From.PinName);
		if (!SourcePin)
		{
			SourcePin = FindFragmentPinByKey(SourceFragment->DataOutputs, DataEdge.From.PortId);
		}
		if (!SourcePin)
		{
			SourcePin = FindFragmentPinByKey(SourceFragment->PinBindings, DataEdge.From.PinName);
		}
		if (!SourcePin)
		{
			SourcePin = FindFragmentPinByKey(SourceFragment->PinBindings, DataEdge.From.PortId);
		}
		if (SourcePin)
		{
			return MakeCallFunctionPinTypeFromEdGraphPin(SourcePin);
		}
	}

	return MakeCallFunctionPinTypeFromDagRef(DataEdge.From.PinType);
}

static TMap<FString, FBlueprintHelperCallFunctionPinType> CollectSemanticArgumentPinTypes(
	const FBlueprintHelperGraphFragmentDag& FragmentDag,
	const TArray<FBlueprintHelperNodeFragment>& GeneratedFragments,
	const FString& ConsumerFragmentId)
{
	TMap<FString, FBlueprintHelperCallFunctionPinType> Result;
	for (const FBlueprintHelperGraphFragmentDataEdge& DataEdge : FragmentDag.DataEdges)
	{
		if (!DataEdge.To.FragmentId.Equals(ConsumerFragmentId, ESearchCase::CaseSensitive))
		{
			continue;
		}

		const FString ArgumentName = !DataEdge.To.PinName.IsEmpty() ? DataEdge.To.PinName : DataEdge.To.PortId;
		if (ArgumentName.IsEmpty())
		{
			continue;
		}
		if (ArgumentName.Equals(TEXT("target_object"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		const FBlueprintHelperCallFunctionPinType PinType = ResolveSemanticDataEdgeSourcePinType(DataEdge, GeneratedFragments);
		if (PinType.IsValid())
		{
			Result.Add(ArgumentName, PinType);
		}
	}
	return Result;
}

static bool SpawnSemanticStatementFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const TSharedPtr<FBlueprintHelperGraphStatementIR>& Statement,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError,
	TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions = nullptr,
	const TMap<FString, FBlueprintHelperCallFunctionPinType>* SemanticArgumentPinTypes = nullptr)
{
	if (!Statement.IsValid())
	{
		OutError = TEXT("Semantic statement is invalid.");
		return false;
	}

	return FBlueprintHelperGraphFragmentBuilderRegistry::TryBuildStatement(
		TargetGraph,
		ActionContextScope,
		*Statement,
		OutFragment,
		OutError,
		OutCandidateFunctions,
		SemanticArgumentPinTypes);
}
static FSemanticStatementExecFlow BuildSemanticStatementArray(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const FBlueprintHelperGraphFragmentDag& FragmentDag,
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
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const FBlueprintHelperGraphFragmentDag& FragmentDag,
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

	BuildSemanticExpressionFragments(TargetGraph, ActionContextScope, Statement->TargetObject, GeneratedFragments, GeneratedFragmentIds, OutUnresolvedNodes, GeneratedNodeCount);
	BuildSemanticExpressionMapFragments(TargetGraph, ActionContextScope, Statement->Args, GeneratedFragments, GeneratedFragmentIds, OutUnresolvedNodes, GeneratedNodeCount);
	BuildSemanticExpressionFragments(TargetGraph, ActionContextScope, Statement->Value, GeneratedFragments, GeneratedFragmentIds, OutUnresolvedNodes, GeneratedNodeCount);
	BuildSemanticExpressionFragments(TargetGraph, ActionContextScope, Statement->Condition, GeneratedFragments, GeneratedFragmentIds, OutUnresolvedNodes, GeneratedNodeCount);

	if (Statement->Kind == EBlueprintHelperGraphStatementKind::Let)
	{
		Flow.bPreservePreviousExits = true;
		return Flow;
	}
	FBlueprintHelperNodeFragment StatementFragment;
	FString Error;
	TArray<FBlueprintHelperCandidateFunctionGroup> CandidateFunctions;
	const FString StatementId = GetSemanticStatementId(*Statement);
	const bool bConsumesSemanticArgumentPinTypes =
		Statement->Kind == EBlueprintHelperGraphStatementKind::Call
		|| Statement->Kind == EBlueprintHelperGraphStatementKind::ContainerAction;
	const TMap<FString, FBlueprintHelperCallFunctionPinType> SemanticArgumentPinTypes =
		bConsumesSemanticArgumentPinTypes
			? CollectSemanticArgumentPinTypes(FragmentDag, GeneratedFragments, StatementId)
			: TMap<FString, FBlueprintHelperCallFunctionPinType>();
	if (!SpawnSemanticStatementFragment(TargetGraph, ActionContextScope, Statement, StatementFragment, Error, &CandidateFunctions, &SemanticArgumentPinTypes))
	{
		AddSemanticUnresolved(
			OutUnresolvedNodes,
			FString::Printf(TEXT("Statement %s"), *GetSemanticStatementId(*Statement)),
			Error.IsEmpty() ? TEXT("Semantic statement node creation failed.") : Error,
			CandidateFunctions);
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
			ActionContextScope,
			FragmentDag,
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
			ActionContextScope,
			FragmentDag,
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
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const FBlueprintHelperGraphFragmentDag& FragmentDag,
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
			ActionContextScope,
			FragmentDag,
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
	Result.Message = TEXT("Generation failed.");
	OutUnresolvedNodes.Empty();

	const TSharedPtr<FJsonObject>* LogicSpecObject = nullptr;
	if (!JsonObject.IsValid() || !JsonObject->TryGetObjectField(TEXT("logic_spec"), LogicSpecObject) || !LogicSpecObject)
	{
		Result.Message = TEXT("GraphWrite only accepts logic_spec/SemanticIR; legacy nodes/links creation is disabled.");
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
		Result.Message = TEXT("SemanticIR parse or semantic validation failed.");
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
		Result.Message = TEXT("SemanticIR to FragmentDag build failed.");
		return Result;
	}

	TArray<FBlueprintHelperActionContextDemand> ActionContextDemands =
		FBlueprintHelperActionContextDemandCollector::CollectFromSemanticIR(SemanticIR);
	FBlueprintHelperActionContextScope ActionContextScope;
	const FBlueprintHelperActionContextRevisionToken ActionContextRevision =
		FBlueprintHelperActionContextScope::MakeRevision(
			Blueprint,
			TargetGraph,
			FString::Printf(TEXT("semantic_graph:%s"), TargetGraph ? *TargetGraph->GetPathName() : TEXT("")),
			FString::Printf(
				TEXT("schema=%s;statements=%d;demands=%d"),
				*SemanticIR.Schema,
				SemanticIR.Statements.Num(),
				ActionContextDemands.Num()));
	FString ActionContextError;
	if (!FBlueprintHelperActionContextBuildService::BuildSync(
		Blueprint,
		TargetGraph,
		ActionContextDemands,
		ActionContextRevision,
		ActionContextScope,
		ActionContextError))
	{
		AddSemanticUnresolved(
			OutUnresolvedNodes,
			TEXT("action_context"),
			ActionContextError.IsEmpty() ? TEXT("ActionContext scope build failed.") : ActionContextError);
		Result.UnresolvedNodeCount = OutUnresolvedNodes.Num();
		Result.Message = TEXT("ActionContext build failed.");
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
	FBlueprintGraphWriteContext GraphWriteContext;
	const double BuildContextStart = FPlatformTime::Seconds();
	GraphWriteContext.Initialize(TargetGraph);
	Result.ExecutionStats.BuildContextMs = GraphWriteElapsedMs(BuildContextStart);
	Result.ExecutionStats.RequestedNodeCount = FragmentDag.Fragments.Num();

	const double SpawnNodesStart = FPlatformTime::Seconds();
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

		UK2Node* EntryNode = ShouldReconstructExistingNodes(JsonObject)
			? FindExistingCustomEventNode(TargetGraph, EntryName)
			: nullptr;
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
			FBlueprintHelperGraphEventReference EntryReference;
			FBlueprintHelperGraphEventReferenceUtils::TryReadEntryReference(*EntryObject, EntryReference);
			if (IsDryRunPayload(JsonObject)
				&& EntryReference.Taxonomy == EBlueprintHelperGraphEventTaxonomy::CustomEvent
				&& EntryReference.HasSignatureEvidence())
			{
				EntryNode = CreateDryRunSignatureDependencyCustomEventNode(TargetGraph, EntryReference.Name.IsEmpty() ? EntryName : EntryReference.Name);
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
					AddSemanticUnresolved(OutUnresolvedNodes, EntryId, TEXT("Semantic entry dry-run fact could not create a temporary CustomEvent entry."));
				}
			}
			else
			{
				AddSemanticUnresolved(OutUnresolvedNodes, EntryId, TEXT("custom_event entry requires BlueprintSignature signature_evidence_id before GraphWrite can write the body/use-site."));
			}
		}
	}

	BuildSemanticStatementArray(
		TargetGraph,
		&ActionContextScope,
		FragmentDag,
		SemanticIR.Statements,
		InitialExits,
		GeneratedFragments,
		GeneratedFragmentIds,
		OutUnresolvedNodes,
		ConnectionDiagnostics,
		GeneratedNodeCount,
		CreatedConnectionCount);
	Result.ExecutionStats.SpawnNodesMs = GraphWriteElapsedMs(SpawnNodesStart);

	const double ContextIndexStart = FPlatformTime::Seconds();
	for (const FBlueprintHelperNodeFragment& Fragment : GeneratedFragments)
	{
		if (Fragment.PrimaryNode)
		{
			GraphWriteContext.RegisterNode(Fragment.FragmentId, Fragment.PrimaryNode, true);
		}
	}
	Result.ExecutionStats.BuildContextMs += GraphWriteElapsedMs(ContextIndexStart);

	const TArray<FBlueprintHelperGraphFragmentDataEdge> DataEdges = FilterSemanticDataEdges(FragmentDag, GeneratedFragmentIds);
	const double ConnectLinksStart = FPlatformTime::Seconds();
	CreatedConnectionCount += FBlueprintGraphLinker::ConnectFragmentDataEdges(
		TargetGraph,
		GeneratedFragments,
		DataEdges,
		ConnectionDiagnostics);
	Result.ExecutionStats.ConnectLinksMs = GraphWriteElapsedMs(ConnectLinksStart);

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	Result.GeneratedNodeCount = GeneratedNodeCount;
	Result.RequestedConnectionCount = DataEdges.Num();
	Result.CreatedConnectionCount = CreatedConnectionCount;
	Result.ExecutionStats.SpawnedNodeCount = GeneratedNodeCount;
	Result.ExecutionStats.RequestedLinkCount = DataEdges.Num();
	Result.ExecutionStats.CreatedLinkCount = CreatedConnectionCount;
	Result.ConnectionDiagnostics = ConnectionDiagnostics;
	Result.UnresolvedNodeCount = OutUnresolvedNodes.Num();
	Result.bSucceed = Result.UnresolvedNodeCount == 0 && GeneratedNodeCount > 0;
	Result.Message = Result.bSucceed
		? FString::Printf(TEXT("SemanticIR generation completed: spawned %d nodes, linked %d pins."), GeneratedNodeCount, CreatedConnectionCount)
		: FString::Printf(TEXT("SemanticIR generation completed with %d unresolved items."), Result.UnresolvedNodeCount);
	return Result;
}
}

FBlueprintGenerateResult FBlueprintGraphGenerationPipeline::GenerateBlueprintFromJson(UEdGraph* TargetGraph, const FString& JsonString,
	TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes)
{
	FBlueprintGenerateResult Result;
	Result.Message = TEXT("Generation failed.");

	if (!TargetGraph)
	{
		Result.Message = TEXT("Target graph is invalid.");
		return Result;
	}

	OutUnresolvedNodes.Empty();
	const FString TrimmedJsonString = JsonString.TrimStartAndEnd();
	if (TrimmedJsonString.IsEmpty())
	{
		Result.Message = TEXT("JSON text is empty.");
		return Result;
	}

	if (!TrimmedJsonString.StartsWith(TEXT("{")))
	{
		Result.Message = TEXT("GraphWrite input must be a JSON object.");
		return Result;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(TrimmedJsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		Result.Message = FString::Printf(TEXT("JSON parse failed: %s"), *Reader->GetErrorMessage());
		return Result;
	}

	if (JsonObject->HasField(TEXT("graphs")))
	{
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
		if (!Blueprint)
		{
			Result.Message = TEXT("Unable to resolve Blueprint for multi-graph SemanticIR input.");
			return Result;
		}

		return FBlueprintMultiGraphGenerationPipeline::GenerateMultiGraphFromJson(Blueprint, TrimmedJsonString, OutUnresolvedNodes);
	}

	const TArray<TSharedPtr<FJsonValue>>* BlueprintOpsArray = nullptr;
	if (JsonObject->TryGetArrayField(TEXT("blueprint_operations"), BlueprintOpsArray) && BlueprintOpsArray && BlueprintOpsArray->Num() > 0)
	{
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

			TSharedPtr<FUnresolvedNodeItem> UnresolvedItem = MakeShared<FUnresolvedNodeItem>();
			UnresolvedItem->DisplayText = FString::Printf(TEXT("BlueprintOp: %s"), *OpName);
			UnresolvedItem->Reason = FString::Printf(TEXT("Blueprint operation '%s' is unsupported because the legacy GraphWrite operation path has been removed."), *OpName);
			OutUnresolvedNodes.Add(UnresolvedItem);
		}
	}

	const TSharedPtr<FJsonObject>* LogicSpecObject = nullptr;
	if (JsonObject->TryGetObjectField(TEXT("logic_spec"), LogicSpecObject) && LogicSpecObject)
	{
		return GenerateSemanticGraphFromJsonObject(TargetGraph, JsonObject, OutUnresolvedNodes);
	}

	Result.Message = TEXT("GraphWrite only accepts logic_spec/SemanticIR. nodes/links node creation is disabled.");
	return Result;
}

FBlueprintGenerateResult FBlueprintGraphGenerationPipeline::GenerateSemanticGraphForGraph(
	UEdGraph* TargetGraph, const TSharedPtr<FJsonObject>& GraphJsonObject,
	TArray<TSharedPtr<FUnresolvedNodeItem>>& OutUnresolvedNodes)
{
	FBlueprintGenerateResult Result;
	Result.Message = TEXT("Generation failed.");

	if (!TargetGraph || !GraphJsonObject.IsValid())
	{
		Result.Message = TEXT("Target graph or graph JSON object is invalid.");
		return Result;
	}

	const TSharedPtr<FJsonObject>* LogicSpecObject = nullptr;
	if (GraphJsonObject->TryGetObjectField(TEXT("logic_spec"), LogicSpecObject) && LogicSpecObject)
	{
		return GenerateSemanticGraphFromJsonObject(TargetGraph, GraphJsonObject, OutUnresolvedNodes);
	}

	Result.Message = TEXT("GraphWrite only accepts logic_spec/SemanticIR. nodes/links node creation is disabled.");
	return Result;
}

UEdGraph* FBlueprintGraphGenerationPipeline::FindGraphByName(UBlueprint* Blueprint, const FString& GraphName)
{
	if (!Blueprint || GraphName.IsEmpty())
	{
		return nullptr;
	}

	// EventGraph閿涙碍鎮崇槐?UbergraphPages
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

	// 娑旂喎婀?UbergraphPages 娑擃厽瀵滅划鍓р€橀崥宥囆為幖婊呭偍
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph && Graph->GetFName() == FName(*GraphName))
		{
			return Graph;
		}
	}

	// 閸戣姤鏆熼崶?
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetFName() == FName(*GraphName))
		{
			return Graph;
		}
	}

	// 鐎瑰繐娴?
	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (Graph && Graph->GetFName() == FName(*GraphName))
		{
			return Graph;
		}
	}

	// 婵梹澧粵鎯ф倳閸?
	for (UEdGraph* Graph : Blueprint->DelegateSignatureGraphs)
	{
		if (Graph && Graph->GetFName() == FName(*GraphName))
		{
			return Graph;
		}
	}

	return nullptr;
}
