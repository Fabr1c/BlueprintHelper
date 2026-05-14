#include "Systems/ToolClusters/GraphWrite/NodeHandlers/PromotableOperatorNodeHandler.h"

#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_PromotableOperator.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "UObject/FieldIterator.h"
#include "UObject/UnrealType.h"

namespace
{
static FString NormalizeOperatorToken(const FString& Operator)
{
	return Operator.TrimStartAndEnd().ToLower();
}

static FString ResolveOperatorBaseName(const FString& Operator)
{
	const FString Token = NormalizeOperatorToken(Operator);
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

static bool LooksLikeIntegerLiteral(const FString& Value)
{
	FString Trimmed = Value.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return false;
	}
	if (Trimmed.StartsWith(TEXT("-")) || Trimmed.StartsWith(TEXT("+")))
	{
		Trimmed.RightChopInline(1);
	}
	for (const TCHAR Ch : Trimmed)
	{
		if (!FChar::IsDigit(Ch))
		{
			return false;
		}
	}
	return true;
}

static bool LooksLikeFloatLiteral(const FString& Value)
{
	return Value.Contains(TEXT(".")) || Value.Contains(TEXT("e"), ESearchCase::IgnoreCase);
}

static void AddUniqueCandidate(TArray<FString>& Candidates, const FString& Candidate)
{
	if (!Candidate.IsEmpty() && !Candidates.Contains(Candidate))
	{
		Candidates.Add(Candidate);
	}
}

static void AddSuffixCandidatesFromDefaults(const FParsedNode& NodeData, TArray<FString>& Suffixes)
{
	const FString* A = NodeData.DefaultValues.Find(TEXT("A"));
	const FString* B = NodeData.DefaultValues.Find(TEXT("B"));
	if (!A || !B)
	{
		return;
	}

	const FString AToken = A->TrimStartAndEnd().ToLower();
	const FString BToken = B->TrimStartAndEnd().ToLower();
	if ((AToken == TEXT("true") || AToken == TEXT("false")) && (BToken == TEXT("true") || BToken == TEXT("false")))
	{
		AddUniqueCandidate(Suffixes, TEXT("BoolBool"));
		return;
	}
	if ((LooksLikeFloatLiteral(*A) || LooksLikeFloatLiteral(*B))
		&& (LooksLikeIntegerLiteral(*A) || LooksLikeFloatLiteral(*A))
		&& (LooksLikeIntegerLiteral(*B) || LooksLikeFloatLiteral(*B)))
	{
		AddUniqueCandidate(Suffixes, TEXT("DoubleDouble"));
		AddUniqueCandidate(Suffixes, TEXT("FloatFloat"));
		return;
	}
	if (LooksLikeIntegerLiteral(*A) && LooksLikeIntegerLiteral(*B))
	{
		AddUniqueCandidate(Suffixes, TEXT("IntInt"));
		AddUniqueCandidate(Suffixes, TEXT("Int64Int64"));
		return;
	}
}

static FString ResolveOperatorFunctionName(const FParsedNode& NodeData)
{
	const FString RawName = NodeData.FunctionName.TrimStartAndEnd();
	if (RawName.IsEmpty())
	{
		return FString();
	}
	if (FBlueprintGraphWriteFacade::FindFunctionByName(RawName))
	{
		return RawName;
	}

	const FString BaseName = ResolveOperatorBaseName(RawName);
	if (BaseName.Equals(TEXT("BooleanAND"), ESearchCase::IgnoreCase)
		|| BaseName.Equals(TEXT("BooleanOR"), ESearchCase::IgnoreCase))
	{
		return BaseName;
	}

	TArray<FString> Suffixes;
	AddSuffixCandidatesFromDefaults(NodeData, Suffixes);
	AddUniqueCandidate(Suffixes, TEXT("DoubleDouble"));
	AddUniqueCandidate(Suffixes, TEXT("FloatFloat"));
	AddUniqueCandidate(Suffixes, TEXT("IntInt"));
	AddUniqueCandidate(Suffixes, TEXT("Int64Int64"));
	AddUniqueCandidate(Suffixes, TEXT("BoolBool"));
	AddUniqueCandidate(Suffixes, TEXT("ByteByte"));
	AddUniqueCandidate(Suffixes, TEXT("ObjectObject"));
	AddUniqueCandidate(Suffixes, TEXT("NameName"));
	AddUniqueCandidate(Suffixes, TEXT("StrStr"));
	AddUniqueCandidate(Suffixes, TEXT("TextText"));
	AddUniqueCandidate(Suffixes, TEXT("VectorVector"));
	AddUniqueCandidate(Suffixes, TEXT("RotatorRotator"));
	AddUniqueCandidate(Suffixes, TEXT("TransformTransform"));

	for (const FString& Suffix : Suffixes)
	{
		const FString Candidate = BaseName + TEXT("_") + Suffix;
		if (FBlueprintGraphWriteFacade::FindFunctionByName(Candidate))
		{
			return Candidate;
		}
	}

	return BaseName;
}

static void ApplyFunctionPinTypes(UK2Node_PromotableOperator* Node, const UFunction* Function)
{
	if (!Node || !Function)
	{
		return;
	}

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (!Schema)
	{
		return;
	}

	FEdGraphPinType ReturnType;
	TArray<FEdGraphPinType> InputTypes;
	for (TFieldIterator<FProperty> PropIt(Function); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		FProperty* Param = *PropIt;
		if (!Param)
		{
			continue;
		}

		FEdGraphPinType PinType;
		if (!Schema->ConvertPropertyToPinType(Param, PinType))
		{
			continue;
		}

		if (Param->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			ReturnType = PinType;
		}
		else
		{
			InputTypes.Add(PinType);
		}
	}

	int32 InputIndex = 0;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin)
		{
			continue;
		}

		if (Pin->Direction == EGPD_Output && !ReturnType.PinCategory.IsNone())
		{
			Pin->PinType = ReturnType;
			continue;
		}

		if (Pin->Direction == EGPD_Input && InputTypes.IsValidIndex(InputIndex))
		{
			Pin->PinType = InputTypes[InputIndex++];
		}
	}
}
}

bool FPromotableOperatorNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::PromotableOperator;
}

UK2Node* FPromotableOperatorNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	if (!TargetGraph)
	{
		OutError = TEXT("PromotableOperator 节点生成失败：目标图表无效。");
		return nullptr;
	}

	if (NodeData.FunctionName.IsEmpty())
	{
		OutError = TEXT("PromotableOperator 节点生成失败：function_name 为空，请指定运算函数名（。Add, Multiply 等）。");
		return nullptr;
	}

	// 查找运算函数
	const FString ResolvedFunctionName = ResolveOperatorFunctionName(NodeData);
	UFunction* TargetFunction = FBlueprintGraphWriteFacade::FindFunctionByName(ResolvedFunctionName);
	if (!TargetFunction)
	{
		OutError = FString::Printf(TEXT("PromotableOperator 节点生成失败：未找到运算函数 '%s'。"), *NodeData.FunctionName);
		return nullptr;
	}

	UK2Node_PromotableOperator* NewNode = NewObject<UK2Node_PromotableOperator>(TargetGraph);
	TargetGraph->AddNode(NewNode, true, false);
	NewNode->CreateNewGuid();
	NewNode->PostPlacedNewNode();
	NewNode->SetFromFunction(TargetFunction);
	NewNode->NodePosX = static_cast<int32>(NodeData.X);
	NewNode->NodePosY = static_cast<int32>(NodeData.Y);
	NewNode->AllocateDefaultPins();
	ApplyFunctionPinTypes(NewNode, TargetFunction);

	FBlueprintGraphWriteFacade::ApplyDefaultValues(NewNode, NodeData.DefaultValues);
	NewNode->NodeConnectionListChanged();
	return NewNode;
}
