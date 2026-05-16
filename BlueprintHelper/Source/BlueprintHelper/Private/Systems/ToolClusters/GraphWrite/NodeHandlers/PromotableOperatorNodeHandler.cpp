#include "Systems/ToolClusters/GraphWrite/NodeHandlers/PromotableOperatorNodeHandler.h"

#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
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

static FString InferOperatorInputSemanticType(const FParsedNode& NodeData)
{
	const FString* AType = NodeData.ArgumentTypes.Find(TEXT("A"));
	const FString* BType = NodeData.ArgumentTypes.Find(TEXT("B"));
	if (AType && !AType->TrimStartAndEnd().IsEmpty())
	{
		return AType->TrimStartAndEnd();
	}
	if (BType && !BType->TrimStartAndEnd().IsEmpty())
	{
		return BType->TrimStartAndEnd();
	}

	const FBlueprintHelperCallFunctionPinType* APinType = NodeData.ArgumentPinTypes.Find(TEXT("A"));
	const FBlueprintHelperCallFunctionPinType* BPinType = NodeData.ArgumentPinTypes.Find(TEXT("B"));
	if (APinType && APinType->IsValid())
	{
		return !APinType->ObjectPath.IsEmpty() ? APinType->ObjectPath : APinType->Category;
	}
	if (BPinType && BPinType->IsValid())
	{
		return !BPinType->ObjectPath.IsEmpty() ? BPinType->ObjectPath : BPinType->Category;
	}

	const FString* A = NodeData.DefaultValues.Find(TEXT("A"));
	const FString* B = NodeData.DefaultValues.Find(TEXT("B"));
	if (A && B)
	{
		const FString AToken = A->TrimStartAndEnd().ToLower();
		const FString BToken = B->TrimStartAndEnd().ToLower();
		if ((AToken == TEXT("true") || AToken == TEXT("false")) && (BToken == TEXT("true") || BToken == TEXT("false")))
		{
			return TEXT("bool");
		}
		if ((LooksLikeFloatLiteral(*A) || LooksLikeFloatLiteral(*B))
			&& (LooksLikeIntegerLiteral(*A) || LooksLikeFloatLiteral(*A))
			&& (LooksLikeIntegerLiteral(*B) || LooksLikeFloatLiteral(*B)))
		{
			return TEXT("double");
		}
		if (LooksLikeIntegerLiteral(*A) && LooksLikeIntegerLiteral(*B))
		{
			return TEXT("int");
		}
	}

	return FString();
}

static bool TryMakePinTypeFromSemanticType(const FString& SemanticType, FEdGraphPinType& OutPinType)
{
	const FString Token = SemanticType.TrimStartAndEnd().ToLower();
	if (Token.IsEmpty())
	{
		return false;
	}

	OutPinType.ResetToDefaults();
	if (Token == TEXT("bool") || Token == TEXT("boolean"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		return true;
	}
	if (Token == TEXT("int") || Token == TEXT("integer"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
		return true;
	}
	if (Token == TEXT("int64") || Token == TEXT("long"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
		return true;
	}
	if (Token == TEXT("float"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
		return true;
	}
	if (Token == TEXT("double") || Token == TEXT("real") || Token == TEXT("number"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
		return true;
	}
	if (Token == TEXT("string"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_String;
		return true;
	}
	if (Token == TEXT("name"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Name;
		return true;
	}
	if (Token == TEXT("text"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Text;
		return true;
	}

	UScriptStruct* StructType = FindObject<UScriptStruct>(nullptr, *SemanticType);
	if (!StructType)
	{
		StructType = LoadObject<UScriptStruct>(nullptr, *SemanticType);
	}
	if (StructType)
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = StructType;
		return true;
	}

	return false;
}

static void ApplyRequestedOperatorPinTypes(UK2Node_PromotableOperator* Node, const FParsedNode& NodeData)
{
	if (!Node)
	{
		return;
	}

	FEdGraphPinType InputPinType;
	if (!TryMakePinTypeFromSemanticType(InferOperatorInputSemanticType(NodeData), InputPinType))
	{
		return;
	}

	const FString BaseName = ResolveOperatorBaseName(NodeData.FunctionName);
	const bool bBooleanOutput =
		BaseName.Equals(TEXT("Greater"), ESearchCase::IgnoreCase)
		|| BaseName.Equals(TEXT("GreaterEqual"), ESearchCase::IgnoreCase)
		|| BaseName.Equals(TEXT("Less"), ESearchCase::IgnoreCase)
		|| BaseName.Equals(TEXT("LessEqual"), ESearchCase::IgnoreCase)
		|| BaseName.Equals(TEXT("EqualEqual"), ESearchCase::IgnoreCase)
		|| BaseName.Equals(TEXT("NotEqual"), ESearchCase::IgnoreCase)
		|| BaseName.Equals(TEXT("BooleanAND"), ESearchCase::IgnoreCase)
		|| BaseName.Equals(TEXT("BooleanOR"), ESearchCase::IgnoreCase);

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin)
		{
			continue;
		}
		if (Pin->Direction == EGPD_Input && Pin->PinName != TEXT("Tolerance"))
		{
			Pin->PinType = InputPinType;
		}
		else if (Pin->Direction == EGPD_Output)
		{
			if (bBooleanOutput)
			{
				Pin->PinType.ResetToDefaults();
				Pin->PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
			}
			else
			{
				Pin->PinType = InputPinType;
			}
		}
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

	UK2Node_CallFunction* NewNode = NewObject<UK2Node_CallFunction>(TargetGraph);
	TargetGraph->AddNode(NewNode, true, false);
	NewNode->CreateNewGuid();
	NewNode->PostPlacedNewNode();
	NewNode->SetFromFunction(TargetFunction);
	NewNode->NodePosX = static_cast<int32>(NodeData.X);
	NewNode->NodePosY = static_cast<int32>(NodeData.Y);
	NewNode->AllocateDefaultPins();

	FBlueprintGraphWriteFacade::ApplyDefaultValues(NewNode, NodeData.DefaultValues);
	NewNode->NodeConnectionListChanged();
	return NewNode;
}
