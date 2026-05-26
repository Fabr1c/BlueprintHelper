#include "Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperContainerActionReadbackVerifier.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionVocabulary.h"

namespace
{
static FString NormalizeToken(const FString& Value)
{
	return Value.TrimStartAndEnd().ToLower();
}

static FString MakeOperationId(const FBlueprintHelperContainerActionReadbackExpectation& Expectation)
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

static FString ExtractFunctionName(const FString& FunctionQuery)
{
	int32 ColonIndex = INDEX_NONE;
	if (!FunctionQuery.FindLastChar(TEXT(':'), ColonIndex) || ColonIndex >= FunctionQuery.Len() - 1)
	{
		return FunctionQuery.TrimStartAndEnd();
	}
	return FunctionQuery.Mid(ColonIndex + 1).TrimStartAndEnd();
}

static EPinContainerType ExpectedContainerType(const FString& ContainerKind)
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

static FName CategoryForTypeToken(const FString& TypeToken)
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

struct FExpectedContainerActionPinType
{
	FString CategoryToken;
	EPinContainerType ContainerType = EPinContainerType::None;
	FString TerminalCategoryToken;
};

static FString ExpectedElementType(const FBlueprintHelperContainerActionReadbackExpectation& Expectation)
{
	return !Expectation.ElementType.TrimStartAndEnd().IsEmpty()
		? Expectation.ElementType.TrimStartAndEnd()
		: Expectation.ValueType.TrimStartAndEnd();
}

static bool TryBuildExpectedResultPinType(
	const FBlueprintHelperContainerActionReadbackExpectation& Expectation,
	FExpectedContainerActionPinType& OutType);

static bool TryBuildExpectedRolePinType(
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

static bool TryBuildExpectedResultPinType(
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

static bool PinMatchesExpectedType(
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

static bool TextMentionsTarget(const FString& Text, const FString& TargetName)
{
	return !TargetName.IsEmpty() && Text.Contains(TargetName, ESearchCase::IgnoreCase);
}

static bool PinOrOwnerMentionsTarget(const UEdGraphPin* Pin, const FString& TargetName)
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

static UEdGraphPin* FindPinByName(UK2Node_CallFunction* Node, const FString& PinName)
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

static const FBlueprintHelperContainerActionRoleBinding* FindRoleBinding(
	const FBlueprintHelperContainerActionSpec& Spec,
	const FString& Role)
{
	return Spec.RoleBindings.FindByPredicate(
		[&Role](const FBlueprintHelperContainerActionRoleBinding& Binding)
		{
			return Binding.RoleName.Equals(Role, ESearchCase::IgnoreCase);
		});
}

static bool IsWildcardPin(const UEdGraphPin* Pin)
{
	return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard;
}

static bool TargetPinLinksToVariable(const UEdGraphPin* TargetPin, const FString& TargetName)
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

static bool HasNonWildcardOutput(UK2Node_CallFunction* Node)
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

static UEdGraphPin* FindFirstNonWildcardOutput(UK2Node_CallFunction* Node)
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

static bool HasLinkedExecFlow(UK2Node_CallFunction* Node)
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

static UK2Node_CallFunction* FindContainerActionNode(
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
} // namespace

bool FBlueprintHelperContainerActionReadbackVerifier::Verify(
	const UBlueprint* Blueprint,
	const UEdGraph* Graph,
	const FBlueprintHelperContainerActionReadbackExpectation& Expectation,
	FString& OutFailure)
{
	OutFailure.Reset();
	if (!Blueprint)
	{
		OutFailure = TEXT("container_action readback failed: Blueprint is null.");
		return false;
	}
	if (!Graph)
	{
		OutFailure = TEXT("container_action readback failed: Graph is null.");
		return false;
	}

	const FString OperationId = MakeOperationId(Expectation);
	const FBlueprintHelperContainerActionSpec* Spec =
		FBlueprintHelperContainerActionVocabulary::Find(Expectation.ContainerKind, Expectation.ContainerOperation);
	if (!Spec)
	{
		OutFailure = FString::Printf(TEXT("container_action readback failed: unsupported operation %s."), *OperationId);
		return false;
	}
	if (!Spec->OperationId.Equals(OperationId, ESearchCase::IgnoreCase))
	{
		OutFailure = FString::Printf(
			TEXT("container_action readback failed: operation id mismatch, expected %s but vocabulary has %s."),
			*OperationId,
			*Spec->OperationId);
		return false;
	}

	UK2Node_CallFunction* CallNode = FindContainerActionNode(Graph, ExtractFunctionName(Spec->StableUFunctionPath.IsEmpty() ? Spec->FunctionQuery : Spec->StableUFunctionPath));
	if (!CallNode)
	{
		OutFailure = FString::Printf(TEXT("container_action readback failed: generated node not found for %s."), *OperationId);
		return false;
	}

	const TArray<FString>& RolesToVerify = Expectation.RequiredRoles.Num() > 0
		? Expectation.RequiredRoles
		: Spec->ReadbackPinRoles;
	for (const FString& Role : RolesToVerify)
	{
		const FBlueprintHelperContainerActionRoleBinding* Binding = FindRoleBinding(*Spec, Role);
		if (!Binding)
		{
			OutFailure = FString::Printf(TEXT("container_action readback failed: role %s has no vocabulary binding."), *Role);
			return false;
		}

		UEdGraphPin* RolePin = FindPinByName(CallNode, Binding->FunctionPinName);
		if (!RolePin)
		{
			OutFailure = FString::Printf(
				TEXT("container_action readback failed: role %s pin %s is missing."),
				*Role,
				*Binding->FunctionPinName);
			return false;
		}
		if (IsWildcardPin(RolePin))
		{
			OutFailure = FString::Printf(
				TEXT("container_action readback failed: role %s pin %s stayed wildcard."),
				*Role,
				*Binding->FunctionPinName);
			return false;
		}
		FExpectedContainerActionPinType ExpectedRoleType;
		if (TryBuildExpectedRolePinType(Expectation, Role, ExpectedRoleType))
		{
			FString TypeReason;
			if (!PinMatchesExpectedType(RolePin, ExpectedRoleType, TypeReason))
			{
				OutFailure = FString::Printf(
					TEXT("container_action readback failed: role %s pin %s type mismatch: %s."),
					*Role,
					*Binding->FunctionPinName,
					*TypeReason);
				return false;
			}
		}
	}

	if (!Expectation.TargetName.TrimStartAndEnd().IsEmpty())
	{
		const FBlueprintHelperContainerActionRoleBinding* TargetBinding = FindRoleBinding(*Spec, TEXT("target"));
		UEdGraphPin* TargetPin = TargetBinding ? FindPinByName(CallNode, TargetBinding->FunctionPinName) : nullptr;
		if (!TargetPin)
		{
			OutFailure = TEXT("container_action readback failed: target pin is missing.");
			return false;
		}
		if (!TargetPinLinksToVariable(TargetPin, Expectation.TargetName))
		{
			OutFailure = FString::Printf(
				TEXT("container_action readback failed: target pin is not linked to %s."),
				*Expectation.TargetName);
			return false;
		}
		const EPinContainerType ExpectedType = ExpectedContainerType(Expectation.ContainerKind);
		if (ExpectedType != EPinContainerType::None && TargetPin->PinType.ContainerType != ExpectedType)
		{
			OutFailure = FString::Printf(
				TEXT("container_action readback failed: target pin container type mismatch for %s."),
				*Expectation.ContainerKind);
			return false;
		}
	}

	if (Expectation.bRequiresOutput && !HasNonWildcardOutput(CallNode))
	{
		OutFailure = FString::Printf(TEXT("container_action readback failed: output pin missing or wildcard for %s."), *OperationId);
		return false;
	}
	if (Expectation.bRequiresOutput)
	{
		UEdGraphPin* OutputPin = FindFirstNonWildcardOutput(CallNode);
		FExpectedContainerActionPinType ExpectedResultType;
		if (TryBuildExpectedResultPinType(Expectation, ExpectedResultType))
		{
			FString TypeReason;
			if (!PinMatchesExpectedType(OutputPin, ExpectedResultType, TypeReason))
			{
				OutFailure = FString::Printf(
					TEXT("container_action readback failed: output pin type mismatch for %s: %s."),
					*OperationId,
					*TypeReason);
				return false;
			}
		}
	}
	if (Expectation.bRequiresExecFlow && !HasLinkedExecFlow(CallNode))
	{
		OutFailure = FString::Printf(TEXT("container_action readback failed: exec flow is not fully linked for %s."), *OperationId);
		return false;
	}

	return true;
}
