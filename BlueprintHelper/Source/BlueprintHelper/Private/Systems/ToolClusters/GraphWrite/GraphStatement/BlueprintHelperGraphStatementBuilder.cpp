#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphPatternRegistry.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/NodeHandlers/PromotableOperatorNodeHandler.h"
#include "Systems/ToolClusters/GraphWrite/NodeHandlers/SelectNodeHandler.h"
#include "Systems/ToolClusters/GraphWrite/NodeHandlers/SequenceNodeHandler.h"
#include "Systems/ToolClusters/GraphWrite/NodeHandlers/StructOperationNodeHandler.h"

namespace
{
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

	Registry.ApplyPinAliasesAndDefaults(TEXT("call"), NodeData.DefaultValues);
}

static bool TryParseExplicitObjectCall(
	const FBlueprintHelperCallFunctionResolveResult& ResolveResult,
	const FString& FunctionQuery,
	FString& OutObjectName,
	FString& OutFunctionName)
{
	OutObjectName.Reset();
	OutFunctionName.Reset();

	if (!ResolveResult.ErrorCode.Equals(TEXT("explicit_member_call_not_supported"), ESearchCase::IgnoreCase))
	{
		return false;
	}

	if (!FBlueprintHelperCallFunctionResolver::TryParseQualifiedQuery(FunctionQuery, OutObjectName, OutFunctionName))
	{
		return false;
	}

	OutObjectName.TrimStartAndEndInline();
	OutFunctionName.TrimStartAndEndInline();
	return !OutObjectName.IsEmpty()
		&& !OutFunctionName.IsEmpty()
		&& !OutObjectName.StartsWith(TEXT("/Script/"));
}

static UEdGraphPin* FindFirstDataOutputPin(UK2Node* Node)
{
	if (!Node)
	{
		return nullptr;
	}

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Output && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
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
		if (Pin && Pin->Direction == EGPD_Input && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
		{
			return Pin;
		}
	}
	return nullptr;
}

static UEdGraphPin* FindCallTargetPin(UK2Node* CallNode)
{
	if (!CallNode)
	{
		return nullptr;
	}

	const TCHAR* AliasCandidates[] = {
		TEXT("self"),
		TEXT("target"),
		TEXT("Target"),
		TEXT("object"),
		TEXT("Object"),
	};
	for (const TCHAR* Alias : AliasCandidates)
	{
		if (UEdGraphPin* Pin = FBlueprintGraphWriteFacade::FindPinByAlias(CallNode, Alias))
		{
			if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
			{
				return Pin;
			}
		}
	}

	for (UEdGraphPin* Pin : CallNode->Pins)
	{
		if (Pin
			&& Pin->Direction == EGPD_Input
			&& Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec
			&& (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object
				|| Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Interface))
		{
			return Pin;
		}
	}
	return nullptr;
}

static bool TryConnectDataPins(
	UEdGraph* TargetGraph,
	UEdGraphPin* FromPin,
	UEdGraphPin* ToPin,
	const FString& Context,
	FString& OutError)
{
	if (!FromPin || !ToPin)
	{
		OutError = FString::Printf(TEXT("%s failed: source or target pin is invalid."), *Context);
		return false;
	}

	const UEdGraphSchema* Schema = TargetGraph ? TargetGraph->GetSchema() : nullptr;
	if (!Schema)
	{
		OutError = FString::Printf(TEXT("%s failed: graph schema is invalid."), *Context);
		return false;
	}

	const FPinConnectionResponse ConnectionResponse = Schema->CanCreateConnection(FromPin, ToPin);
	if (Schema->TryCreateConnection(FromPin, ToPin))
	{
		return true;
	}

	OutError = ConnectionResponse.Message.IsEmpty()
		? FString::Printf(TEXT("%s rejected: %s -> %s."), *Context, *FromPin->PinName.ToString(), *ToPin->PinName.ToString())
		: ConnectionResponse.Message.ToString();
	return false;
}

static bool SpawnExplicitObjectCallFragment(
	UEdGraph* TargetGraph,
	const FParsedNode& NodeData,
	const FString& ObjectName,
	const FString& FunctionName,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	const FBlueprintHelperCallFunctionResolveResult ObjectCallResolveResult =
		FBlueprintGraphWriteFacade::ResolveFunctionForGraph(TargetGraph, FunctionName, NodeData.DefaultValues);
	if (!ObjectCallResolveResult.IsResolved())
	{
		OutError = ObjectCallResolveResult.Message.IsEmpty()
			? FString::Printf(TEXT("explicit object call resolve failed: %s"), *FunctionName)
			: ObjectCallResolveResult.Message;
		return false;
	}

	UK2Node* CallNode = FBlueprintHelperCallFunctionResolver::SpawnResolvedNode(
		TargetGraph,
		ObjectCallResolveResult.Selected,
		FVector2D(NodeData.X, NodeData.Y),
		OutError);
	if (!CallNode)
	{
		return false;
	}

	FBlueprintGraphWriteFacade::ApplyDefaultValues(CallNode, NodeData.DefaultValues, NodeData.Id);

	FParsedNode ObjectGetterData;
	ObjectGetterData.Id = NodeData.Id + TEXT("_target");
	ObjectGetterData.NodeType = EParsedBlueprintNodeType::VariableGet;
	ObjectGetterData.SourceType = TEXT("K2Node_VariableGet");
	ObjectGetterData.X = NodeData.X - 260.0f;
	ObjectGetterData.Y = NodeData.Y;
	ObjectGetterData.VariableReference.ScopeType = TEXT("member");
	ObjectGetterData.VariableReference.VariableName = ObjectName;
	ObjectGetterData.VariableReference.bSelfContext = true;

	FString ObjectGetterError;
	UK2Node* ObjectGetterNode = FBlueprintGraphWriteFacade::SpawnVariableGetNode(TargetGraph, ObjectGetterData, ObjectGetterError);
	if (!ObjectGetterNode)
	{
		OutError = ObjectGetterError.IsEmpty()
			? FString::Printf(TEXT("explicit object call target not found: %s"), *ObjectName)
			: ObjectGetterError;
		return false;
	}

	UEdGraphPin* ObjectOutputPin = FindFirstDataOutputPin(ObjectGetterNode);
	UEdGraphPin* CallTargetPin = FindCallTargetPin(CallNode);
	if (!ObjectOutputPin || !CallTargetPin)
	{
		OutError = FString::Printf(
			TEXT("explicit object call target pin not found: %s.%s"),
			*ObjectName,
			*FunctionName);
		return false;
	}

	const UEdGraphSchema* Schema = TargetGraph ? TargetGraph->GetSchema() : nullptr;
	if (!Schema)
	{
		OutError = TEXT("explicit object call connection failed: graph schema is invalid.");
		return false;
	}

	const FPinConnectionResponse ConnectionResponse = Schema->CanCreateConnection(ObjectOutputPin, CallTargetPin);
	if (!Schema->TryCreateConnection(ObjectOutputPin, CallTargetPin))
	{
		OutError = ConnectionResponse.Message.IsEmpty()
			? FString::Printf(TEXT("explicit object call connection rejected: %s.%s"), *ObjectName, *FunctionName)
			: ConnectionResponse.Message.ToString();
		return false;
	}

	OutFragment.FragmentId = NodeData.Id;
	OutFragment.SourceStatementId = NodeData.Id;
	OutFragment.PrimaryNode = CallNode;
	OutFragment.Nodes.Add(ObjectGetterNode);
	OutFragment.Nodes.Add(CallNode);
	PopulateCallFragmentPins(CallNode, OutFragment);
	PopulateCommonFragmentMetadata(NodeData, OutFragment);
	OutFragment.DataInputs.Add(ObjectName, FBlueprintHelperFragmentPinRef{ ObjectGetterData.Id, ObjectName, TEXT("object"), ObjectOutputPin });
	OutFragment.InternalLinks.Add(FBlueprintHelperFragmentLink{
		FBlueprintHelperFragmentPinRef{ ObjectGetterData.Id, ObjectName, TEXT("object"), ObjectOutputPin },
		FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("target"), TEXT("object"), CallTargetPin }
	});
	return true;
}

static FString SanitizeGraphFragmentIdPart(const FString& Value)
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

static FString ExpressionKindName(const EBlueprintHelperGraphExpressionKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperGraphExpressionKind::Literal:
		return TEXT("literal");
	case EBlueprintHelperGraphExpressionKind::Get:
		return TEXT("get");
	case EBlueprintHelperGraphExpressionKind::GetProperty:
		return TEXT("get_property");
	case EBlueprintHelperGraphExpressionKind::Ref:
		return TEXT("ref");
	case EBlueprintHelperGraphExpressionKind::Call:
		return TEXT("call");
	case EBlueprintHelperGraphExpressionKind::Compare:
		return TEXT("compare");
	case EBlueprintHelperGraphExpressionKind::Select:
		return TEXT("select");
	case EBlueprintHelperGraphExpressionKind::MakeStruct:
		return TEXT("make_struct");
	default:
		return TEXT("unknown");
	}
}

static FString MakeExpressionFragmentId(const FBlueprintHelperGraphExpressionIR& Expression)
{
	const FString SourceId = !Expression.ExpressionId.IsEmpty() ? Expression.ExpressionId : Expression.Path;
	if (!SourceId.Contains(TEXT("$")) && !SourceId.Contains(TEXT(".")) && !SourceId.Contains(TEXT("[")) && !SourceId.Contains(TEXT("]")))
	{
		return SanitizeGraphFragmentIdPart(SourceId);
	}

	const FString Suffix = ExpressionKindName(Expression.Kind);
	return SanitizeGraphFragmentIdPart(TEXT("expr_") + Suffix + TEXT("_") + SourceId + TEXT("_") + Suffix);
}

static FString NormalizeCompareOperatorToken(const FString& Operator)
{
	return Operator.TrimStartAndEnd().ToLower();
}

static FString ResolveCompareOperatorBaseName(const FString& Operator)
{
	const FString Token = NormalizeCompareOperatorToken(Operator);
	if (Token == TEXT(">") || Token == TEXT("gt") || Token == TEXT("greater"))
	{
		return TEXT("Greater");
	}
	if (Token == TEXT(">=") || Token == TEXT("gte") || Token == TEXT("greater_equal") || Token == TEXT("greaterequal"))
	{
		return TEXT("GreaterEqual");
	}
	if (Token == TEXT("<") || Token == TEXT("lt") || Token == TEXT("less"))
	{
		return TEXT("Less");
	}
	if (Token == TEXT("<=") || Token == TEXT("lte") || Token == TEXT("less_equal") || Token == TEXT("lessequal"))
	{
		return TEXT("LessEqual");
	}
	if (Token == TEXT("==") || Token == TEXT("=") || Token == TEXT("eq") || Token == TEXT("equal") || Token == TEXT("equals"))
	{
		return TEXT("EqualEqual");
	}
	if (Token == TEXT("!=") || Token == TEXT("<>") || Token == TEXT("ne") || Token == TEXT("not_equal") || Token == TEXT("notequal"))
	{
		return TEXT("NotEqual");
	}
	if (Token == TEXT("&&") || Token == TEXT("and") || Token == TEXT("boolean_and") || Token == TEXT("booleanand"))
	{
		return TEXT("BooleanAND");
	}
	if (Token == TEXT("||") || Token == TEXT("or") || Token == TEXT("boolean_or") || Token == TEXT("booleanor"))
	{
		return TEXT("BooleanOR");
	}
	return Operator.TrimStartAndEnd();
}

static FString NormalizeCompareTypeToken(const FString& Type)
{
	FString Token = Type;
	Token.TrimStartAndEndInline();
	Token.ToLowerInline();
	Token.ReplaceInline(TEXT(" "), TEXT(""));
	Token.ReplaceInline(TEXT("-"), TEXT(""));
	Token.ReplaceInline(TEXT("_"), TEXT(""));
	return Token;
}

static void AddUniqueString(TArray<FString>& Values, const FString& Value)
{
	if (!Value.IsEmpty() && !Values.Contains(Value))
	{
		Values.Add(Value);
	}
}

static void AddCompareTypeSuffixesForToken(const FString& TypeToken, TArray<FString>& Suffixes)
{
	if (TypeToken.Contains(TEXT("bool")))
	{
		AddUniqueString(Suffixes, TEXT("BoolBool"));
	}
	if (TypeToken.Contains(TEXT("int64")) || TypeToken.Contains(TEXT("long")))
	{
		AddUniqueString(Suffixes, TEXT("Int64Int64"));
	}
	if (TypeToken.Contains(TEXT("int")) || TypeToken.Contains(TEXT("integer")))
	{
		AddUniqueString(Suffixes, TEXT("IntInt"));
	}
	if (TypeToken.Contains(TEXT("byte")))
	{
		AddUniqueString(Suffixes, TEXT("ByteByte"));
	}
	if (TypeToken.Contains(TEXT("double")) || TypeToken.Contains(TEXT("real")) || TypeToken.Contains(TEXT("number")))
	{
		AddUniqueString(Suffixes, TEXT("DoubleDouble"));
		AddUniqueString(Suffixes, TEXT("FloatFloat"));
	}
	if (TypeToken.Contains(TEXT("float")))
	{
		AddUniqueString(Suffixes, TEXT("FloatFloat"));
		AddUniqueString(Suffixes, TEXT("DoubleDouble"));
	}
	if (TypeToken.Contains(TEXT("string")))
	{
		AddUniqueString(Suffixes, TEXT("StrStr"));
	}
	if (TypeToken.Contains(TEXT("name")))
	{
		AddUniqueString(Suffixes, TEXT("NameName"));
	}
	if (TypeToken.Contains(TEXT("text")))
	{
		AddUniqueString(Suffixes, TEXT("TextText"));
	}
	if (TypeToken.Contains(TEXT("vector")))
	{
		AddUniqueString(Suffixes, TEXT("VectorVector"));
	}
	if (TypeToken.Contains(TEXT("rotator")))
	{
		AddUniqueString(Suffixes, TEXT("RotatorRotator"));
	}
	if (TypeToken.Contains(TEXT("transform")))
	{
		AddUniqueString(Suffixes, TEXT("TransformTransform"));
	}
	if (TypeToken.Contains(TEXT("object")) || TypeToken.Contains(TEXT("actor")) || TypeToken.Contains(TEXT("component")))
	{
		AddUniqueString(Suffixes, TEXT("ObjectObject"));
	}
}

static TArray<FString> BuildCompareTypeSuffixCandidates(const FBlueprintHelperGraphExpressionIR& Expression)
{
	TArray<FString> Suffixes;
	if (Expression.Left.IsValid())
	{
		AddCompareTypeSuffixesForToken(NormalizeCompareTypeToken(Expression.Left->Type), Suffixes);
	}
	if (Expression.Right.IsValid())
	{
		AddCompareTypeSuffixesForToken(NormalizeCompareTypeToken(Expression.Right->Type), Suffixes);
	}

	AddUniqueString(Suffixes, TEXT("DoubleDouble"));
	AddUniqueString(Suffixes, TEXT("FloatFloat"));
	AddUniqueString(Suffixes, TEXT("IntInt"));
	AddUniqueString(Suffixes, TEXT("Int64Int64"));
	AddUniqueString(Suffixes, TEXT("BoolBool"));
	AddUniqueString(Suffixes, TEXT("ByteByte"));
	AddUniqueString(Suffixes, TEXT("ObjectObject"));
	AddUniqueString(Suffixes, TEXT("NameName"));
	AddUniqueString(Suffixes, TEXT("StrStr"));
	AddUniqueString(Suffixes, TEXT("TextText"));
	AddUniqueString(Suffixes, TEXT("VectorVector"));
	AddUniqueString(Suffixes, TEXT("RotatorRotator"));
	AddUniqueString(Suffixes, TEXT("TransformTransform"));
	return Suffixes;
}

static FString ResolveCompareOperatorFunctionName(const FBlueprintHelperGraphExpressionIR& Expression)
{
	const FString RawOperator = Expression.Operator.TrimStartAndEnd();
	if (RawOperator.IsEmpty())
	{
		return FString();
	}

	if (FBlueprintGraphWriteFacade::FindFunctionByName(RawOperator))
	{
		return RawOperator;
	}

	const FString BaseName = ResolveCompareOperatorBaseName(RawOperator);
	if (BaseName.IsEmpty())
	{
		return RawOperator;
	}

	if (BaseName.Equals(TEXT("BooleanAND"), ESearchCase::IgnoreCase)
		|| BaseName.Equals(TEXT("BooleanOR"), ESearchCase::IgnoreCase))
	{
		return BaseName;
	}

	TArray<FString> Candidates;
	for (const FString& Suffix : BuildCompareTypeSuffixCandidates(Expression))
	{
		AddUniqueString(Candidates, BaseName + TEXT("_") + Suffix);
	}
	AddUniqueString(Candidates, BaseName);

	for (const FString& Candidate : Candidates)
	{
		if (FBlueprintGraphWriteFacade::FindFunctionByName(Candidate))
		{
			return Candidate;
		}
	}

	return BaseName;
}
}

bool FBlueprintHelperGraphStatementBuilder::BuildCallFunctionFragment(
	UEdGraph* TargetGraph,
	const FParsedNode& NodeData,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	OutFragment = FBlueprintHelperNodeFragment();
	FParsedNode BoundNodeData = NodeData;
	ApplyCallPatternBindings(BoundNodeData);

	const FBlueprintHelperCallFunctionResolveResult ResolveResult =
		FBlueprintGraphWriteFacade::ResolveFunctionForGraph(TargetGraph, BoundNodeData.FunctionName, BoundNodeData.DefaultValues);

	if (!ResolveResult.IsResolved())
	{
		FString ObjectName;
		FString FunctionName;
		if (TryParseExplicitObjectCall(ResolveResult, BoundNodeData.FunctionName, ObjectName, FunctionName))
		{
			return SpawnExplicitObjectCallFragment(TargetGraph, BoundNodeData, ObjectName, FunctionName, OutFragment, OutError);
		}

		OutError = ResolveResult.Message.IsEmpty()
			? FString::Printf(TEXT("call_function resolve failed: %s"), *BoundNodeData.FunctionName)
			: ResolveResult.Message;
		return false;
	}

	UK2Node* SpawnedNode = FBlueprintHelperCallFunctionResolver::SpawnResolvedNode(
		TargetGraph,
		ResolveResult.Selected,
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

	if (NodeData.VariableReference.IsLocalVariable() && NodeData.VariableReference.bEnsureExists)
	{
		FParsedLocalVariableDeclaration LocalDeclaration;
		LocalDeclaration.Name = NodeData.VariableReference.VariableName;
		LocalDeclaration.PinType = NodeData.VariableReference.PinType;
		LocalDeclaration.DefaultValue = NodeData.VariableReference.DefaultValue;
		LocalDeclaration.bEnsureExists = true;
		FBlueprintGraphWriteFacade::EnsureLocalVariableExists(TargetGraph, LocalDeclaration, OutError);
		if (!OutError.IsEmpty())
		{
			return false;
		}
	}

	UK2Node* SpawnedNode = FBlueprintGraphWriteFacade::SpawnVariableSetNode(TargetGraph, NodeData, OutError);
	if (!SpawnedNode)
	{
		return false;
	}

	OutFragment.FragmentId = NodeData.Id;
	OutFragment.SourceStatementId = NodeData.Id;
	OutFragment.PrimaryNode = SpawnedNode;
	OutFragment.Nodes.Add(SpawnedNode);
	OutFragment.ExecEntryPin = FBlueprintGraphWriteFacade::FindPinByAlias(SpawnedNode, TEXT("execute"));
	OutFragment.ExecExitPin = FBlueprintGraphWriteFacade::FindPinByAlias(SpawnedNode, TEXT("then"));
	OutFragment.PinBindings.Add(TEXT("execute"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("execute"), TEXT("exec"), OutFragment.ExecEntryPin });
	OutFragment.PinBindings.Add(TEXT("then"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("then"), TEXT("exec"), OutFragment.ExecExitPin });
	UEdGraphPin* ValuePin = FBlueprintGraphWriteFacade::FindPinByAlias(SpawnedNode, NodeData.VariableReference.VariableName);
	if (!ValuePin)
	{
		ValuePin = FBlueprintGraphWriteFacade::FindPinByAlias(SpawnedNode, TEXT("value"));
	}
	OutFragment.DataInputs.Add(NodeData.VariableReference.VariableName, FBlueprintHelperFragmentPinRef{ NodeData.Id, NodeData.VariableReference.VariableName, TEXT("value"), ValuePin });
	OutFragment.DataInputs.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("value"), TEXT("value"), ValuePin });
	OutFragment.PinBindings.Add(NodeData.VariableReference.VariableName, FBlueprintHelperFragmentPinRef{ NodeData.Id, NodeData.VariableReference.VariableName, TEXT("value"), ValuePin });
	OutFragment.PinBindings.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("value"), TEXT("value"), ValuePin });
	PopulateCommonFragmentMetadata(NodeData, OutFragment);
	return true;
}

bool FBlueprintHelperGraphStatementBuilder::BuildSequenceFragment(
	UEdGraph* TargetGraph,
	const FString& FragmentId,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	OutFragment = FBlueprintHelperNodeFragment();

	FParsedNode NodeData;
	NodeData.Id = FragmentId.IsEmpty() ? TEXT("semantic_sequence") : FragmentId;
	NodeData.NodeType = EParsedBlueprintNodeType::Sequence;
	NodeData.SourceType = TEXT("K2Node_ExecutionSequence");

	FSequenceNodeHandler Handler;
	UK2Node* SpawnedNode = Handler.Spawn(TargetGraph, NodeData, OutError);
	if (!SpawnedNode)
	{
		return false;
	}

	OutFragment.FragmentId = NodeData.Id;
	OutFragment.SourceStatementId = NodeData.Id;
	OutFragment.PrimaryNode = SpawnedNode;
	OutFragment.Nodes.Add(SpawnedNode);
	OutFragment.ExecEntryPin = FBlueprintGraphWriteFacade::FindPinByAlias(SpawnedNode, TEXT("execute"));
	OutFragment.ExecExitPin = FBlueprintGraphWriteFacade::FindPinByAlias(SpawnedNode, TEXT("then"));
	OutFragment.PinBindings.Add(TEXT("execute"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("execute"), TEXT("exec"), OutFragment.ExecEntryPin });
	OutFragment.PinBindings.Add(TEXT("then"), FBlueprintHelperFragmentPinRef{ TEXT("primary"), TEXT("then"), TEXT("exec"), OutFragment.ExecExitPin });
	PopulateCommonFragmentMetadata(NodeData, OutFragment);
	return true;
}

bool FBlueprintHelperGraphStatementBuilder::BuildExpressionFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphExpressionIR& Expression,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	OutFragment = FBlueprintHelperNodeFragment();
	OutError.Reset();

	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::Get)
	{
		const FString VariableName = !Expression.ResolvedTarget.Member.IsEmpty()
			? Expression.ResolvedTarget.Member
			: Expression.Target;
		if (VariableName.TrimStartAndEnd().IsEmpty())
		{
			OutError = TEXT("get expression fragment build failed: target is empty.");
			return false;
		}

		FParsedNode NodeData;
		NodeData.Id = MakeExpressionFragmentId(Expression);
		NodeData.NodeType = EParsedBlueprintNodeType::VariableGet;
		NodeData.SourceType = TEXT("K2Node_VariableGet");
		NodeData.VariableReference.ScopeType = TEXT("member");
		NodeData.VariableReference.VariableName = VariableName;
		NodeData.VariableReference.bSelfContext = true;

		UK2Node* SpawnedNode = FBlueprintGraphWriteFacade::SpawnVariableGetNode(TargetGraph, NodeData, OutError);
		if (!SpawnedNode)
		{
			return false;
		}

		UEdGraphPin* OutputPin = FindFirstDataOutputPin(SpawnedNode);
		OutFragment.FragmentId = NodeData.Id;
		OutFragment.SourceStatementId = Expression.ExpressionId;
		OutFragment.PrimaryNode = SpawnedNode;
		OutFragment.Nodes.Add(SpawnedNode);
		OutFragment.DataOutputs.Add(VariableName, FBlueprintHelperFragmentPinRef{ NodeData.Id, VariableName, Expression.Type, OutputPin });
		OutFragment.DataOutputs.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("value"), Expression.Type, OutputPin });
		OutFragment.PinBindings.Add(VariableName, FBlueprintHelperFragmentPinRef{ NodeData.Id, VariableName, Expression.Type, OutputPin });
		OutFragment.PinBindings.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("value"), Expression.Type, OutputPin });
		OutFragment.ReviewTargets.Add(Expression.ExpressionId);
		return true;
	}

	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::GetProperty)
	{
		FString OwnerName = Expression.ResolvedTarget.Owner;
		FString PropertyPath = Expression.ResolvedTarget.PropertyPath;
		if (OwnerName.IsEmpty() || PropertyPath.IsEmpty())
		{
			FString ParsedOwner;
			FString ParsedPath;
			if (Expression.Target.Split(TEXT("."), &ParsedOwner, &ParsedPath))
			{
				OwnerName = OwnerName.IsEmpty() ? ParsedOwner : OwnerName;
				PropertyPath = PropertyPath.IsEmpty() ? ParsedPath : PropertyPath;
			}
		}

		OwnerName.TrimStartAndEndInline();
		PropertyPath.TrimStartAndEndInline();
		if (OwnerName.IsEmpty() || PropertyPath.IsEmpty())
		{
			OutError = TEXT("get_property expression fragment build failed: target must be Owner.PropertyPath.");
			return false;
		}

		TArray<FString> Segments;
		PropertyPath.ParseIntoArray(Segments, TEXT("."), true);
		if (Segments.Num() == 0)
		{
			OutError = TEXT("get_property expression fragment build failed: property path is empty.");
			return false;
		}

		const FString BaseId = MakeExpressionFragmentId(Expression);
		FParsedNode OwnerGetterData;
		OwnerGetterData.Id = BaseId + TEXT("_owner");
		OwnerGetterData.NodeType = EParsedBlueprintNodeType::VariableGet;
		OwnerGetterData.SourceType = TEXT("K2Node_VariableGet");
		OwnerGetterData.X = -300.0f;
		OwnerGetterData.Y = 0.0f;
		OwnerGetterData.VariableReference.ScopeType = TEXT("member");
		OwnerGetterData.VariableReference.VariableName = OwnerName;
		OwnerGetterData.VariableReference.bSelfContext = true;

		UK2Node* OwnerGetterNode = FBlueprintGraphWriteFacade::SpawnVariableGetNode(TargetGraph, OwnerGetterData, OutError);
		if (!OwnerGetterNode)
		{
			return false;
		}

		TArray<UEdGraphNode*> Nodes;
		TArray<FBlueprintHelperFragmentLink> InternalLinks;
		Nodes.Add(OwnerGetterNode);

		UEdGraphPin* CurrentOutputPin = FindFirstDataOutputPin(OwnerGetterNode);
		if (!CurrentOutputPin)
		{
			OutError = FString::Printf(TEXT("get_property expression fragment build failed: owner output pin not found: %s."), *OwnerName);
			return false;
		}

		UK2Node* LastNode = OwnerGetterNode;
		for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
		{
			const FString Segment = Segments[SegmentIndex].TrimStartAndEnd();
			if (Segment.IsEmpty())
			{
				OutError = FString::Printf(TEXT("get_property expression fragment build failed: empty segment in path '%s'."), *PropertyPath);
				return false;
			}

			if (CurrentOutputPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
			{
				UScriptStruct* StructType = Cast<UScriptStruct>(CurrentOutputPin->PinType.PinSubCategoryObject.Get());
				if (!StructType)
				{
					OutError = FString::Printf(TEXT("get_property expression fragment build failed: struct type is missing before segment '%s'."), *Segment);
					return false;
				}

				FParsedNode BreakNodeData;
				BreakNodeData.Id = FString::Printf(TEXT("%s_break_%d"), *BaseId, SegmentIndex);
				BreakNodeData.NodeType = EParsedBlueprintNodeType::BreakStruct;
				BreakNodeData.SourceType = TEXT("K2Node_BreakStruct");
				BreakNodeData.X = static_cast<float>(SegmentIndex * 260);
				BreakNodeData.Y = 0.0f;
				BreakNodeData.StructReference.StructPath = StructType->GetPathName();

				FStructOperationNodeHandler Handler;
				UK2Node* BreakNode = Handler.Spawn(TargetGraph, BreakNodeData, OutError);
				if (!BreakNode)
				{
					return false;
				}

				UEdGraphPin* StructInputPin = FindFirstDataInputPin(BreakNode);
				if (!TryConnectDataPins(TargetGraph, CurrentOutputPin, StructInputPin, TEXT("get_property struct access"), OutError))
				{
					return false;
				}

				UEdGraphPin* SegmentOutputPin = FBlueprintGraphWriteFacade::FindPinByAlias(BreakNode, Segment);
				if (!SegmentOutputPin)
				{
					OutError = FString::Printf(TEXT("get_property expression fragment build failed: struct output pin not found: %s.%s."), *StructType->GetName(), *Segment);
					return false;
				}

				InternalLinks.Add(FBlueprintHelperFragmentLink{
					FBlueprintHelperFragmentPinRef{ LastNode->GetName(), CurrentOutputPin->PinName.ToString(), CurrentOutputPin->PinType.PinCategory.ToString(), CurrentOutputPin },
					FBlueprintHelperFragmentPinRef{ BreakNodeData.Id, StructInputPin ? StructInputPin->PinName.ToString() : FString(), StructInputPin ? StructInputPin->PinType.PinCategory.ToString() : FString(), StructInputPin }
				});
				Nodes.Add(BreakNode);
				LastNode = BreakNode;
				CurrentOutputPin = SegmentOutputPin;
				continue;
			}

			if (CurrentOutputPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object
				|| CurrentOutputPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Interface)
			{
				UClass* OwnerClass = Cast<UClass>(CurrentOutputPin->PinType.PinSubCategoryObject.Get());
				if (!OwnerClass)
				{
					OutError = FString::Printf(TEXT("get_property expression fragment build failed: object class is missing before segment '%s'."), *Segment);
					return false;
				}

				FParsedNode PropertyGetterData;
				PropertyGetterData.Id = FString::Printf(TEXT("%s_prop_%d"), *BaseId, SegmentIndex);
				PropertyGetterData.NodeType = EParsedBlueprintNodeType::VariableGet;
				PropertyGetterData.SourceType = TEXT("K2Node_VariableGet");
				PropertyGetterData.X = static_cast<float>(SegmentIndex * 260);
				PropertyGetterData.Y = 0.0f;
				PropertyGetterData.VariableReference.ScopeType = TEXT("member");
				PropertyGetterData.VariableReference.VariableName = Segment;
				PropertyGetterData.VariableReference.OwnerClassPath = OwnerClass->GetPathName();
				PropertyGetterData.VariableReference.bSelfContext = false;

				UK2Node* PropertyGetterNode = FBlueprintGraphWriteFacade::SpawnVariableGetNode(TargetGraph, PropertyGetterData, OutError);
				if (!PropertyGetterNode)
				{
					return false;
				}

				UEdGraphPin* TargetPin = FindCallTargetPin(PropertyGetterNode);
				if (!TryConnectDataPins(TargetGraph, CurrentOutputPin, TargetPin, TEXT("get_property object access"), OutError))
				{
					return false;
				}

				UEdGraphPin* SegmentOutputPin = FindFirstDataOutputPin(PropertyGetterNode);
				if (!SegmentOutputPin)
				{
					OutError = FString::Printf(TEXT("get_property expression fragment build failed: property output pin not found: %s.%s."), *OwnerClass->GetName(), *Segment);
					return false;
				}

				InternalLinks.Add(FBlueprintHelperFragmentLink{
					FBlueprintHelperFragmentPinRef{ LastNode->GetName(), CurrentOutputPin->PinName.ToString(), CurrentOutputPin->PinType.PinCategory.ToString(), CurrentOutputPin },
					FBlueprintHelperFragmentPinRef{ PropertyGetterData.Id, TargetPin ? TargetPin->PinName.ToString() : FString(), TargetPin ? TargetPin->PinType.PinCategory.ToString() : FString(), TargetPin }
				});
				Nodes.Add(PropertyGetterNode);
				LastNode = PropertyGetterNode;
				CurrentOutputPin = SegmentOutputPin;
				continue;
			}

			OutError = FString::Printf(
				TEXT("get_property expression fragment build failed: unsupported owner pin category '%s' before segment '%s'."),
				*CurrentOutputPin->PinType.PinCategory.ToString(),
				*Segment);
			return false;
		}

		OutFragment.FragmentId = BaseId;
		OutFragment.SourceStatementId = Expression.ExpressionId;
		OutFragment.PrimaryNode = LastNode;
		OutFragment.Nodes = MoveTemp(Nodes);
		OutFragment.InternalLinks = MoveTemp(InternalLinks);
		OutFragment.DataOutputs.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ BaseId, TEXT("value"), Expression.Type, CurrentOutputPin });
		OutFragment.DataOutputs.Add(PropertyPath, FBlueprintHelperFragmentPinRef{ BaseId, PropertyPath, Expression.Type, CurrentOutputPin });
		OutFragment.PinBindings.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ BaseId, TEXT("value"), Expression.Type, CurrentOutputPin });
		OutFragment.PinBindings.Add(PropertyPath, FBlueprintHelperFragmentPinRef{ BaseId, PropertyPath, Expression.Type, CurrentOutputPin });
		OutFragment.ReviewTargets.Add(Expression.ExpressionId);
		return true;
	}

	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::Literal)
	{
		OutError = TEXT("literal expression fragment build skipped: literals are expected to bind as pin defaults.");
		return false;
	}

	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::Call)
	{
		if (Expression.Target.TrimStartAndEnd().IsEmpty())
		{
			OutError = TEXT("call expression fragment build failed: target is empty.");
			return false;
		}

		FParsedNode NodeData;
		NodeData.Id = MakeExpressionFragmentId(Expression);
		NodeData.NodeType = EParsedBlueprintNodeType::CallFunction;
		NodeData.SourceType = TEXT("K2Node_CallFunction");
		NodeData.FunctionName = Expression.Target;
		for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& ArgPair : Expression.Args)
		{
			if (ArgPair.Value.IsValid() && ArgPair.Value->Kind == EBlueprintHelperGraphExpressionKind::Literal)
			{
				NodeData.DefaultValues.Add(ArgPair.Key, ArgPair.Value->LiteralValue);
			}
		}

		if (!BuildCallFunctionFragment(TargetGraph, NodeData, OutFragment, OutError))
		{
			return false;
		}
		OutFragment.SourceStatementId = Expression.ExpressionId;
		OutFragment.ReviewTargets.Add(Expression.ExpressionId);
		return true;
	}

	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::Compare)
	{
		if (Expression.Operator.TrimStartAndEnd().IsEmpty())
		{
			OutError = TEXT("compare expression fragment build failed: operator is empty.");
			return false;
		}

		FParsedNode NodeData;
		NodeData.Id = MakeExpressionFragmentId(Expression);
		NodeData.NodeType = EParsedBlueprintNodeType::PromotableOperator;
		NodeData.SourceType = TEXT("K2Node_PromotableOperator");
		NodeData.FunctionName = ResolveCompareOperatorFunctionName(Expression);
		if (Expression.Left.IsValid() && Expression.Left->Kind == EBlueprintHelperGraphExpressionKind::Literal)
		{
			NodeData.DefaultValues.Add(TEXT("A"), Expression.Left->LiteralValue);
		}
		if (Expression.Right.IsValid() && Expression.Right->Kind == EBlueprintHelperGraphExpressionKind::Literal)
		{
			NodeData.DefaultValues.Add(TEXT("B"), Expression.Right->LiteralValue);
		}

		FPromotableOperatorNodeHandler Handler;
		UK2Node* SpawnedNode = Handler.Spawn(TargetGraph, NodeData, OutError);
		if (!SpawnedNode)
		{
			return false;
		}

		UEdGraphPin* LeftPin = FBlueprintGraphWriteFacade::FindPinByAlias(SpawnedNode, TEXT("A"));
		UEdGraphPin* RightPin = FBlueprintGraphWriteFacade::FindPinByAlias(SpawnedNode, TEXT("B"));
		UEdGraphPin* OutputPin = FindFirstDataOutputPin(SpawnedNode);
		OutFragment.FragmentId = NodeData.Id;
		OutFragment.SourceStatementId = Expression.ExpressionId;
		OutFragment.PrimaryNode = SpawnedNode;
		OutFragment.Nodes.Add(SpawnedNode);
		OutFragment.DataInputs.Add(TEXT("left"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("A"), Expression.Left.IsValid() ? Expression.Left->Type : FString(), LeftPin });
		OutFragment.DataInputs.Add(TEXT("right"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("B"), Expression.Right.IsValid() ? Expression.Right->Type : FString(), RightPin });
		OutFragment.DataOutputs.Add(TEXT("result"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("result"), TEXT("bool"), OutputPin });
		OutFragment.PinBindings.Add(TEXT("A"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("A"), Expression.Left.IsValid() ? Expression.Left->Type : FString(), LeftPin });
		OutFragment.PinBindings.Add(TEXT("B"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("B"), Expression.Right.IsValid() ? Expression.Right->Type : FString(), RightPin });
		OutFragment.PinBindings.Add(TEXT("result"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("result"), TEXT("bool"), OutputPin });
		OutFragment.ReviewTargets.Add(Expression.ExpressionId);
		return true;
	}

	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::Select)
	{
		FParsedNode NodeData;
		NodeData.Id = MakeExpressionFragmentId(Expression);
		NodeData.NodeType = EParsedBlueprintNodeType::Select;
		NodeData.SourceType = TEXT("K2Node_Select");
		NodeData.SelectReference.NumOptions = FMath::Max(2, Expression.Options.Num());
		const TSharedPtr<FBlueprintHelperGraphExpressionIR>* ConditionExpressionPtr = Expression.Args.Find(TEXT("condition"));
		if (!ConditionExpressionPtr)
		{
			ConditionExpressionPtr = Expression.Args.Find(TEXT("index"));
		}
		const TSharedPtr<FBlueprintHelperGraphExpressionIR> ConditionExpression =
			ConditionExpressionPtr ? *ConditionExpressionPtr : nullptr;
		if (ConditionExpression.IsValid() && ConditionExpression->Kind == EBlueprintHelperGraphExpressionKind::Literal)
		{
			NodeData.DefaultValues.Add(TEXT("Index"), ConditionExpression->LiteralValue);
		}
		for (int32 OptionIndex = 0; OptionIndex < Expression.Options.Num(); ++OptionIndex)
		{
			const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Option = Expression.Options[OptionIndex];
			if (Option.IsValid() && Option->Kind == EBlueprintHelperGraphExpressionKind::Literal)
			{
				NodeData.DefaultValues.Add(FString::Printf(TEXT("Option%d"), OptionIndex), Option->LiteralValue);
			}
		}

		FSelectNodeHandler Handler;
		UK2Node* SpawnedNode = Handler.Spawn(TargetGraph, NodeData, OutError);
		if (!SpawnedNode)
		{
			return false;
		}

		OutFragment.FragmentId = NodeData.Id;
		OutFragment.SourceStatementId = Expression.ExpressionId;
		OutFragment.PrimaryNode = SpawnedNode;
		OutFragment.Nodes.Add(SpawnedNode);

		UEdGraphPin* IndexPin = FBlueprintGraphWriteFacade::FindPinByAlias(SpawnedNode, TEXT("Index"));
		OutFragment.DataInputs.Add(TEXT("condition"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("Index"), ConditionExpression.IsValid() ? ConditionExpression->Type : FString(), IndexPin });
		OutFragment.PinBindings.Add(TEXT("Index"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("Index"), ConditionExpression.IsValid() ? ConditionExpression->Type : FString(), IndexPin });
		for (int32 OptionIndex = 0; OptionIndex < Expression.Options.Num(); ++OptionIndex)
		{
			const FString OptionPinName = FString::Printf(TEXT("Option%d"), OptionIndex);
			const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Option = Expression.Options[OptionIndex];
			UEdGraphPin* OptionPin = FBlueprintGraphWriteFacade::FindPinByAlias(SpawnedNode, OptionPinName);
			OutFragment.DataInputs.Add(OptionPinName, FBlueprintHelperFragmentPinRef{ NodeData.Id, OptionPinName, Option.IsValid() ? Option->Type : FString(), OptionPin });
			OutFragment.PinBindings.Add(OptionPinName, FBlueprintHelperFragmentPinRef{ NodeData.Id, OptionPinName, Option.IsValid() ? Option->Type : FString(), OptionPin });
		}

		UEdGraphPin* OutputPin = FindFirstDataOutputPin(SpawnedNode);
		OutFragment.DataOutputs.Add(TEXT("result"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("result"), Expression.Type, OutputPin });
		OutFragment.PinBindings.Add(TEXT("result"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("result"), Expression.Type, OutputPin });
		OutFragment.ReviewTargets.Add(Expression.ExpressionId);
		return true;
	}

	if (Expression.Kind == EBlueprintHelperGraphExpressionKind::MakeStruct)
	{
		if (Expression.Type.TrimStartAndEnd().IsEmpty())
		{
			OutError = TEXT("make_struct expression fragment build failed: type is empty.");
			return false;
		}

		FParsedNode NodeData;
		NodeData.Id = MakeExpressionFragmentId(Expression);
		NodeData.NodeType = EParsedBlueprintNodeType::MakeStruct;
		NodeData.SourceType = TEXT("K2Node_MakeStruct");
		NodeData.StructReference.StructPath = Expression.Type;

		for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& ArgPair : Expression.Args)
		{
			if (ArgPair.Value.IsValid() && ArgPair.Value->Kind == EBlueprintHelperGraphExpressionKind::Literal)
			{
				NodeData.DefaultValues.Add(ArgPair.Key, ArgPair.Value->LiteralValue);
			}
		}

		FStructOperationNodeHandler Handler;
		UK2Node* SpawnedNode = Handler.Spawn(TargetGraph, NodeData, OutError);
		if (!SpawnedNode)
		{
			return false;
		}

		UEdGraphPin* OutputPin = FindFirstDataOutputPin(SpawnedNode);
		OutFragment.FragmentId = NodeData.Id;
		OutFragment.SourceStatementId = Expression.ExpressionId;
		OutFragment.PrimaryNode = SpawnedNode;
		OutFragment.Nodes.Add(SpawnedNode);
		for (const TPair<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& ArgPair : Expression.Args)
		{
			if (ArgPair.Value.IsValid() && ArgPair.Value->Kind != EBlueprintHelperGraphExpressionKind::Literal)
			{
				UEdGraphPin* FieldPin = FBlueprintGraphWriteFacade::FindPinByAlias(SpawnedNode, ArgPair.Key);
				OutFragment.DataInputs.Add(ArgPair.Key, FBlueprintHelperFragmentPinRef{ NodeData.Id, ArgPair.Key, ArgPair.Value->Type, FieldPin });
				OutFragment.PinBindings.Add(ArgPair.Key, FBlueprintHelperFragmentPinRef{ NodeData.Id, ArgPair.Key, ArgPair.Value->Type, FieldPin });
			}
		}
		OutFragment.DataOutputs.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("value"), Expression.Type, OutputPin });
		OutFragment.PinBindings.Add(TEXT("value"), FBlueprintHelperFragmentPinRef{ NodeData.Id, TEXT("value"), Expression.Type, OutputPin });
		OutFragment.ReviewTargets.Add(Expression.ExpressionId);
		return true;
	}

	OutError = FString::Printf(
		TEXT("expression fragment pattern is not implemented yet: %s."),
		*Expression.PatternName);
	return false;
}
