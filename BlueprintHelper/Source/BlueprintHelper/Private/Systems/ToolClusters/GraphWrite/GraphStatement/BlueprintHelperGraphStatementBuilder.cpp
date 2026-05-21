#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphPatternRegistry.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementTypeUtils.h"

static void PopulateCallFragmentPins(UK2Node* CallNode, FBlueprintHelperNodeFragment& OutFragment)
{
	OutFragment.ExecEntryPin = FBlueprintGraphWriteFacade::FindPinByAlias(CallNode, TEXT("execute"));
	OutFragment.ExecExitPin = FBlueprintGraphWriteFacade::FindPinByAlias(CallNode, TEXT("then"));
	OutFragment.PinBindings.Add(TEXT("execute"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("execute"), TEXT("exec"), OutFragment.ExecEntryPin });
	OutFragment.PinBindings.Add(TEXT("then"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("then"), TEXT("exec"), OutFragment.ExecExitPin });
	if (!CallNode)
	{
		return;
	}

	for (UEdGraphPin* Pin : CallNode->Pins)
	{
		if (!Pin || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			continue;
		}

		const FString PinName = Pin->PinName.ToString();
		FBlueprintHelperFragmentPinRef PinRef{ TEXT("primary"), PinName, Pin->PinType.PinCategory.ToString(), Pin };
		OutFragment.PinBindings.Add(PinName, PinRef);
		if (Pin->Direction == EGPD_Input)
		{
			OutFragment.DataInputs.Add(PinName, PinRef);
		}
		else if (Pin->Direction == EGPD_Output)
		{
			OutFragment.DataOutputs.Add(PinName, PinRef);
			if (!OutFragment.DataOutputs.Contains(TEXT("return")))
			{
				OutFragment.DataOutputs.Add(TEXT("return"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("return"), Pin->PinType.PinCategory.ToString(), Pin });
				OutFragment.PinBindings.Add(TEXT("return"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("return"), Pin->PinType.PinCategory.ToString(), Pin });
			}
			if (!OutFragment.DataOutputs.Contains(TEXT("result")))
			{
				OutFragment.DataOutputs.Add(TEXT("result"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("result"), Pin->PinType.PinCategory.ToString(), Pin });
				OutFragment.PinBindings.Add(TEXT("result"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("result"), Pin->PinType.PinCategory.ToString(), Pin });
			}
		}
	}
}

static void PopulateCommonFragmentMetadata(const FParsedNode& NodeData, FBlueprintHelperNodeFragment& OutFragment)
{
	OutFragment.OwnershipTags.Add(TEXT("statement_id"), NodeData.Id);
	OutFragment.ReviewTargets.Add(NodeData.Id);
	// DEPRECATED_LAYOUT: these x/y hints are legacy spawn metadata only.
	// Final node positions must come from the UE-side GraphLayout system.
	OutFragment.LayoutHints.Add(TEXT("x"), LexToString(NodeData.X));
	OutFragment.LayoutHints.Add(TEXT("y"), LexToString(NodeData.Y));
}

static void ApplyCallPatternBindings(FParsedNode& NodeData)
{
	FBlueprintHelperGraphPatternRegistry& Registry = FBlueprintHelperGraphPatternRegistry::Get();

	FString ObjectName;
	FString FunctionName;
	if (FBlueprintHelperCallFunctionResolver::TryParseQualifiedQuery(NodeData.FunctionName, ObjectName, FunctionName))
	{
		FunctionName = Registry.ResolveAlias(TEXT("call"), FunctionName);
		NodeData.FunctionName = ObjectName + TEXT(".") + FunctionName;
	}
	else
	{
		NodeData.FunctionName = Registry.ResolveAlias(TEXT("call"), NodeData.FunctionName);
	}

	Registry.ApplyPinAliases(TEXT("call"), NodeData.DefaultValues);
	Registry.ApplyPinAliases(TEXT("call"), NodeData.ArgumentTypes);
}

static void ApplyCallPatternDefaults(FParsedNode& NodeData)
{
	FBlueprintHelperGraphPatternRegistry::Get().ApplyDefaults(TEXT("call"), NodeData.DefaultValues);
}

static FString MakeCallFunctionResolveQuery(const FParsedNode& NodeData)
{
	const FString StableId = NodeData.ResolvedCallFunctionStableId.TrimStartAndEnd();
	return StableId.IsEmpty() ? NodeData.FunctionName : StableId;
}

static void AppendCandidateActionGroup(
	const FString& Target,
	const FBlueprintHelperActionResolutionResult& ResolveResult,
	TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions)
{
	if (!OutCandidateFunctions || ResolveResult.CandidateActions.Num() == 0)
	{
		return;
	}

	FBlueprintHelperCandidateFunctionGroup Group;
	Group.Target = Target;
	Group.Candidates = ResolveResult.CandidateActions;
	OutCandidateFunctions->Add(MoveTemp(Group));
}

static EBlueprintHelperActionSemanticKind ResolveActionSemanticKindForExpressionKind(EBlueprintHelperGraphExpressionKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperGraphExpressionKind::Get:
		return EBlueprintHelperActionSemanticKind::Get;
	case EBlueprintHelperGraphExpressionKind::GetProperty:
		return EBlueprintHelperActionSemanticKind::GetProperty;
	case EBlueprintHelperGraphExpressionKind::Op:
		return EBlueprintHelperActionSemanticKind::Op;
	case EBlueprintHelperGraphExpressionKind::Construct:
		return EBlueprintHelperActionSemanticKind::Construct;
	case EBlueprintHelperGraphExpressionKind::Deconstruct:
		return EBlueprintHelperActionSemanticKind::Deconstruct;
	case EBlueprintHelperGraphExpressionKind::Select:
		return EBlueprintHelperActionSemanticKind::Select;
	default:
		return EBlueprintHelperActionSemanticKind::Unknown;
	}
}

static EBlueprintHelperSpawnerClusterKind ResolveSpawnerClusterForSemanticKind(EBlueprintHelperActionSemanticKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperActionSemanticKind::Call:
	case EBlueprintHelperActionSemanticKind::Op:
		return EBlueprintHelperSpawnerClusterKind::FunctionAction;
	case EBlueprintHelperActionSemanticKind::Get:
	case EBlueprintHelperActionSemanticKind::Set:
	case EBlueprintHelperActionSemanticKind::GetProperty:
	case EBlueprintHelperActionSemanticKind::SetProperty:
		return EBlueprintHelperSpawnerClusterKind::FieldVariableAction;
	case EBlueprintHelperActionSemanticKind::Event:
	case EBlueprintHelperActionSemanticKind::ComponentBoundEvent:
	case EBlueprintHelperActionSemanticKind::Bind:
		return EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
	case EBlueprintHelperActionSemanticKind::Construct:
	case EBlueprintHelperActionSemanticKind::Deconstruct:
	case EBlueprintHelperActionSemanticKind::Select:
	case EBlueprintHelperActionSemanticKind::Control:
	case EBlueprintHelperActionSemanticKind::Create:
	case EBlueprintHelperActionSemanticKind::Convert:
	case EBlueprintHelperActionSemanticKind::Schedule:
		return EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	default:
		return EBlueprintHelperSpawnerClusterKind::Unknown;
	}
}

static bool RequireResolvedActionProvider(
	UEdGraph* TargetGraph,
	EBlueprintHelperSpawnerClusterKind ClusterKind,
	EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& Query,
	const FString& TargetPath,
	const FString& TypeName,
	FString& OutError)
{
	FBlueprintHelperActionResolutionRequest ActionRequest;
	ActionRequest.ClusterKind = ClusterKind;
	ActionRequest.TargetGraph = TargetGraph;
	ActionRequest.Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	ActionRequest.Semantic.Kind = SemanticKind;
	ActionRequest.Semantic.Query = Query;
	ActionRequest.Semantic.TargetPath = TargetPath;
	ActionRequest.Semantic.TypeName = TypeName;

	const FBlueprintHelperActionResolutionResult ActionResult =
		FBlueprintGraphWriteFacade::ResolveActionForGraph(ActionRequest);
	if (ActionResult.IsResolved())
	{
		return true;
	}

	OutError = ActionResult.Message.IsEmpty()
		? FString::Printf(
			TEXT("action provider unavailable: semantic=%s cluster=%s"),
			*FBlueprintHelperActionResolutionCore::SemanticKindToString(SemanticKind),
			*FBlueprintHelperActionResolutionCore::ClusterKindToString(ActionResult.ClusterKind))
		: ActionResult.Message;
	return false;
}
bool FBlueprintHelperGraphStatementBuilder::BuildCallFunctionFragment(
	UEdGraph* TargetGraph,
	const FParsedNode& NodeData,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError,
	TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions)
{
	OutFragment = FBlueprintHelperNodeFragment();
	FParsedNode BoundNodeData = NodeData;
	ApplyCallPatternBindings(BoundNodeData);

	const FString ExplicitTargetObjectName = BoundNodeData.TargetObjectName.TrimStartAndEnd();

	FBlueprintHelperActionResolutionRequest ActionRequest;
	ActionRequest.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	ActionRequest.TargetGraph = TargetGraph;
	ActionRequest.Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	ActionRequest.Semantic.Kind = EBlueprintHelperActionSemanticKind::Call;
	ActionRequest.Semantic.Query = MakeCallFunctionResolveQuery(BoundNodeData);
	ActionRequest.Semantic.TargetPath = ExplicitTargetObjectName;
	ActionRequest.Semantic.SearchMode = BoundNodeData.SearchMode;
	ActionRequest.Semantic.AmbiguityPolicy = BoundNodeData.AmbiguityPolicy;
	ActionRequest.Semantic.CategoryPriority = BoundNodeData.CategoryPriority;
	ActionRequest.Semantic.ArgumentTypes = BoundNodeData.ArgumentTypes;
	ActionRequest.Semantic.ArgumentPinTypes = BoundNodeData.ArgumentPinTypes;
	ActionRequest.Semantic.TargetObjectType = BoundNodeData.TargetObjectType;
	ActionRequest.Semantic.TargetObjectPinType = BoundNodeData.TargetObjectPinType;
	ActionRequest.Semantic.ExpectedReturnType = BoundNodeData.ExpectedReturnType;
	ActionRequest.Semantic.ExpectedReturnPinType = BoundNodeData.ExpectedReturnPinType;
	BoundNodeData.DefaultValues.GetKeys(ActionRequest.Semantic.ArgumentNames);
	ActionRequest.Semantic.DefaultValues = BoundNodeData.DefaultValues;
	const FBlueprintHelperActionResolutionResult ActionResult =
		FBlueprintGraphWriteFacade::ResolveActionForGraph(ActionRequest);

	if (!ActionResult.IsResolved())
	{
		AppendCandidateActionGroup(BoundNodeData.FunctionName, ActionResult, OutCandidateFunctions);
		OutError = ActionResult.Message.IsEmpty()
			? FString::Printf(TEXT("call_function resolve failed: %s"), *BoundNodeData.FunctionName)
			: ActionResult.Message;
		return false;
	}

	ApplyCallPatternDefaults(BoundNodeData);

	UK2Node* SpawnedNode = FBlueprintHelperCallFunctionResolver::SpawnResolvedNode(
		TargetGraph,
		ActionResult.FunctionCandidate,
		FVector2D(BoundNodeData.X, BoundNodeData.Y),
		OutError);

	if (!SpawnedNode)
	{
		return false;
	}

	FBlueprintGraphWriteFacade::ApplyDefaultValues(SpawnedNode, BoundNodeData.DefaultValues, BoundNodeData.Id);

	OutFragment.FragmentId = BoundNodeData.Id;
	OutFragment.SourceStatementId = BoundNodeData.Id;
	OutFragment.PrimaryNode = SpawnedNode;
	OutFragment.Nodes.Add(SpawnedNode);
	PopulateCallFragmentPins(SpawnedNode, OutFragment);
	PopulateCommonFragmentMetadata(BoundNodeData, OutFragment);
	return true;
}

bool FBlueprintHelperGraphStatementBuilder::BuildVariableSetFragment(
	UEdGraph* TargetGraph,
	const FParsedNode& NodeData,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	OutFragment = FBlueprintHelperNodeFragment();

	if (!RequireResolvedActionProvider(
		TargetGraph,
		EBlueprintHelperSpawnerClusterKind::FieldVariableAction,
		EBlueprintHelperActionSemanticKind::Set,
		NodeData.VariableReference.VariableName,
		NodeData.VariableReference.VariableName,
		NodeData.ExpectedReturnType,
		OutError))
	{
		return false;
	}

	OutError = TEXT("set fragment provider resolved, but FragmentDAG emission has not migrated to the spawner cluster path.");
	return false;
}

bool FBlueprintHelperGraphStatementBuilder::BuildSetPropertyFragment(
	UEdGraph* TargetGraph,
	const FParsedNode& NodeData,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	OutFragment = FBlueprintHelperNodeFragment();
	if (NodeData.VariableReference.VariableName.TrimStartAndEnd().IsEmpty())
	{
		OutError = TEXT("set_property fragment build failed: graph-body property target is empty.");
		return false;
	}

	if (!RequireResolvedActionProvider(
		TargetGraph,
		EBlueprintHelperSpawnerClusterKind::FieldVariableAction,
		EBlueprintHelperActionSemanticKind::SetProperty,
		NodeData.VariableReference.VariableName,
		NodeData.VariableReference.VariableName,
		NodeData.ExpectedReturnType,
		OutError))
	{
		return false;
	}

	OutError = TEXT("set_property fragment provider resolved, but FragmentDAG emission has not migrated to the spawner cluster path.");
	return false;
}

bool FBlueprintHelperGraphStatementBuilder::BuildSequenceFragment(
	UEdGraph* TargetGraph,
	const FString& FragmentId,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	OutFragment = FBlueprintHelperNodeFragment();
	if (!RequireResolvedActionProvider(
		TargetGraph,
		EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction,
		EBlueprintHelperActionSemanticKind::Control,
		TEXT("sequence"),
		FragmentId,
		FString(),
		OutError))
	{
		return false;
	}

	OutError = TEXT("sequence fragment provider resolved, but FragmentDAG emission has not migrated to the spawner cluster path.");
	return false;
}

bool FBlueprintHelperGraphStatementBuilder::BuildExpressionFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphExpressionIR& Expression,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError,
	TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions)
{
	OutFragment = FBlueprintHelperNodeFragment();
	OutError.Reset();

	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::Literal)
	{
		OutError = TEXT("literal expression does not create a graph fragment.");
		return false;
	}

	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::Call)
	{
		FParsedNode NodeData;
		NodeData.Id = FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
		NodeData.NodeType = EParsedBlueprintNodeType::CallFunction;
		NodeData.SourceType = TEXT("K2Node_CallFunction");
		NodeData.FunctionName = Expression.Target;
		NodeData.SearchMode = Expression.SearchMode;
		NodeData.AmbiguityPolicy = Expression.AmbiguityPolicy;
		NodeData.CategoryPriority = Expression.CategoryPriority;
		NodeData.ExpectedReturnType = Expression.Type;
		if (Expression.TargetObject.IsValid())
		{
			NodeData.TargetObjectName = Expression.TargetObject->Target;
		}
		for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& ArgPair : Expression.Args)
		{
			if (!ArgPair.Value.IsValid())
			{
				continue;
			}
			if (ArgPair.Value->Kind == EBlueprintHelperGraphExpressionKind::Literal)
			{
				NodeData.DefaultValues.Add(ArgPair.Key, ArgPair.Value->LiteralValue);
			}
			if (!ArgPair.Value->Type.TrimStartAndEnd().IsEmpty())
			{
				NodeData.ArgumentTypes.Add(ArgPair.Key, ArgPair.Value->Type);
			}
		}
		if (!BuildCallFunctionFragment(TargetGraph, NodeData, OutFragment, OutError, OutCandidateFunctions))
		{
			return false;
		}
		OutFragment.SourceStatementId = Expression.ExpressionId;
		return true;
	}

	const EBlueprintHelperActionSemanticKind SemanticKind = ResolveActionSemanticKindForExpressionKind(Expression.Kind);
	if (SemanticKind != EBlueprintHelperActionSemanticKind::Unknown)
	{
		const FString Query = Expression.Kind == EBlueprintHelperGraphExpressionKind::Op
			? Expression.Operator
			: Expression.Target;
		const FString TargetPath = !Expression.ResolvedTarget.PropertyPath.IsEmpty()
			? Expression.ResolvedTarget.PropertyPath
			: Expression.Target;
		const FString TypeName = Expression.Type;
		if (!RequireResolvedActionProvider(TargetGraph, ResolveSpawnerClusterForSemanticKind(SemanticKind), SemanticKind, Query, TargetPath, TypeName, OutError))
		{
			return false;
		}

		OutError = TEXT("expression provider resolved, but FragmentDAG emission has not migrated to the spawner cluster path.");
		return false;
	}

	OutError = FString::Printf(
		TEXT("expression fragment pattern is not implemented yet: %s."),
		*Expression.PatternName);
	return false;
}
