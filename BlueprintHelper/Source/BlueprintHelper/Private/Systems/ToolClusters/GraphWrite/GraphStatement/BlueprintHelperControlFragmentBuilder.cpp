#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFragmentBuilder.h"

#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"

namespace
{
static FString SanitizeFragmentIdPart(const FString& Value)
{
	FString Clean = Value.TrimStartAndEnd();
	if (Clean.IsEmpty())
	{
		return TEXT("unnamed");
	}

	FString Result;
	Result.Reserve(Clean.Len());
	for (int32 Index = 0; Index < Clean.Len(); ++Index)
	{
		const TCHAR Character = Clean[Index];
		Result.AppendChar(FChar::IsAlnum(Character) ? Character : TEXT('_'));
	}
	return Result.IsEmpty() ? TEXT("unnamed") : Result;
}

static FString StatementKindName(const EBlueprintHelperGraphStatementKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperGraphStatementKind::Branch:
		return TEXT("branch");
	case EBlueprintHelperGraphStatementKind::Return:
		return TEXT("return");
	default:
		return TEXT("control");
	}
}

static FString ResolveStatementFragmentId(const FBlueprintHelperGraphStatementIR& Statement)
{
	const FString SourceId = !Statement.StatementId.IsEmpty() ? Statement.StatementId : Statement.Path;
	if (!SourceId.Contains(TEXT("$")) && !SourceId.Contains(TEXT(".")) && !SourceId.Contains(TEXT("["))
		&& !SourceId.Contains(TEXT("]")))
	{
		return SanitizeFragmentIdPart(SourceId);
	}

	const FString KindName = StatementKindName(Statement.Kind);
	return SanitizeFragmentIdPart(TEXT("stmt_") + KindName + TEXT("_") + SourceId + TEXT("_") + KindName);
}

static void AddPinAlias(
	TMap<FString, FBlueprintHelperFragmentPinRef>& PinMap,
	const FString& Alias,
	UEdGraphPin* Pin)
{
	if (Alias.IsEmpty() || !Pin)
	{
		return;
	}

	const FString Type = Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec
		? FString(TEXT("exec"))
		: Pin->PinType.PinCategory.ToString();
	const FBlueprintHelperFragmentPinRef PinRef{ TEXT("primary"), Alias, Type, Pin };
	PinMap.Add(Alias, PinRef);

	const FString LowerAlias = Alias.ToLower();
	if (!PinMap.Contains(LowerAlias))
	{
		PinMap.Add(LowerAlias, FBlueprintHelperFragmentPinRef{ TEXT("primary"), LowerAlias, Type, Pin });
	}
}

static void AddExecPinAlias(
	FBlueprintHelperNodeFragment& Fragment,
	const FString& Alias,
	UEdGraphPin* Pin)
{
	AddPinAlias(Fragment.PinBindings, Alias, Pin);
}

static void AddDataInputAlias(
	FBlueprintHelperNodeFragment& Fragment,
	const FString& Alias,
	UEdGraphPin* Pin)
{
	AddPinAlias(Fragment.PinBindings, Alias, Pin);
	AddPinAlias(Fragment.DataInputs, Alias, Pin);
}

static UEdGraphPin* FindFirstExecPin(UK2Node* Node, const EEdGraphPinDirection Direction)
{
	if (!Node)
	{
		return nullptr;
	}

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin
			&& Pin->Direction == Direction
			&& Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			return Pin;
		}
	}
	return nullptr;
}

static UEdGraphPin* FindFirstDataInputPin(UK2Node* Node)
{
	if (!Node)
	{
		return nullptr;
	}

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin
			&& Pin->Direction == EGPD_Input
			&& Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
		{
			return Pin;
		}
	}
	return nullptr;
}

static void CollectExecOutputPins(UK2Node* Node, TArray<UEdGraphPin*>& OutPins)
{
	OutPins.Reset();
	if (!Node)
	{
		return;
	}

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin
			&& Pin->Direction == EGPD_Output
			&& Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			OutPins.Add(Pin);
		}
	}
}

static bool RequireDedicatedControlBuilderBoundary(
	UEdGraph* TargetGraph,
	const FString& ControlKind,
	const FString& FragmentId,
	FString& OutError)
{
	if (!TargetGraph)
	{
		OutError = TEXT("control fragment build failed: target graph is invalid.");
		return false;
	}

	FBlueprintHelperActionResolutionRequest ActionRequest;
	ActionRequest.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	ActionRequest.TargetGraph = TargetGraph;
	ActionRequest.Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	ActionRequest.Semantic.Kind = EBlueprintHelperActionSemanticKind::Control;
	ActionRequest.Semantic.Query = ControlKind;
	ActionRequest.Semantic.TargetPath = FragmentId;

	const FBlueprintHelperActionResolutionResult ActionResult =
		FBlueprintGraphWriteFacade::ResolveActionForGraph(ActionRequest);
	if (ActionResult.Status == EBlueprintHelperActionResolutionStatus::Blocked
		&& ActionResult.ErrorCode.Equals(TEXT("dedicated_fragment_builder_required"), ESearchCase::IgnoreCase))
	{
		return true;
	}

	OutError = ActionResult.Message.IsEmpty()
		? FString::Printf(
			TEXT("control fragment build failed: ActionResolution did not expose the dedicated ControlFragmentBuilder boundary for '%s'."),
			*ControlKind)
		: ActionResult.Message;
	return false;
}

static UK2Node* SpawnControlNodeThroughSpawner(
	UEdGraph* TargetGraph,
	UClass* NodeClass,
	const FString& StableId,
	const FVector2D& Location,
	const FBlueprintHelperActionNodeSpawnOptions& SpawnOptions,
	FString& OutError)
{
	if (!NodeClass)
	{
		OutError = TEXT("control fragment build failed: control node class is invalid.");
		return nullptr;
	}

	UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(NodeClass);
	if (!NodeSpawner)
	{
		OutError = FString::Printf(
			TEXT("control fragment build failed: UE node spawner was unavailable for '%s'."),
			*NodeClass->GetName());
		return nullptr;
	}

	FBlueprintHelperActionResolutionResult ActionResult;
	ActionResult.Status = EBlueprintHelperActionResolutionStatus::Resolved;
	ActionResult.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	ActionResult.SelectedStableId = StableId;
	ActionResult.SelectedSpawner = NodeSpawner;
	return FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
		TargetGraph,
		ActionResult,
		Location,
		SpawnOptions,
		OutError);
}

template <typename TNode>
static TNode* SpawnTypedControlNode(
	UEdGraph* TargetGraph,
	const FString& ControlKind,
	const FString& FragmentId,
	const FBlueprintHelperActionNodeSpawnOptions& SpawnOptions,
	FString& OutError)
{
	UK2Node* SpawnedNode = SpawnControlNodeThroughSpawner(
		TargetGraph,
		TNode::StaticClass(),
		TEXT("control:") + ControlKind + TEXT(":") + FragmentId,
		FVector2D::ZeroVector,
		SpawnOptions,
		OutError);
	TNode* TypedNode = Cast<TNode>(SpawnedNode);
	if (!TypedNode && SpawnedNode)
	{
		OutError = FString::Printf(
			TEXT("control fragment build failed: spawner for '%s' created unexpected node class '%s'."),
			*ControlKind,
			*SpawnedNode->GetClass()->GetName());
	}
	return TypedNode;
}

template <typename TNode>
static TNode* SpawnTypedControlNode(
	UEdGraph* TargetGraph,
	const FString& ControlKind,
	const FString& FragmentId,
	FString& OutError)
{
	FBlueprintHelperActionNodeSpawnOptions SpawnOptions;
	SpawnOptions.NodeId = FragmentId;
	return SpawnTypedControlNode<TNode>(TargetGraph, ControlKind, FragmentId, SpawnOptions, OutError);
}

static void PopulateCommonControlMetadata(
	const FString& FragmentId,
	const FString& SourceStatementId,
	const FString& ControlKind,
	UK2Node* Node,
	FBlueprintHelperNodeFragment& OutFragment)
{
	OutFragment.FragmentId = FragmentId;
	OutFragment.SourceStatementId = SourceStatementId;
	OutFragment.PrimaryNode = Node;
	OutFragment.Nodes.Add(Node);
	OutFragment.OwnershipTags.Add(TEXT("semantic_kind"), TEXT("control"));
	OutFragment.OwnershipTags.Add(TEXT("control_kind"), ControlKind);
	if (!SourceStatementId.IsEmpty())
	{
		OutFragment.OwnershipTags.Add(TEXT("statement_id"), SourceStatementId);
		OutFragment.ReviewTargets.Add(SourceStatementId);
	}
	else
	{
		OutFragment.ReviewTargets.Add(FragmentId);
	}
}

static bool EnsureSequenceOutputCount(
	UK2Node_ExecutionSequence* SequenceNode,
	const int32 DesiredOutputCount,
	TArray<UEdGraphPin*>& OutOutputPins,
	FString& OutError)
{
	CollectExecOutputPins(SequenceNode, OutOutputPins);
	while (OutOutputPins.Num() < DesiredOutputCount)
	{
		SequenceNode->AddInputPin();
		CollectExecOutputPins(SequenceNode, OutOutputPins);
	}

	if (OutOutputPins.Num() < DesiredOutputCount)
	{
		OutError = FString::Printf(
			TEXT("sequence fragment build failed: expected at least %d exec outputs, got %d."),
			DesiredOutputCount,
			OutOutputPins.Num());
		return false;
	}
	return true;
}

static void PopulateSequencePins(
	UK2Node_ExecutionSequence* SequenceNode,
	const TArray<UEdGraphPin*>& OutputPins,
	FBlueprintHelperNodeFragment& OutFragment)
{
	OutFragment.ExecEntryPin = FBlueprintGraphWriteFacade::FindPinByAlias(SequenceNode, TEXT("execute"));
	if (!OutFragment.ExecEntryPin)
	{
		OutFragment.ExecEntryPin = FindFirstExecPin(SequenceNode, EGPD_Input);
	}
	AddExecPinAlias(OutFragment, TEXT("execute"), OutFragment.ExecEntryPin);
	AddExecPinAlias(OutFragment, TEXT("exec"), OutFragment.ExecEntryPin);

	if (OutputPins.Num() > 0)
	{
		OutFragment.ExecExitPin = OutputPins[0];
		AddExecPinAlias(OutFragment, TEXT("then"), OutputPins[0]);
	}

	for (int32 OutputIndex = 0; OutputIndex < OutputPins.Num(); ++OutputIndex)
	{
		UEdGraphPin* Pin = OutputPins[OutputIndex];
		if (!Pin)
		{
			continue;
		}

		AddExecPinAlias(OutFragment, Pin->PinName.ToString(), Pin);
		AddExecPinAlias(OutFragment, FString::Printf(TEXT("then_%d"), OutputIndex), Pin);
		AddExecPinAlias(OutFragment, FString::Printf(TEXT("then%d"), OutputIndex), Pin);
	}
}

static void PopulateBranchPins(
	UK2Node_IfThenElse* BranchNode,
	FBlueprintHelperNodeFragment& OutFragment)
{
	OutFragment.ExecEntryPin = FBlueprintGraphWriteFacade::FindPinByAlias(BranchNode, TEXT("execute"));
	if (!OutFragment.ExecEntryPin)
	{
		OutFragment.ExecEntryPin = FindFirstExecPin(BranchNode, EGPD_Input);
	}
	AddExecPinAlias(OutFragment, TEXT("execute"), OutFragment.ExecEntryPin);
	AddExecPinAlias(OutFragment, TEXT("exec"), OutFragment.ExecEntryPin);

	UEdGraphPin* ThenPin = FBlueprintGraphWriteFacade::FindPinByAlias(BranchNode, TEXT("then"));
	UEdGraphPin* ElsePin = FBlueprintGraphWriteFacade::FindPinByAlias(BranchNode, TEXT("else"));
	OutFragment.ExecExitPin = ThenPin;
	AddExecPinAlias(OutFragment, TEXT("then"), ThenPin);
	AddExecPinAlias(OutFragment, TEXT("true"), ThenPin);
	AddExecPinAlias(OutFragment, TEXT("else"), ElsePin);
	AddExecPinAlias(OutFragment, TEXT("false"), ElsePin);

	if (UEdGraphPin* ConditionPin = FBlueprintGraphWriteFacade::FindPinByAlias(BranchNode, TEXT("condition")))
	{
		AddDataInputAlias(OutFragment, TEXT("condition"), ConditionPin);
		AddDataInputAlias(OutFragment, ConditionPin->PinName.ToString(), ConditionPin);
	}
}

static void PopulateReturnPins(
	UK2Node_FunctionResult* ReturnNode,
	FBlueprintHelperNodeFragment& OutFragment)
{
	OutFragment.ExecEntryPin = FBlueprintGraphWriteFacade::FindPinByAlias(ReturnNode, TEXT("execute"));
	if (!OutFragment.ExecEntryPin)
	{
		OutFragment.ExecEntryPin = FindFirstExecPin(ReturnNode, EGPD_Input);
	}
	OutFragment.ExecExitPin = nullptr;
	AddExecPinAlias(OutFragment, TEXT("execute"), OutFragment.ExecEntryPin);
	AddExecPinAlias(OutFragment, TEXT("exec"), OutFragment.ExecEntryPin);

	UEdGraphPin* FirstDataPin = nullptr;
	for (UEdGraphPin* Pin : ReturnNode->Pins)
	{
		if (!Pin
			|| Pin->Direction != EGPD_Input
			|| Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			continue;
		}

		if (!FirstDataPin)
		{
			FirstDataPin = Pin;
		}
		AddDataInputAlias(OutFragment, Pin->PinName.ToString(), Pin);
	}

	AddDataInputAlias(OutFragment, TEXT("value"), FirstDataPin);
	AddDataInputAlias(OutFragment, TEXT("return"), FirstDataPin);
	AddDataInputAlias(OutFragment, TEXT("result"), FirstDataPin);
}

static void CollectBranchLiteralDefaults(
	const FBlueprintHelperGraphStatementIR& Statement,
	TMap<FString, FString>& OutDefaults)
{
	OutDefaults.Reset();
	if (!Statement.Condition.IsValid()
		|| Statement.Condition->Kind != EBlueprintHelperGraphExpressionKind::Literal
		|| Statement.Condition->LiteralValue.IsEmpty())
	{
		return;
	}

	OutDefaults.Add(TEXT("Condition"), Statement.Condition->LiteralValue);
	OutDefaults.Add(TEXT("condition"), Statement.Condition->LiteralValue);
}

static void CollectReturnLiteralDefault(
	UK2Node_FunctionResult* ReturnNode,
	const FBlueprintHelperGraphStatementIR& Statement,
	TMap<FString, FString>& InOutDefaults)
{
	if (!ReturnNode
		|| !Statement.Value.IsValid()
		|| Statement.Value->Kind != EBlueprintHelperGraphExpressionKind::Literal
		|| Statement.Value->LiteralValue.IsEmpty())
	{
		return;
	}

	UEdGraphPin* ValuePin = FindFirstDataInputPin(ReturnNode);
	if (!ValuePin)
	{
		return;
	}

	InOutDefaults.Add(ValuePin->PinName.ToString(), Statement.Value->LiteralValue);
	InOutDefaults.Add(TEXT("value"), Statement.Value->LiteralValue);
	InOutDefaults.Add(TEXT("return"), Statement.Value->LiteralValue);
}
}

bool FBlueprintHelperControlFragmentBuilder::BuildSequence(
	UEdGraph* TargetGraph,
	const FString& FragmentId,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	OutFragment = FBlueprintHelperNodeFragment();
	OutError.Reset();

	const FString CleanFragmentId = SanitizeFragmentIdPart(FragmentId);
	if (!RequireDedicatedControlBuilderBoundary(TargetGraph, TEXT("sequence"), CleanFragmentId, OutError))
	{
		return false;
	}

	UK2Node_ExecutionSequence* SequenceNode = SpawnTypedControlNode<UK2Node_ExecutionSequence>(
		TargetGraph,
		TEXT("sequence"),
		CleanFragmentId,
		OutError);
	if (!SequenceNode)
	{
		return false;
	}

	TArray<UEdGraphPin*> OutputPins;
	if (!EnsureSequenceOutputCount(SequenceNode, 2, OutputPins, OutError))
	{
		return false;
	}

	PopulateCommonControlMetadata(CleanFragmentId, CleanFragmentId, TEXT("sequence"), SequenceNode, OutFragment);
	PopulateSequencePins(SequenceNode, OutputPins, OutFragment);
	return true;
}

bool FBlueprintHelperControlFragmentBuilder::BuildBranch(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphStatementIR& Statement,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	OutFragment = FBlueprintHelperNodeFragment();
	OutError.Reset();

	if (Statement.Kind != EBlueprintHelperGraphStatementKind::Branch)
	{
		OutError = TEXT("branch control fragment build failed: statement kind is not Branch.");
		return false;
	}

	const FString FragmentId = ResolveStatementFragmentId(Statement);
	if (!RequireDedicatedControlBuilderBoundary(TargetGraph, TEXT("branch"), FragmentId, OutError))
	{
		return false;
	}

	TMap<FString, FString> DefaultValues;
	CollectBranchLiteralDefaults(Statement, DefaultValues);
	FBlueprintHelperActionNodeSpawnOptions SpawnOptions;
	SpawnOptions.NodeId = FragmentId;
	SpawnOptions.DefaultValues = MoveTemp(DefaultValues);
	UK2Node_IfThenElse* BranchNode = SpawnTypedControlNode<UK2Node_IfThenElse>(
		TargetGraph,
		TEXT("branch"),
		FragmentId,
		SpawnOptions,
		OutError);
	if (!BranchNode)
	{
		return false;
	}

	PopulateCommonControlMetadata(FragmentId, Statement.StatementId, TEXT("branch"), BranchNode, OutFragment);
	PopulateBranchPins(BranchNode, OutFragment);
	return true;
}

bool FBlueprintHelperControlFragmentBuilder::BuildReturn(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphStatementIR& Statement,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	OutFragment = FBlueprintHelperNodeFragment();
	OutError.Reset();

	if (Statement.Kind != EBlueprintHelperGraphStatementKind::Return)
	{
		OutError = TEXT("return control fragment build failed: statement kind is not Return.");
		return false;
	}

	const FString FragmentId = ResolveStatementFragmentId(Statement);
	if (!RequireDedicatedControlBuilderBoundary(TargetGraph, TEXT("return"), FragmentId, OutError))
	{
		return false;
	}

	FBlueprintHelperActionNodeSpawnOptions SpawnOptions;
	SpawnOptions.NodeId = FragmentId;
	SpawnOptions.DefaultValueProvider = [&Statement](UK2Node& SpawnedNode, const FBlueprintHelperActionNodeSpawnContext&, TMap<FString, FString>& InOutDefaults)
	{
		if (UK2Node_FunctionResult* FunctionResultNode = Cast<UK2Node_FunctionResult>(&SpawnedNode))
		{
			CollectReturnLiteralDefault(FunctionResultNode, Statement, InOutDefaults);
		}
	};
	UK2Node_FunctionResult* ReturnNode = SpawnTypedControlNode<UK2Node_FunctionResult>(
		TargetGraph,
		TEXT("return"),
		FragmentId,
		SpawnOptions,
		OutError);
	if (!ReturnNode)
	{
		return false;
	}

	PopulateCommonControlMetadata(FragmentId, Statement.StatementId, TEXT("return"), ReturnNode, OutFragment);
	PopulateReturnPins(ReturnNode, OutFragment);
	return true;
}

bool FBlueprintHelperControlFragmentBuilder::BuildStatement(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphStatementIR& Statement,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	switch (Statement.Kind)
	{
	case EBlueprintHelperGraphStatementKind::Branch:
		return BuildBranch(TargetGraph, Statement, OutFragment, OutError);
	case EBlueprintHelperGraphStatementKind::Return:
		return BuildReturn(TargetGraph, Statement, OutFragment, OutError);
	default:
		OutFragment = FBlueprintHelperNodeFragment();
		OutError = FString::Printf(
			TEXT("control fragment build failed: unsupported statement kind '%s'."),
			*Statement.PatternName);
		return false;
	}
}
