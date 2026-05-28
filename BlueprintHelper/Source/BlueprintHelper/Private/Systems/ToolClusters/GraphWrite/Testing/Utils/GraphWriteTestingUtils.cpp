#include "Systems/ToolClusters/GraphWrite/Testing/Utils/GraphWriteTestingUtils.h"

#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "UObject/FieldIterator.h"
#include "UObject/UnrealType.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionVocabulary.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"
#include "Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperOpCoverageReadbackVerifier.h"
#include "Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperGenericOpsReadbackVerifier.h"
#include "Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperContainerActionReadbackVerifier.h"

// ========== From BlueprintHelperOpCoverageReadbackVerifier.cpp ==========

bool UGraphWriteTestingUtils::Fail(FString& OutFailure, const FString& Message)
{
	OutFailure = Message;
	return false;
}

const FBlueprintHelperCallFunctionCandidateInfo* UGraphWriteTestingUtils::FirstCandidate(
	const FBlueprintHelperActionResolutionResult& Result)
{
	return Result.CandidateActions.Num() > 0 ? &Result.CandidateActions[0] : nullptr;
}

const FBlueprintHelperCallFunctionCandidateInfo* UGraphWriteTestingUtils::FindSelectedCandidate(
	const FBlueprintHelperActionResolutionResult& Result)
{
	const FString SelectedStableId = !Result.SelectedStableId.TrimStartAndEnd().IsEmpty()
		? Result.SelectedStableId.TrimStartAndEnd()
		: Result.FunctionCandidate.StableId.TrimStartAndEnd();
	if (!SelectedStableId.IsEmpty())
	{
		if (const FBlueprintHelperCallFunctionCandidateInfo* Candidate = Result.CandidateActions.FindByPredicate(
			[&SelectedStableId](const FBlueprintHelperCallFunctionCandidateInfo& CandidateInfo)
			{
				return CandidateInfo.StableId.Equals(SelectedStableId, ESearchCase::IgnoreCase);
			}))
		{
			return Candidate;
		}
	}

	return Result.CandidateActions.Num() == 1 ? FirstCandidate(Result) : nullptr;
}

bool UGraphWriteTestingUtils::FunctionHasInputPin(const UFunction* Function, const FString& PinName)
{
	if (!Function || PinName.TrimStartAndEnd().IsEmpty())
	{
		return false;
	}

	for (TFieldIterator<FProperty> PropIt(Function); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		const FProperty* Property = *PropIt;
		if (Property
			&& !Property->HasAnyPropertyFlags(CPF_ReturnParm)
			&& Property->GetName().Equals(PinName, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

bool UGraphWriteTestingUtils::FunctionHasReturnPin(const UFunction* Function)
{
	if (!Function)
	{
		return false;
	}

	for (TFieldIterator<FProperty> PropIt(Function); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		const FProperty* Property = *PropIt;
		if (Property && Property->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			return true;
		}
	}
	return false;
}

// ========== From BlueprintHelperGenericOpsReadbackVerifier.cpp ==========

bool UGraphWriteTestingUtils::Fail(
	const TCHAR* Code,
	const FString& Message,
	FString& OutFailureCode,
	FString& OutFailure)
{
	OutFailureCode = Code;
	OutFailure = Message;
	return false;
}

FString UGraphWriteTestingUtils::FactValue(
	const FBlueprintHelperCallFunctionCandidateInfo& Candidate,
	const FString& Key)
{
	if (const FString* Value = Candidate.ReadbackFacts.Find(Key))
	{
		return Value->TrimStartAndEnd();
	}
	if (const FString* Value = Candidate.CapabilityFacts.Find(Key))
	{
		return Value->TrimStartAndEnd();
	}
	return FString();
}

bool UGraphWriteTestingUtils::HasPinNamed(
	const UEdGraphNode* Node,
	const FString& PinName,
	const TOptional<EEdGraphPinDirection> Direction)
{
	if (!Node)
	{
		return false;
	}
	for (const UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin || !Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
		{
			continue;
		}
		if (!Direction.IsSet() || Pin->Direction == Direction.GetValue())
		{
			return true;
		}
	}
	return false;
}

// ========== From BlueprintHelperContainerActionReadbackVerifier.cpp ==========

FString UGraphWriteTestingUtils::NormalizeToken(const FString& Value)
{
	return Value.TrimStartAndEnd().ToLower();
}

FString UGraphWriteTestingUtils::MakeOperationId(const FBlueprintHelperContainerActionReadbackExpectation& Expectation)
{
	if (!Expectation.OperationId.TrimStartAndEnd().IsEmpty())
	{
		return Expectation.OperationId.TrimStartAndEnd();
	}
	return FString::Printf(
		TEXT("container.%s.%s"),
		*NormalizeToken(Expectation.ContainerKind),
		*NormalizeToken(Expectation.ContainerOperation));
}

FString UGraphWriteTestingUtils::ExtractFunctionName(const FString& FunctionQuery)
{
	int32 ColonIndex = INDEX_NONE;
	if (!FunctionQuery.FindLastChar(TEXT(':'), ColonIndex) || ColonIndex >= FunctionQuery.Len() - 1)
	{
		return FunctionQuery.TrimStartAndEnd();
	}
	return FunctionQuery.Mid(ColonIndex + 1).TrimStartAndEnd();
}

EPinContainerType UGraphWriteTestingUtils::ExpectedContainerType(const FString& ContainerKind)
{
	const FString Kind = NormalizeToken(ContainerKind);
	if (Kind == TEXT("array"))
	{
		return EPinContainerType::Array;
	}
	if (Kind == TEXT("map"))
	{
		return EPinContainerType::Map;
	}
	if (Kind == TEXT("set"))
	{
		return EPinContainerType::Set;
	}
	return EPinContainerType::None;
}

FName UGraphWriteTestingUtils::CategoryForTypeToken(const FString& TypeToken)
{
	const FString Type = NormalizeToken(TypeToken);
	if (Type.IsEmpty())
	{
		return NAME_None;
	}
	if (Type == TEXT("bool") || Type == TEXT("boolean"))
	{
		return UEdGraphSchema_K2::PC_Boolean;
	}
	if (Type == TEXT("int") || Type == TEXT("integer"))
	{
		return UEdGraphSchema_K2::PC_Int;
	}
	if (Type == TEXT("float") || Type == TEXT("double") || Type == TEXT("real"))
	{
		return UEdGraphSchema_K2::PC_Real;
	}
	if (Type == TEXT("string"))
	{
		return UEdGraphSchema_K2::PC_String;
	}
	if (Type == TEXT("name"))
	{
		return UEdGraphSchema_K2::PC_Name;
	}
	if (Type == TEXT("text"))
	{
		return UEdGraphSchema_K2::PC_Text;
	}
	return FName(*TypeToken.TrimStartAndEnd());
}

FString UGraphWriteTestingUtils::ExpectedElementType(const FBlueprintHelperContainerActionReadbackExpectation& Expectation)
{
	return !Expectation.ElementType.TrimStartAndEnd().IsEmpty()
		? Expectation.ElementType.TrimStartAndEnd()
		: Expectation.ValueType.TrimStartAndEnd();
}

bool UGraphWriteTestingUtils::TryBuildExpectedRolePinType(
	const FBlueprintHelperContainerActionReadbackExpectation& Expectation,
	const FString& RoleName,
	FExpectedContainerActionPinType& OutType)
{
	const FString Role = NormalizeToken(RoleName);
	const FString Kind = NormalizeToken(Expectation.ContainerKind);
	if (Role == TEXT("target"))
	{
		OutType.ContainerType = ExpectedContainerType(Expectation.ContainerKind);
		if (Kind == TEXT("map"))
		{
			OutType.CategoryToken = Expectation.KeyType;
			OutType.TerminalCategoryToken = Expectation.ValueType;
		}
		else
		{
			OutType.CategoryToken = ExpectedElementType(Expectation);
		}
		return true;
	}
	if (Role == TEXT("item"))
	{
		OutType.CategoryToken = ExpectedElementType(Expectation);
		return true;
	}
	if (Role == TEXT("items"))
	{
		OutType.CategoryToken = ExpectedElementType(Expectation);
		OutType.ContainerType = EPinContainerType::Array;
		return true;
	}
	if (Role == TEXT("other"))
	{
		OutType.CategoryToken = ExpectedElementType(Expectation);
		OutType.ContainerType = ExpectedContainerType(Expectation.ContainerKind);
		return true;
	}
	if (Role == TEXT("key"))
	{
		OutType.CategoryToken = Expectation.KeyType;
		return true;
	}
	if (Role == TEXT("value"))
	{
		OutType.CategoryToken = Expectation.ValueType;
		return true;
	}
	if (Role == TEXT("index"))
	{
		OutType.CategoryToken = TEXT("int");
		return true;
	}
	if (Role == TEXT("result"))
	{
		return TryBuildExpectedResultPinType(Expectation, OutType);
	}
	return false;
}

bool UGraphWriteTestingUtils::TryBuildExpectedResultPinType(
	const FBlueprintHelperContainerActionReadbackExpectation& Expectation,
	FExpectedContainerActionPinType& OutType)
{
	const FString Kind = NormalizeToken(Expectation.ContainerKind);
	const FString Operation = NormalizeToken(Expectation.ContainerOperation);
	if (Operation == TEXT("contains"))
	{
		OutType.CategoryToken = TEXT("bool");
		return true;
	}
	if (Operation == TEXT("length")
		|| Operation == TEXT("last_index")
		|| Operation == TEXT("get_last_index")
		|| (Kind == TEXT("array") && (Operation == TEXT("find") || Operation == TEXT("add") || Operation == TEXT("add_unique"))))
	{
		OutType.CategoryToken = TEXT("int");
		return true;
	}
	if (Operation == TEXT("is_empty") || Operation == TEXT("is_not_empty") || Operation == TEXT("identical") || Operation == TEXT("is_valid_index"))
	{
		OutType.CategoryToken = TEXT("bool");
		return true;
	}
	if ((Kind == TEXT("map") || Kind == TEXT("set")) && Operation == TEXT("remove"))
	{
		OutType.CategoryToken = TEXT("bool");
		return true;
	}
	if (Kind == TEXT("array") && (Operation == TEXT("get") || Operation == TEXT("random") || Operation == TEXT("random_from_stream")))
	{
		OutType.CategoryToken = ExpectedElementType(Expectation);
		return true;
	}
	if (Kind == TEXT("map") && Operation == TEXT("find"))
	{
		OutType.CategoryToken = Expectation.ValueType;
		return true;
	}
	if (Kind == TEXT("map") && Operation == TEXT("keys"))
	{
		OutType.CategoryToken = Expectation.KeyType;
		OutType.ContainerType = EPinContainerType::Array;
		return true;
	}
	if (Kind == TEXT("map") && Operation == TEXT("values"))
	{
		OutType.CategoryToken = Expectation.ValueType;
		OutType.ContainerType = EPinContainerType::Array;
		return true;
	}
	if (Kind == TEXT("set") && Operation == TEXT("to_array"))
	{
		OutType.CategoryToken = ExpectedElementType(Expectation);
		OutType.ContainerType = EPinContainerType::Array;
		return true;
	}
	if (Kind == TEXT("set") && Operation == TEXT("get_item_by_index"))
	{
		OutType.CategoryToken = ExpectedElementType(Expectation);
		return true;
	}
	if (Kind == TEXT("set") && (Operation == TEXT("intersection") || Operation == TEXT("union") || Operation == TEXT("difference")))
	{
		OutType.CategoryToken = ExpectedElementType(Expectation);
		OutType.ContainerType = EPinContainerType::Set;
		return true;
	}
	if (Kind == TEXT("array") && Operation == TEXT("filter_array"))
	{
		OutType.CategoryToken = ExpectedElementType(Expectation);
		OutType.ContainerType = EPinContainerType::Array;
		return true;
	}
	return false;
}

bool UGraphWriteTestingUtils::PinMatchesExpectedType(
	const UEdGraphPin* Pin,
	const FExpectedContainerActionPinType& Expected,
	FString& OutReason)
{
	if (!Pin)
	{
		OutReason = TEXT("pin is null");
		return false;
	}
	const FName ExpectedCategory = CategoryForTypeToken(Expected.CategoryToken);
	if (!ExpectedCategory.IsNone() && Pin->PinType.PinCategory != ExpectedCategory)
	{
		OutReason = FString::Printf(
			TEXT("category expected %s but got %s"),
			*ExpectedCategory.ToString(),
			*Pin->PinType.PinCategory.ToString());
		return false;
	}
	if (Expected.ContainerType != EPinContainerType::None && Pin->PinType.ContainerType != Expected.ContainerType)
	{
		OutReason = FString::Printf(TEXT("container type expected %d but got %d"), static_cast<int32>(Expected.ContainerType), static_cast<int32>(Pin->PinType.ContainerType));
		return false;
	}
	const FName ExpectedTerminalCategory = CategoryForTypeToken(Expected.TerminalCategoryToken);
	if (!ExpectedTerminalCategory.IsNone()
		&& Pin->PinType.PinValueType.TerminalCategory != ExpectedTerminalCategory)
	{
		OutReason = FString::Printf(
			TEXT("terminal category expected %s but got %s"),
			*ExpectedTerminalCategory.ToString(),
			*Pin->PinType.PinValueType.TerminalCategory.ToString());
		return false;
	}
	return true;
}

bool UGraphWriteTestingUtils::TextMentionsTarget(const FString& Text, const FString& TargetName)
{
	return !TargetName.IsEmpty() && Text.Contains(TargetName, ESearchCase::IgnoreCase);
}

bool UGraphWriteTestingUtils::PinOrOwnerMentionsTarget(const UEdGraphPin* Pin, const FString& TargetName)
{
	if (!Pin)
	{
		return false;
	}
	if (TextMentionsTarget(Pin->PinName.ToString(), TargetName))
	{
		return true;
	}

	const UEdGraphNode* Owner = Pin->GetOwningNode();
	return Owner
		&& (TextMentionsTarget(Owner->GetName(), TargetName)
			|| TextMentionsTarget(Owner->GetNodeTitle(ENodeTitleType::ListView).ToString(), TargetName)
			|| TextMentionsTarget(Owner->GetNodeTitle(ENodeTitleType::FullTitle).ToString(), TargetName));
}

UEdGraphPin* UGraphWriteTestingUtils::FindPinByName(UK2Node_CallFunction* Node, const FString& PinName)
{
	if (!Node || PinName.IsEmpty())
	{
		return nullptr;
	}
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
		{
			return Pin;
		}
	}
	return nullptr;
}

const FBlueprintHelperContainerActionRoleBinding* UGraphWriteTestingUtils::FindRoleBinding(
	const FBlueprintHelperContainerActionSpec& Spec,
	const FString& Role)
{
	return Spec.RoleBindings.FindByPredicate(
		[&Role](const FBlueprintHelperContainerActionRoleBinding& Binding)
		{
			return Binding.RoleName.Equals(Role, ESearchCase::IgnoreCase);
		});
}

bool UGraphWriteTestingUtils::IsWildcardPin(const UEdGraphPin* Pin)
{
	return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard;
}

bool UGraphWriteTestingUtils::TargetPinLinksToVariable(const UEdGraphPin* TargetPin, const FString& TargetName)
{
	if (!TargetPin || TargetName.IsEmpty())
	{
		return false;
	}
	for (const UEdGraphPin* LinkedPin : TargetPin->LinkedTo)
	{
		if (PinOrOwnerMentionsTarget(LinkedPin, TargetName))
		{
			return true;
		}
	}
	return false;
}

bool UGraphWriteTestingUtils::HasNonWildcardOutput(UK2Node_CallFunction* Node)
{
	if (!Node)
	{
		return false;
	}
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin
			&& Pin->Direction == EGPD_Output
			&& Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec
			&& !IsWildcardPin(Pin))
		{
			return true;
		}
	}
	return false;
}

UEdGraphPin* UGraphWriteTestingUtils::FindFirstNonWildcardOutput(UK2Node_CallFunction* Node)
{
	if (!Node)
	{
		return nullptr;
	}
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin
			&& Pin->Direction == EGPD_Output
			&& Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec
			&& !IsWildcardPin(Pin))
		{
			return Pin;
		}
	}
	return nullptr;
}

bool UGraphWriteTestingUtils::HasLinkedExecFlow(UK2Node_CallFunction* Node)
{
	if (!Node)
	{
		return false;
	}

	bool bHasLinkedInput = false;
	bool bHasLinkedOutput = false;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin || Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec || Pin->LinkedTo.Num() == 0)
		{
			continue;
		}
		bHasLinkedInput |= Pin->Direction == EGPD_Input;
		bHasLinkedOutput |= Pin->Direction == EGPD_Output;
	}
	return bHasLinkedInput && bHasLinkedOutput;
}

UK2Node_CallFunction* UGraphWriteTestingUtils::FindContainerActionNode(
	const UEdGraph* Graph,
	const FString& FunctionName)
{
	if (!Graph || FunctionName.IsEmpty())
	{
		return nullptr;
	}
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);
		if (CallNode && CallNode->GetFunctionName().ToString().Equals(FunctionName, ESearchCase::IgnoreCase))
		{
			return CallNode;
		}
	}
	return nullptr;
}
