#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperSelectFragmentBuilder.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Select.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphNodeFactory.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementTypeUtils.h"

namespace
{
static void AddPinAlias(
	TMap<FString, FBlueprintHelperFragmentPinRef>& PinMap,
	const FString& Alias,
	const FBlueprintHelperFragmentPinRef& PinRef)
{
	if (Alias.IsEmpty() || !PinRef.Pin)
	{
		return;
	}

	PinMap.Add(Alias, PinRef);
	const FString LowerAlias = Alias.ToLower();
	if (!PinMap.Contains(LowerAlias))
	{
		PinMap.Add(LowerAlias, FBlueprintHelperFragmentPinRef{ PinRef.NodeId, LowerAlias, PinRef.Type, PinRef.Pin });
	}
}

static bool TryBuildSelectPinType(const FString& TypeName, FEdGraphPinType& OutPinType)
{
	const FString Normalized = TypeName.TrimStartAndEnd().ToLower();
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

static bool TryGetExpressionLiteral(
	const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression,
	FString& OutLiteral)
{
	if (!Expression.IsValid() || Expression->Kind != EBlueprintHelperGraphExpressionKind::Literal)
	{
		return false;
	}

	OutLiteral = Expression->LiteralValue;
	return !OutLiteral.IsEmpty();
}

static void ApplyIndexPinType(UK2Node_Select* SelectNode, const FBlueprintHelperGraphExpressionIR& Expression)
{
	if (!SelectNode || !Expression.Condition.IsValid())
	{
		return;
	}

	FEdGraphPinType PinType;
	if (!TryBuildSelectPinType(Expression.Condition->Type, PinType))
	{
		return;
	}

	UEdGraphPin* IndexPin = SelectNode->GetIndexPin();
	if (!IndexPin || !SelectNode->CanChangePinType(IndexPin))
	{
		return;
	}

	IndexPin->PinType = PinType;
	SelectNode->ChangePinType(IndexPin);
}

static void ApplyResultPinType(UK2Node_Select* SelectNode, const FBlueprintHelperGraphExpressionIR& Expression)
{
	if (!SelectNode)
	{
		return;
	}

	FEdGraphPinType PinType;
	if (!TryBuildSelectPinType(Expression.Type, PinType))
	{
		return;
	}

	UEdGraphPin* ReturnPin = SelectNode->GetReturnValuePin();
	if (!ReturnPin || !SelectNode->CanChangePinType(ReturnPin))
	{
		return;
	}

	ReturnPin->PinType = PinType;
	SelectNode->ChangePinType(ReturnPin);
}

static int32 GetDesiredOptionCount(const FBlueprintHelperGraphExpressionIR& Expression)
{
	if (Expression.ThenValue.IsValid() || Expression.ElseValue.IsValid())
	{
		return 2;
	}
	return FMath::Max(2, Expression.Options.Num());
}

static bool EnsureOptionPinCount(
	UK2Node_Select* SelectNode,
	const int32 DesiredOptionCount,
	FString& OutError)
{
	if (!SelectNode)
	{
		OutError = TEXT("select fragment build failed: select node is invalid.");
		return false;
	}

	TArray<UEdGraphPin*> OptionPins;
	SelectNode->GetOptionPins(OptionPins);
	while (OptionPins.Num() < DesiredOptionCount)
	{
		if (!SelectNode->CanAddPin())
		{
			OutError = FString::Printf(
				TEXT("select fragment build failed: cannot add option pin %d."),
				OptionPins.Num());
			return false;
		}
		SelectNode->AddInputPin();
		OptionPins.Reset();
		SelectNode->GetOptionPins(OptionPins);
	}

	return true;
}

static void ApplyLiteralDefaults(
	UK2Node_Select* SelectNode,
	const FBlueprintHelperGraphExpressionIR& Expression,
	const FString& FragmentId)
{
	if (!SelectNode)
	{
		return;
	}

	TMap<FString, FString> Defaults;
	FString Literal;
	if (TryGetExpressionLiteral(Expression.Condition, Literal))
	{
		Defaults.Add(TEXT("Index"), Literal);
		Defaults.Add(TEXT("condition"), Literal);
	}

	TArray<UEdGraphPin*> OptionPins;
	SelectNode->GetOptionPins(OptionPins);
	if (Expression.ThenValue.IsValid() || Expression.ElseValue.IsValid())
	{
		if (OptionPins.IsValidIndex(0) && TryGetExpressionLiteral(Expression.ElseValue, Literal))
		{
			Defaults.Add(OptionPins[0]->PinName.ToString(), Literal);
			Defaults.Add(TEXT("else"), Literal);
		}
		if (OptionPins.IsValidIndex(1) && TryGetExpressionLiteral(Expression.ThenValue, Literal))
		{
			Defaults.Add(OptionPins[1]->PinName.ToString(), Literal);
			Defaults.Add(TEXT("then"), Literal);
		}
	}
	else
	{
		for (int32 OptionIndex = 0; OptionIndex < Expression.Options.Num() && OptionIndex < OptionPins.Num(); ++OptionIndex)
		{
			if (TryGetExpressionLiteral(Expression.Options[OptionIndex], Literal))
			{
				Defaults.Add(OptionPins[OptionIndex]->PinName.ToString(), Literal);
				Defaults.Add(FString::Printf(TEXT("option_%d"), OptionIndex), Literal);
			}
		}
	}

	if (Defaults.Num() > 0)
	{
		FBlueprintGraphWriteFacade::ApplyDefaultValues(SelectNode, Defaults, FragmentId);
	}
}

static void PopulateSelectPins(
	UK2Node_Select* SelectNode,
	FBlueprintHelperNodeFragment& OutFragment)
{
	if (!SelectNode)
	{
		return;
	}

	if (UEdGraphPin* IndexPin = SelectNode->GetIndexPin())
	{
		const FString Type = IndexPin->PinType.PinCategory.ToString();
		const FBlueprintHelperFragmentPinRef PinRef{ TEXT("primary"), IndexPin->PinName.ToString(), Type, IndexPin };
		AddPinAlias(OutFragment.PinBindings, IndexPin->PinName.ToString(), PinRef);
		AddPinAlias(OutFragment.DataInputs, IndexPin->PinName.ToString(), PinRef);
		AddPinAlias(OutFragment.PinBindings, TEXT("condition"), PinRef);
		AddPinAlias(OutFragment.DataInputs, TEXT("condition"), PinRef);
		AddPinAlias(OutFragment.PinBindings, TEXT("index"), PinRef);
		AddPinAlias(OutFragment.DataInputs, TEXT("index"), PinRef);
	}

	TArray<UEdGraphPin*> OptionPins;
	SelectNode->GetOptionPins(OptionPins);
	for (int32 OptionIndex = 0; OptionIndex < OptionPins.Num(); ++OptionIndex)
	{
		UEdGraphPin* OptionPin = OptionPins[OptionIndex];
		if (!OptionPin)
		{
			continue;
		}

		const FString Type = OptionPin->PinType.PinCategory.ToString();
		const FBlueprintHelperFragmentPinRef PinRef{ TEXT("primary"), OptionPin->PinName.ToString(), Type, OptionPin };
		AddPinAlias(OutFragment.PinBindings, OptionPin->PinName.ToString(), PinRef);
		AddPinAlias(OutFragment.DataInputs, OptionPin->PinName.ToString(), PinRef);
		AddPinAlias(OutFragment.PinBindings, FString::Printf(TEXT("option_%d"), OptionIndex), PinRef);
		AddPinAlias(OutFragment.DataInputs, FString::Printf(TEXT("option_%d"), OptionIndex), PinRef);
	}

	if (OptionPins.IsValidIndex(0))
	{
		const FBlueprintHelperFragmentPinRef PinRef{ TEXT("primary"), TEXT("else"), OptionPins[0]->PinType.PinCategory.ToString(), OptionPins[0] };
		AddPinAlias(OutFragment.PinBindings, TEXT("else"), PinRef);
		AddPinAlias(OutFragment.DataInputs, TEXT("else"), PinRef);
	}
	if (OptionPins.IsValidIndex(1))
	{
		const FBlueprintHelperFragmentPinRef PinRef{ TEXT("primary"), TEXT("then"), OptionPins[1]->PinType.PinCategory.ToString(), OptionPins[1] };
		AddPinAlias(OutFragment.PinBindings, TEXT("then"), PinRef);
		AddPinAlias(OutFragment.DataInputs, TEXT("then"), PinRef);
	}

	if (UEdGraphPin* ReturnPin = SelectNode->GetReturnValuePin())
	{
		const FString Type = ReturnPin->PinType.PinCategory.ToString();
		const FBlueprintHelperFragmentPinRef PinRef{ TEXT("primary"), ReturnPin->PinName.ToString(), Type, ReturnPin };
		AddPinAlias(OutFragment.PinBindings, ReturnPin->PinName.ToString(), PinRef);
		AddPinAlias(OutFragment.DataOutputs, ReturnPin->PinName.ToString(), PinRef);
		AddPinAlias(OutFragment.PinBindings, TEXT("result"), PinRef);
		AddPinAlias(OutFragment.DataOutputs, TEXT("result"), PinRef);
		AddPinAlias(OutFragment.PinBindings, TEXT("value"), PinRef);
		AddPinAlias(OutFragment.DataOutputs, TEXT("value"), PinRef);
		AddPinAlias(OutFragment.PinBindings, TEXT("return"), PinRef);
		AddPinAlias(OutFragment.DataOutputs, TEXT("return"), PinRef);
	}
}
}

bool FBlueprintHelperSelectFragmentBuilder::Build(
	UEdGraph* TargetGraph,
	const FBlueprintHelperGraphExpressionIR& Expression,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	OutFragment = FBlueprintHelperNodeFragment();
	OutError.Reset();
	if (!TargetGraph)
	{
		OutError = TEXT("select fragment build failed: target graph is invalid.");
		return false;
	}

	UK2Node_Select* SelectNode = FBlueprintHelperGraphNodeFactory::SpawnK2Node<UK2Node_Select>(
		TargetGraph,
		FVector2D::ZeroVector);
	if (!SelectNode)
	{
		OutError = TEXT("select fragment build failed: UK2Node_Select spawn failed.");
		return false;
	}

	ApplyIndexPinType(SelectNode, Expression);
	if (!EnsureOptionPinCount(SelectNode, GetDesiredOptionCount(Expression), OutError))
	{
		return false;
	}
	ApplyResultPinType(SelectNode, Expression);

	const FString ExpressionId = FBlueprintHelperGraphStatementTypeUtils::MakeExpressionFragmentId(Expression);
	ApplyLiteralDefaults(SelectNode, Expression, ExpressionId);

	OutFragment.FragmentId = ExpressionId;
	OutFragment.SourceStatementId = Expression.ExpressionId;
	OutFragment.PrimaryNode = SelectNode;
	OutFragment.Nodes.Add(SelectNode);
	PopulateSelectPins(SelectNode, OutFragment);
	OutFragment.OwnershipTags.Add(TEXT("expression_id"), Expression.ExpressionId);
	OutFragment.OwnershipTags.Add(TEXT("semantic_kind"), TEXT("select"));
	OutFragment.ReviewTargets.Add(Expression.ExpressionId);
	return true;
}
