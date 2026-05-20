#include "Systems/GraphLayout/BlueprintHelperGraphLayoutClassifier.h"

namespace BlueprintHelper::GraphLayout
{
static bool ContainsIgnoreCase(const FString& Haystack, const FString& Needle)
{
	return !Needle.IsEmpty() && Haystack.Contains(Needle, ESearchCase::IgnoreCase);
}

static bool ContainsAnyIgnoreCase(const FString& Haystack, const TArray<FString>& Needles)
{
	for (const FString& Needle : Needles)
	{
		if (ContainsIgnoreCase(Haystack, Needle))
		{
			return true;
		}
	}
	return false;
}

static bool HasExecPin(const FNodeSnapshot& Node, EPinDirection Direction)
{
	for (const FPinSnapshot& Pin : Node.Pins)
	{
		if (Pin.Direction == Direction && Pin.bExec)
		{
			return true;
		}
	}
	return false;
}

static bool HasAnyExecPin(const FNodeSnapshot& Node)
{
	return HasExecPin(Node, EPinDirection::Input) || HasExecPin(Node, EPinDirection::Output);
}

static bool HasLinkedInput(const FNodeSnapshot& Node)
{
	for (const FPinSnapshot& Pin : Node.Pins)
	{
		if (Pin.Direction == EPinDirection::Input && Pin.LinkedNodeIds.Num() > 0)
		{
			return true;
		}
	}
	return false;
}

static bool IsOperatorOrCompareNode(const FNodeSnapshot& Node)
{
	static const TArray<FString> ClassTokens = {
		TEXT("K2Node_CommutativeAssociativeBinaryOperator"),
		TEXT("K2Node_PromotableOperator")
	};
	static const TArray<FString> TitleTokens = {
		TEXT("=="),
		TEXT("!="),
		TEXT(">"),
		TEXT("<"),
		TEXT("+"),
		TEXT("-"),
		TEXT("*"),
		TEXT("/"),
		TEXT("AND"),
		TEXT("OR"),
		TEXT("NOT"),
		TEXT("Equal"),
		TEXT("Greater"),
		TEXT("Less"),
		TEXT("Is Valid"),
		TEXT("IsValid")
	};
	return ContainsAnyIgnoreCase(Node.ClassPath, ClassTokens) ||
		ContainsAnyIgnoreCase(Node.Title, TitleTokens);
}

FNodeClassification FClassifier::ClassifyNode(const FNodeSnapshot& Node, const FRuleSet& RuleSet)
{
	FNodeClassification Result;
	Result.NodeId = Node.NodeId;

	for (const FRoleRule& Rule : RuleSet.RoleRules)
	{
		const bool bClassMatches = Rule.MatchClassContains.Num() == 0 || ContainsAnyIgnoreCase(Node.ClassPath, Rule.MatchClassContains);
		const bool bTitleMatches = Rule.MatchTitleContains.Num() == 0 || ContainsAnyIgnoreCase(Node.Title, Rule.MatchTitleContains);
		const bool bExecMatches = !Rule.bHasExecPinMatcher || HasAnyExecPin(Node) == Rule.bMatchHasExecPin;
		if (bClassMatches && bTitleMatches && bExecMatches)
		{
			Result.Role = Rule.Role;
			Result.Reason = TEXT("matched_rule");
			return Result;
		}
	}

	if (ContainsIgnoreCase(Node.ClassPath, TEXT("K2Node_Knot")) || ContainsIgnoreCase(Node.ClassPath, TEXT("Reroute")) || ContainsIgnoreCase(Node.Title, TEXT("reroute")))
	{
		Result.Role = ENodeRole::Unknown;
		Result.Reason = TEXT("deprecated_reroute_ignored");
		return Result;
	}

	if (ContainsIgnoreCase(Node.ClassPath, TEXT("K2Node_Event")) || ContainsIgnoreCase(Node.ClassPath, TEXT("K2Node_CustomEvent")))
	{
		Result.Role = ENodeRole::EventEntry;
		Result.Reason = TEXT("event_class");
		return Result;
	}

	if (ContainsIgnoreCase(Node.ClassPath, TEXT("K2Node_IfThenElse")) ||
		ContainsIgnoreCase(Node.ClassPath, TEXT("K2Node_Switch")) ||
		ContainsIgnoreCase(Node.ClassPath, TEXT("K2Node_ExecutionSequence")) ||
		ContainsIgnoreCase(Node.Title, TEXT("branch")))
	{
		Result.Role = ENodeRole::BranchControl;
		Result.Reason = TEXT("branch_class_or_title");
		return Result;
	}

	if (ContainsIgnoreCase(Node.ClassPath, TEXT("Async")) ||
		ContainsIgnoreCase(Node.Title, TEXT("delay")))
	{
		Result.Role = ENodeRole::AsyncNode;
		Result.Reason = TEXT("async_or_latent_node");
		return Result;
	}

	if (ContainsIgnoreCase(Node.ClassPath, TEXT("Delegate")) ||
		ContainsIgnoreCase(Node.Title, TEXT("dispatcher")))
	{
		Result.Role = ENodeRole::DelegateNode;
		Result.Reason = TEXT("delegate_node");
		return Result;
	}

	if (ContainsIgnoreCase(Node.ClassPath, TEXT("EdGraphNode_Comment")) ||
		ContainsIgnoreCase(Node.Title, TEXT("comment")))
	{
		Result.Role = ENodeRole::Comment;
		Result.Reason = TEXT("comment_node");
		return Result;
	}

	if (ContainsIgnoreCase(Node.ClassPath, TEXT("K2Node_VariableGet")) || ContainsIgnoreCase(Node.ClassPath, TEXT("K2Node_VariableSet")))
	{
		Result.Role = HasExecPin(Node, EPinDirection::Input) || HasExecPin(Node, EPinDirection::Output) ? ENodeRole::ExecNode : ENodeRole::VariableInput;
		Result.Reason = Result.Role == ENodeRole::VariableInput ? TEXT("variable_get_without_exec") : TEXT("variable_with_exec");
		return Result;
	}

	if (IsOperatorOrCompareNode(Node))
	{
		Result.Role = ENodeRole::OperatorOrCompare;
		Result.Reason = TEXT("operator_or_compare");
		return Result;
	}

	const bool bHasExecInput = HasExecPin(Node, EPinDirection::Input);
	const bool bHasExecOutput = HasExecPin(Node, EPinDirection::Output);
	if (bHasExecInput || bHasExecOutput)
	{
		Result.Role = ENodeRole::ExecNode;
		Result.Reason = TEXT("has_exec_pin");
		return Result;
	}

	if (ContainsIgnoreCase(Node.ClassPath, TEXT("K2Node_CallFunction")) || HasLinkedInput(Node))
	{
		Result.Role = ENodeRole::PureFunction;
		Result.Reason = TEXT("pure_data_node");
		return Result;
	}

	Result.Role = ENodeRole::Unknown;
	Result.Reason = TEXT("no_rule_or_heuristic");
	return Result;
}

TArray<FNodeClassification> FClassifier::ClassifyGraph(const FGraphSnapshot& Snapshot, const FRuleSet& RuleSet)
{
	TArray<FNodeClassification> Results;
	Results.Reserve(Snapshot.Nodes.Num());
	for (const FNodeSnapshot& Node : Snapshot.Nodes)
	{
		Results.Add(ClassifyNode(Node, RuleSet));
	}
	return Results;
}
}
