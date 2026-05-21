#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "K2Node_CallFunction.h"
#include "K2Node_PromotableOperator.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionNodeSpawnerAdapter.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFragmentBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphPatternRegistry.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperSelectFragmentBuilder.h"
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
	FBlueprintHelperActionResolutionResult* OutResult,
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
		if (OutResult)
		{
			*OutResult = ActionResult;
		}
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

static bool RequireResolvedActionProvider(
	UEdGraph* TargetGraph,
	EBlueprintHelperSpawnerClusterKind ClusterKind,
	EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& Query,
	const FString& TargetPath,
	const FString& TypeName,
	FString& OutError)
{
	return RequireResolvedActionProvider(
		TargetGraph,
		ClusterKind,
		SemanticKind,
		Query,
		TargetPath,
		TypeName,
		nullptr,
		OutError);
}

static void PopulateActionProviderFragmentPins(UK2Node* Node, FBlueprintHelperNodeFragment& OutFragment)
{
	OutFragment.ExecEntryPin = FBlueprintGraphWriteFacade::FindPinByAlias(Node, TEXT("execute"));
	OutFragment.ExecExitPin = FBlueprintGraphWriteFacade::FindPinByAlias(Node, TEXT("then"));
	if (OutFragment.ExecEntryPin)
	{
		OutFragment.PinBindings.Add(TEXT("execute"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("execute"), TEXT("exec"), OutFragment.ExecEntryPin });
	}
	if (OutFragment.ExecExitPin)
	{
		OutFragment.PinBindings.Add(TEXT("then"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("then"), TEXT("exec"), OutFragment.ExecExitPin });
	}
	if (!Node)
	{
		return;
	}

	int32 DataInputIndex = 0;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			continue;
		}

		const FString PinName = Pin->PinName.ToString();
		FBlueprintHelperFragmentPinRef PinRef{ TEXT("primary"), PinName, Pin->PinType.PinCategory.ToString(), Pin };
		OutFragment.PinBindings.Add(PinName, PinRef);
		if (!OutFragment.PinBindings.Contains(PinName.ToLower()))
		{
			OutFragment.PinBindings.Add(PinName.ToLower(), FBlueprintHelperFragmentPinRef{ TEXT("primary"), PinName.ToLower(), Pin->PinType.PinCategory.ToString(), Pin });
		}
		if (Pin->Direction == EGPD_Input)
		{
			OutFragment.DataInputs.Add(PinName, PinRef);
			if (!OutFragment.DataInputs.Contains(PinName.ToLower()))
			{
				OutFragment.DataInputs.Add(PinName.ToLower(), FBlueprintHelperFragmentPinRef{ TEXT("primary"), PinName.ToLower(), Pin->PinType.PinCategory.ToString(), Pin });
			}
			if (!PinName.Equals(TEXT("self"), ESearchCase::IgnoreCase) && !OutFragment.DataInputs.Contains(TEXT("value")))
			{
				OutFragment.DataInputs.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("value"), Pin->PinType.PinCategory.ToString(), Pin });
				OutFragment.PinBindings.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("value"), Pin->PinType.PinCategory.ToString(), Pin });
			}
			if (!PinName.Equals(TEXT("self"), ESearchCase::IgnoreCase))
			{
				if (DataInputIndex == 0 && !OutFragment.DataInputs.Contains(TEXT("left")))
				{
					OutFragment.DataInputs.Add(TEXT("left"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("left"), Pin->PinType.PinCategory.ToString(), Pin });
					OutFragment.PinBindings.Add(TEXT("left"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("left"), Pin->PinType.PinCategory.ToString(), Pin });
					if (!OutFragment.DataInputs.Contains(TEXT("condition")))
					{
						OutFragment.DataInputs.Add(TEXT("condition"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("condition"), Pin->PinType.PinCategory.ToString(), Pin });
						OutFragment.PinBindings.Add(TEXT("condition"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("condition"), Pin->PinType.PinCategory.ToString(), Pin });
					}
				}
				else if (DataInputIndex == 1 && !OutFragment.DataInputs.Contains(TEXT("right")))
				{
					OutFragment.DataInputs.Add(TEXT("right"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("right"), Pin->PinType.PinCategory.ToString(), Pin });
					OutFragment.PinBindings.Add(TEXT("right"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("right"), Pin->PinType.PinCategory.ToString(), Pin });
				}
				++DataInputIndex;
			}
		}
		else if (Pin->Direction == EGPD_Output)
		{
			OutFragment.DataOutputs.Add(PinName, PinRef);
			if (!OutFragment.DataOutputs.Contains(PinName.ToLower()))
			{
				OutFragment.DataOutputs.Add(PinName.ToLower(), FBlueprintHelperFragmentPinRef{ TEXT("primary"), PinName.ToLower(), Pin->PinType.PinCategory.ToString(), Pin });
			}
			if (!OutFragment.DataOutputs.Contains(TEXT("value")))
			{
				OutFragment.DataOutputs.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("value"), Pin->PinType.PinCategory.ToString(), Pin });
				OutFragment.PinBindings.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("value"), Pin->PinType.PinCategory.ToString(), Pin });
			}
			if (!OutFragment.DataOutputs.Contains(TEXT("result")))
			{
				OutFragment.DataOutputs.Add(TEXT("result"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("result"), Pin->PinType.PinCategory.ToString(), Pin });
				OutFragment.PinBindings.Add(TEXT("result"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("result"), Pin->PinType.PinCategory.ToString(), Pin });
			}
			if (!OutFragment.DataOutputs.Contains(TEXT("return")))
			{
				OutFragment.DataOutputs.Add(TEXT("return"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("return"), Pin->PinType.PinCategory.ToString(), Pin });
				OutFragment.PinBindings.Add(TEXT("return"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("return"), Pin->PinType.PinCategory.ToString(), Pin });
			}
		}
	}
}

static UEdGraphPin* FindActionProviderDataInputPinByIndex(UK2Node* Node, const int32 RequestedIndex)
{
	if (!Node || RequestedIndex < 0)
	{
		return nullptr;
	}

	int32 DataInputIndex = 0;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin
			|| Pin->Direction != EGPD_Input
			|| Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec
			|| Pin->PinName.ToString().Equals(TEXT("self"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		if (DataInputIndex == RequestedIndex)
		{
			return Pin;
		}
		++DataInputIndex;
	}

	return nullptr;
}

static void AddLiteralDefaultForActionProviderInput(
	UK2Node* Node,
	const int32 InputIndex,
	const FString& SemanticInputName,
	const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression,
	TMap<FString, FString>& InOutDefaults)
{
	if (!Expression.IsValid() || Expression->Kind != EBlueprintHelperGraphExpressionKind::Literal)
	{
		return;
	}

	const FString LiteralValue = Expression->LiteralValue;
	if (LiteralValue.IsEmpty())
	{
		return;
	}

	if (!SemanticInputName.IsEmpty())
	{
		InOutDefaults.Add(SemanticInputName, LiteralValue);
	}

	if (UEdGraphPin* Pin = FindActionProviderDataInputPinByIndex(Node, InputIndex))
	{
		InOutDefaults.Add(Pin->PinName.ToString(), LiteralValue);
	}
}

static void CollectLiteralDefaultsForActionProviderExpression(
	UK2Node* Node,
	const FBlueprintHelperGraphExpressionIR& Expression,
	TMap<FString, FString>& OutDefaults)
{
	OutDefaults.Reset();
	AddLiteralDefaultForActionProviderInput(Node, 0, TEXT("left"), Expression.Left, OutDefaults);
	AddLiteralDefaultForActionProviderInput(Node, 1, TEXT("right"), Expression.Right, OutDefaults);
	AddLiteralDefaultForActionProviderInput(Node, 0, TEXT("condition"), Expression.Condition, OutDefaults);
	AddLiteralDefaultForActionProviderInput(Node, 0, TEXT("value"), Expression.Value, OutDefaults);
}

static bool TryBuildLiteralPromotablePinType(const FString& Type, FEdGraphPinType& OutPinType)
{
	const FString Normalized = Type.TrimStartAndEnd().ToLower();
	if (Normalized.IsEmpty())
	{
		return false;
	}

	if (Normalized == TEXT("bool") || Normalized == TEXT("boolean"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		return true;
	}
	if (Normalized == TEXT("int") || Normalized == TEXT("integer"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
		return true;
	}
	if (Normalized == TEXT("int64") || Normalized == TEXT("long"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
		return true;
	}
	if (Normalized == TEXT("float"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
		return true;
	}
	if (Normalized == TEXT("double") || Normalized == TEXT("real") || Normalized == TEXT("number"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
		return true;
	}
	if (Normalized == TEXT("string"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_String;
		return true;
	}
	if (Normalized == TEXT("name"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Name;
		return true;
	}
	if (Normalized == TEXT("text"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Text;
		return true;
	}
	return false;
}

static bool TryApplyPromotableOperatorLiteralType(
	UK2Node* Node,
	const int32 InputIndex,
	const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression)
{
	UK2Node_PromotableOperator* OperatorNode = Cast<UK2Node_PromotableOperator>(Node);
	if (!OperatorNode || !Expression.IsValid() || Expression->Kind != EBlueprintHelperGraphExpressionKind::Literal)
	{
		return false;
	}

	FEdGraphPinType PinType;
	if (!TryBuildLiteralPromotablePinType(Expression->Type, PinType))
	{
		return false;
	}

	UEdGraphPin* InputPin = FindActionProviderDataInputPinByIndex(Node, InputIndex);
	if (!InputPin || !OperatorNode->CanConvertPinType(InputPin))
	{
		return false;
	}

	OperatorNode->ConvertPinType(InputPin, PinType);
	return true;
}

static void ApplyPromotableOperatorLiteralTypes(
	UK2Node* Node,
	const FBlueprintHelperGraphExpressionIR& Expression)
{
	if (!Node || Expression.Kind != EBlueprintHelperGraphExpressionKind::Op)
	{
		return;
	}

	if (TryApplyPromotableOperatorLiteralType(Node, 0, Expression.Left))
	{
		return;
	}
	if (TryApplyPromotableOperatorLiteralType(Node, 1, Expression.Right))
	{
		return;
	}
	TryApplyPromotableOperatorLiteralType(Node, 0, Expression.Value);
}

static bool ResolveActionProviderForExpression(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphExpressionIR& Expression,
	const EBlueprintHelperActionSemanticKind SemanticKind,
	const FString& Query,
	const FString& TargetPath,
	const FString& TypeName,
	FBlueprintHelperActionResolutionResult& OutResult,
	FString& OutError)
{
	FBlueprintHelperActionResolutionRequest ActionRequest;
	ActionRequest.ClusterKind = ResolveSpawnerClusterForSemanticKind(SemanticKind);
	ActionRequest.TargetGraph = TargetGraph;
	ActionRequest.Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	ActionRequest.Semantic.Kind = SemanticKind;
	ActionRequest.Semantic.Query = Query;
	ActionRequest.Semantic.TargetPath = TargetPath;
	ActionRequest.Semantic.TypeName = TypeName;
	ActionRequest.Semantic.ExpectedReturnType = TypeName;

	OutResult = FBlueprintGraphWriteFacade::ResolveActionForGraph(ActionRequest);
	if (OutResult.IsResolved())
	{
		return true;
	}

	OutError = OutResult.Message.IsEmpty()
		? FString::Printf(
			TEXT("action provider unavailable: semantic=%s cluster=%s"),
			*FBlueprintHelperActionResolutionCore::SemanticKindToString(SemanticKind),
			*FBlueprintHelperActionResolutionCore::ClusterKindToString(OutResult.ClusterKind))
		: OutResult.Message;
	return false;
}

static FString ResolveStructExpressionTypeName(const FBlueprintHelperGraphExpressionIR& Expression)
{
	if (!Expression.Type.TrimStartAndEnd().IsEmpty())
	{
		return Expression.Type.TrimStartAndEnd();
	}
	if (!Expression.Target.TrimStartAndEnd().IsEmpty())
	{
		return Expression.Target.TrimStartAndEnd();
	}
	if (!Expression.Name.TrimStartAndEnd().IsEmpty())
	{
		return Expression.Name.TrimStartAndEnd();
	}
	if (Expression.Value.IsValid() && !Expression.Value->Type.TrimStartAndEnd().IsEmpty())
	{
		return Expression.Value->Type.TrimStartAndEnd();
	}
	if (Expression.TargetObject.IsValid() && !Expression.TargetObject->Type.TrimStartAndEnd().IsEmpty())
	{
		return Expression.TargetObject->Type.TrimStartAndEnd();
	}
	return FString();
}

static void CollectStructExpressionDefaultValues(
	const FBlueprintHelperGraphExpressionIR& Expression,
	TMap<FString, FString>& OutDefaultValues)
{
	OutDefaultValues.Reset();
	for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& FieldPair : Expression.Fields)
	{
		if (!FieldPair.Value.IsValid())
		{
			continue;
		}

		if (FieldPair.Value->Kind == EBlueprintHelperGraphExpressionKind::Literal)
		{
			OutDefaultValues.Add(FieldPair.Key, FieldPair.Value->LiteralValue);
		}
	}
}

static void PopulateStructExpressionFragment(
	const FBlueprintHelperGraphExpressionIR& Expression,
	UK2Node* SpawnedNode,
	const FString& SemanticKind,
	FBlueprintHelperNodeFragment& OutFragment)
{
	const FString ExpressionId = FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
	OutFragment.FragmentId = ExpressionId;
	OutFragment.SourceStatementId = Expression.ExpressionId;
	OutFragment.PrimaryNode = SpawnedNode;
	OutFragment.Nodes.Add(SpawnedNode);
	PopulateActionProviderFragmentPins(SpawnedNode, OutFragment);
	OutFragment.OwnershipTags.Add(TEXT("expression_id"), Expression.ExpressionId);
	OutFragment.OwnershipTags.Add(TEXT("semantic_kind"), SemanticKind);
	OutFragment.ReviewTargets.Add(Expression.ExpressionId);
}

static bool BuildConstructExpressionFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphExpressionIR& Expression,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	OutFragment = FBlueprintHelperNodeFragment();
	const FString TypeName = ResolveStructExpressionTypeName(Expression);
	if (TypeName.IsEmpty())
	{
		OutError = TEXT("construct fragment build failed: struct type is required.");
		return false;
	}

	FBlueprintHelperActionResolutionResult ActionResult;
	if (!ResolveActionProviderForExpression(
		TargetGraph,
		Expression,
		EBlueprintHelperActionSemanticKind::Construct,
		TypeName,
		TypeName,
		TypeName,
		ActionResult,
		OutError))
	{
		return false;
	}

	const FString ExpressionId = FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
	TMap<FString, FString> DefaultValues;
	CollectStructExpressionDefaultValues(Expression, DefaultValues);
	FBlueprintHelperActionNodeSpawnOptions SpawnOptions;
	SpawnOptions.NodeId = ExpressionId;
	SpawnOptions.DefaultValues = MoveTemp(DefaultValues);
	UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
		TargetGraph,
		ActionResult,
		FVector2D::ZeroVector,
		SpawnOptions,
		OutError);
	if (!SpawnedNode)
	{
		return false;
	}

	PopulateStructExpressionFragment(Expression, SpawnedNode, TEXT("construct"), OutFragment);
	return true;
}

static bool BuildDeconstructExpressionFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphExpressionIR& Expression,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	OutFragment = FBlueprintHelperNodeFragment();
	const FString TypeName = ResolveStructExpressionTypeName(Expression);
	if (TypeName.IsEmpty())
	{
		OutError = TEXT("deconstruct fragment build failed: struct type is required.");
		return false;
	}

	FBlueprintHelperActionResolutionResult ActionResult;
	if (!ResolveActionProviderForExpression(
		TargetGraph,
		Expression,
		EBlueprintHelperActionSemanticKind::Deconstruct,
		TypeName,
		TypeName,
		TypeName,
		ActionResult,
		OutError))
	{
		return false;
	}

	const FString ExpressionId = FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
	TMap<FString, FString> DefaultValues;
	CollectStructExpressionDefaultValues(Expression, DefaultValues);
	FBlueprintHelperActionNodeSpawnOptions SpawnOptions;
	SpawnOptions.NodeId = ExpressionId;
	SpawnOptions.DefaultValues = MoveTemp(DefaultValues);
	UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
		TargetGraph,
		ActionResult,
		FVector2D::ZeroVector,
		SpawnOptions,
		OutError);
	if (!SpawnedNode)
	{
		return false;
	}

	PopulateStructExpressionFragment(Expression, SpawnedNode, TEXT("deconstruct"), OutFragment);
	return true;
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

	FBlueprintHelperActionNodeSpawnOptions SpawnOptions;
	SpawnOptions.NodeId = BoundNodeData.Id;
	SpawnOptions.DefaultValues = BoundNodeData.DefaultValues;
	UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
		TargetGraph,
		ActionResult,
		FVector2D(BoundNodeData.X, BoundNodeData.Y),
		SpawnOptions,
		OutError);

	if (!SpawnedNode)
	{
		return false;
	}

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

	FBlueprintHelperActionResolutionResult ActionResult;
	if (!RequireResolvedActionProvider(
		TargetGraph,
		EBlueprintHelperSpawnerClusterKind::FieldVariableAction,
		EBlueprintHelperActionSemanticKind::Set,
		NodeData.VariableReference.VariableName,
		NodeData.VariableReference.VariableName,
		NodeData.ExpectedReturnType,
		&ActionResult,
		OutError))
	{
		return false;
	}

	FBlueprintHelperActionNodeSpawnOptions SpawnOptions;
	SpawnOptions.NodeId = NodeData.Id;
	SpawnOptions.DefaultValues = NodeData.DefaultValues;
	UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
		TargetGraph,
		ActionResult,
		FVector2D(NodeData.X, NodeData.Y),
		SpawnOptions,
		OutError);
	if (!SpawnedNode)
	{
		return false;
	}

	OutFragment.FragmentId = NodeData.Id;
	OutFragment.SourceStatementId = NodeData.Id;
	OutFragment.PrimaryNode = SpawnedNode;
	OutFragment.Nodes.Add(SpawnedNode);
	PopulateActionProviderFragmentPins(SpawnedNode, OutFragment);
	PopulateCommonFragmentMetadata(NodeData, OutFragment);
	return true;
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
	return FBlueprintHelperControlFragmentBuilder::BuildSequence(
		TargetGraph,
		FragmentId,
		OutFragment,
		OutError);
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

	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::Construct)
	{
		return BuildConstructExpressionFragment(TargetGraph, Expression, OutFragment, OutError);
	}

	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::Deconstruct)
	{
		return BuildDeconstructExpressionFragment(TargetGraph, Expression, OutFragment, OutError);
	}

	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::Select)
	{
		return FBlueprintHelperSelectFragmentBuilder::Build(TargetGraph, Expression, OutFragment, OutError);
	}

	const EBlueprintHelperActionSemanticKind SemanticKind = ResolveActionSemanticKindForExpressionKind(Expression.Kind);
	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::Get)
	{
		const FString VariableName = !Expression.ResolvedTarget.Member.IsEmpty()
			? Expression.ResolvedTarget.Member
			: Expression.Target;
		FBlueprintHelperActionResolutionResult ActionResult;
		if (!RequireResolvedActionProvider(
			TargetGraph,
			EBlueprintHelperSpawnerClusterKind::FieldVariableAction,
			EBlueprintHelperActionSemanticKind::Get,
			VariableName,
			VariableName,
			Expression.Type,
			&ActionResult,
			OutError))
		{
			return false;
		}

		UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
			TargetGraph,
			ActionResult,
			FVector2D::ZeroVector,
			OutError);
		if (!SpawnedNode)
		{
			return false;
		}

		const FString ExpressionId = FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
		OutFragment.FragmentId = ExpressionId;
		OutFragment.SourceStatementId = Expression.ExpressionId;
		OutFragment.PrimaryNode = SpawnedNode;
		OutFragment.Nodes.Add(SpawnedNode);
		PopulateActionProviderFragmentPins(SpawnedNode, OutFragment);
		return true;
	}

	if (SemanticKind != EBlueprintHelperActionSemanticKind::Unknown)
	{
		const FString Query = Expression.Kind == EBlueprintHelperGraphExpressionKind::Op
			? Expression.Operator
			: Expression.Target;
		const FString TargetPath = !Expression.ResolvedTarget.PropertyPath.IsEmpty()
			? Expression.ResolvedTarget.PropertyPath
			: Expression.Target;
		const FString TypeName = Expression.Type;
		FBlueprintHelperActionResolutionResult ActionResult;
		if (!ResolveActionProviderForExpression(
			TargetGraph,
			Expression,
			SemanticKind,
			Query,
			TargetPath,
			TypeName,
			ActionResult,
			OutError))
		{
			return false;
		}

		const FString ExpressionId = FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
		FBlueprintHelperActionNodeSpawnOptions SpawnOptions;
		SpawnOptions.NodeId = ExpressionId;
		SpawnOptions.PinNormalizationHook = [&Expression](UK2Node& SpawnedNode, const FBlueprintHelperActionNodeSpawnContext&)
		{
			ApplyPromotableOperatorLiteralTypes(&SpawnedNode, Expression);
		};
		SpawnOptions.DefaultValueProvider = [&Expression](UK2Node& SpawnedNode, const FBlueprintHelperActionNodeSpawnContext&, TMap<FString, FString>& InOutDefaults)
		{
			CollectLiteralDefaultsForActionProviderExpression(&SpawnedNode, Expression, InOutDefaults);
		};
		UK2Node* SpawnedNode = FBlueprintHelperActionNodeSpawnerAdapter::InvokeSelectedSpawner(
			TargetGraph,
			ActionResult,
			FVector2D::ZeroVector,
			SpawnOptions,
			OutError);
		if (!SpawnedNode)
		{
			return false;
		}

		OutFragment.FragmentId = ExpressionId;
		OutFragment.SourceStatementId = Expression.ExpressionId;
		OutFragment.PrimaryNode = SpawnedNode;
		OutFragment.Nodes.Add(SpawnedNode);
		PopulateActionProviderFragmentPins(SpawnedNode, OutFragment);
		OutFragment.OwnershipTags.Add(TEXT("expression_id"), Expression.ExpressionId);
		OutFragment.OwnershipTags.Add(TEXT("semantic_kind"), FBlueprintHelperActionResolutionCore::SemanticKindToString(SemanticKind));
		OutFragment.ReviewTargets.Add(Expression.ExpressionId);
		return true;
	}

	OutError = FString::Printf(
		TEXT("expression fragment pattern is not implemented yet: %s."),
		*Expression.PatternName);
	return false;
}
