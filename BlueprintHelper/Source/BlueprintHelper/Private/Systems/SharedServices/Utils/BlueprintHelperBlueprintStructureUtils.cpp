// BlueprintHelper Utils -- 蓝图结构查询与操作函数库实现

#include "Systems/SharedServices/Utils/BlueprintHelperBlueprintStructureUtils.h"

#include "Systems/SharedServices/Utils/BlueprintHelperPinTypeSpecUtils.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLocalVariableService.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphParsedTypes.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Tunnel.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include <type_traits>

// ─── Template helper ───

static bool BlueprintHelperPinMatches(
	const FEdGraphPinType& ExistingType,
	const FEdGraphPinType& ExpectedType)
{
	return ExistingType.PinCategory == ExpectedType.PinCategory
		&& ExistingType.PinSubCategory == ExpectedType.PinSubCategory
		&& ExistingType.PinSubCategoryObject == ExpectedType.PinSubCategoryObject
		&& ExistingType.ContainerType == ExpectedType.ContainerType
		&& ExistingType.bIsReference == ExpectedType.bIsReference
		&& ExistingType.bIsConst == ExpectedType.bIsConst;
}

static bool BlueprintHelperUserPinMatches(
	const TSharedPtr<FUserPinInfo>& ExistingPin,
	const FName PinName,
	const FEdGraphPinType& ExpectedType,
	const EEdGraphPinDirection ExpectedDirection)
{
	return ExistingPin.IsValid()
		&& ExistingPin->PinName == PinName
		&& ExistingPin->DesiredPinDirection == ExpectedDirection
		&& BlueprintHelperPinMatches(ExistingPin->PinType, ExpectedType);
}

template <typename TNode>
static TSharedPtr<FUserPinInfo> FindUserPinByName(TNode* Node, const FName PinName)
{
	if (!Node)
	{
		return nullptr;
	}

	for (const TSharedPtr<FUserPinInfo>& ExistingPin : Node->UserDefinedPins)
	{
		if (ExistingPin.IsValid() && ExistingPin->PinName == PinName)
		{
			return ExistingPin;
		}
	}
	return nullptr;
}

static bool BlueprintHelperGraphPinMatches(
	const UEdGraphPin* ExistingPin,
	const FEdGraphPinType& ExpectedType,
	const EEdGraphPinDirection ExpectedDirection)
{
	return ExistingPin
		&& ExistingPin->Direction == ExpectedDirection
		&& BlueprintHelperPinMatches(ExistingPin->PinType, ExpectedType);
}

static bool BlueprintHelperIsExecPin(const UEdGraphPin* Pin)
{
	return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
}

static UEdGraphPin* BlueprintHelperFindFirstExecPin(UEdGraphNode* Node, const EEdGraphPinDirection Direction)
{
	if (!Node)
	{
		return nullptr;
	}

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->Direction == Direction && BlueprintHelperIsExecPin(Pin))
		{
			return Pin;
		}
	}
	return nullptr;
}

static bool BlueprintHelperPinsAreLinkedToEachOther(const UEdGraphPin* FirstPin, const UEdGraphPin* SecondPin)
{
	return FirstPin &&
		SecondPin &&
		FirstPin->LinkedTo.Contains(const_cast<UEdGraphPin*>(SecondPin)) &&
		SecondPin->LinkedTo.Contains(const_cast<UEdGraphPin*>(FirstPin));
}

static bool BlueprintHelperMacroGraphHasBodyNodes(const UEdGraph* Graph)
{
	if (!Graph)
	{
		return false;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node && !Cast<UK2Node_Tunnel>(Node))
		{
			return true;
		}
	}
	return false;
}

static void BlueprintHelperFindMacroTunnelBoundaries(
	UEdGraph* Graph,
	UK2Node_Tunnel*& OutEntryNode,
	UK2Node_Tunnel*& OutExitNode)
{
	OutEntryNode = nullptr;
	OutExitNode = nullptr;
	if (!Graph)
	{
		return;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		UK2Node_Tunnel* TunnelNode = Cast<UK2Node_Tunnel>(Node);
		if (!TunnelNode)
		{
			continue;
		}

		if (!OutEntryNode && TunnelNode->bCanHaveOutputs)
		{
			OutEntryNode = TunnelNode;
		}
		else if (!OutExitNode && TunnelNode->bCanHaveInputs)
		{
			OutExitNode = TunnelNode;
		}
	}
}

static UK2Node_Tunnel* BlueprintHelperCreateMacroTunnelBoundary(
	UEdGraph* Graph,
	const bool bEntry)
{
	if (!Graph)
	{
		return nullptr;
	}

	FGraphNodeCreator<UK2Node_Tunnel> NodeCreator(*Graph);
	UK2Node_Tunnel* TunnelNode = NodeCreator.CreateNode(true);
	if (!TunnelNode)
	{
		return nullptr;
	}

	TunnelNode->bCanHaveOutputs = bEntry;
	TunnelNode->bCanHaveInputs = !bEntry;
	TunnelNode->NodePosX = bEntry ? 0 : 240;
	TunnelNode->NodePosY = 0;
	NodeCreator.Finalize();
	return TunnelNode;
}

static bool BlueprintHelperEnsureMacroTunnelBoundaries(UEdGraph* Graph, FString& OutError)
{
	if (!Graph)
	{
		OutError = TEXT("add_macro_graph failed: target macro graph is invalid.");
		return false;
	}

	UK2Node_Tunnel* EntryNode = nullptr;
	UK2Node_Tunnel* ExitNode = nullptr;
	BlueprintHelperFindMacroTunnelBoundaries(Graph, EntryNode, ExitNode);
	if (EntryNode && ExitNode)
	{
		return true;
	}

	if (BlueprintHelperMacroGraphHasBodyNodes(Graph))
	{
		OutError = TEXT("add_macro_graph failed: existing macro graph has body nodes but no complete tunnel boundary.");
		return false;
	}

	Graph->Modify();
	if (!EntryNode)
	{
		EntryNode = BlueprintHelperCreateMacroTunnelBoundary(Graph, true);
	}
	if (!ExitNode)
	{
		ExitNode = BlueprintHelperCreateMacroTunnelBoundary(Graph, false);
	}

	if (!EntryNode || !ExitNode)
	{
		OutError = TEXT("add_macro_graph failed: could not create macro tunnel boundary nodes.");
		return false;
	}

	Graph->NotifyGraphChanged();
	return true;
}

static bool BlueprintHelperEnsureEmptyMacroGraphDefaultExecLink(UEdGraph* Graph, FString& OutError)
{
	if (!Graph || BlueprintHelperMacroGraphHasBodyNodes(Graph))
	{
		return true;
	}

	UK2Node_Tunnel* EntryNode = nullptr;
	UK2Node_Tunnel* ExitNode = nullptr;
	BlueprintHelperFindMacroTunnelBoundaries(Graph, EntryNode, ExitNode);
	if (!EntryNode || !ExitNode)
	{
		return true;
	}

	UEdGraphPin* EntryExecOut = BlueprintHelperFindFirstExecPin(EntryNode, EGPD_Output);
	UEdGraphPin* ExitExecIn = BlueprintHelperFindFirstExecPin(ExitNode, EGPD_Input);
	if (!EntryExecOut || !ExitExecIn || BlueprintHelperPinsAreLinkedToEachOther(EntryExecOut, ExitExecIn))
	{
		return true;
	}

	if (EntryExecOut->LinkedTo.Num() > 0 || ExitExecIn->LinkedTo.Num() > 0)
	{
		return true;
	}

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (!Schema)
	{
		OutError = TEXT("add_macro_graph failed: K2 schema is unavailable for default tunnel exec link.");
		return false;
	}

	EntryExecOut->Modify();
	ExitExecIn->Modify();
	if (!Schema->TryCreateConnection(EntryExecOut, ExitExecIn) ||
		!BlueprintHelperPinsAreLinkedToEachOther(EntryExecOut, ExitExecIn))
	{
		OutError = TEXT("add_macro_graph failed: could not connect default macro tunnel exec pins.");
		return false;
	}

	Graph->NotifyGraphChanged();
	return true;
}

template <typename TNode>
static void AppendUserPins(
	TNode* Node,
	const TArray<TSharedPtr<FJsonValue>>* PinsArray,
	const EEdGraphPinDirection Direction,
	const FName DefaultCategory,
	const bool bSkipExistingNames)
{
	if (!Node || !PinsArray)
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& PinValue : *PinsArray)
	{
		const TSharedPtr<FJsonObject> PinObject = PinValue.IsValid() ? PinValue->AsObject() : nullptr;
		if (!PinObject.IsValid())
		{
			continue;
		}

		FString PinName;
		PinObject->TryGetStringField(TEXT("name"), PinName);
		if (PinName.IsEmpty())
		{
			continue;
		}

		const FName PinFName(*PinName);

		FEdGraphPinType PinType;
		UBlueprintHelperBlueprintStructureUtils::ReadOptionalPinTypeOrDefault(PinObject, DefaultCategory, PinType);

		if constexpr (std::is_base_of_v<UK2Node_Tunnel, TNode>)
		{
			UK2Node_Tunnel* TunnelNode = Node;
			const TSharedPtr<FUserPinInfo> ExistingUserPin = FindUserPinByName(TunnelNode, PinFName);
			UEdGraphPin* ExistingGraphPin = TunnelNode->FindPin(PinFName);
			if (bSkipExistingNames
				&& BlueprintHelperUserPinMatches(ExistingUserPin, PinFName, PinType, Direction)
				&& BlueprintHelperGraphPinMatches(ExistingGraphPin, PinType, Direction))
			{
				continue;
			}

			if (ExistingUserPin.IsValid() || ExistingGraphPin)
			{
				TunnelNode->RemoveUserDefinedPinByName(PinFName);
			}

			TunnelNode->CreateUserDefinedPin(PinFName, PinType, Direction, false);
			continue;
		}

		if (bSkipExistingNames && FindUserPinByName(Node, PinFName).IsValid())
		{
			continue;
		}

		TSharedPtr<FUserPinInfo> NewPin = MakeShared<FUserPinInfo>();
		NewPin->PinName = PinFName;
		NewPin->PinType = PinType;
		NewPin->DesiredPinDirection = Direction;
		Node->UserDefinedPins.Add(NewPin);
	}

	Node->ReconstructNode();
}

// ─── Class method implementations ───

bool UBlueprintHelperBlueprintStructureUtils::ParseStableNodeGuid(const FString& NodeId, FGuid& OutGuid)
{
	return FGuid::ParseExact(NodeId, EGuidFormats::Digits, OutGuid)
		|| FGuid::ParseExact(NodeId, EGuidFormats::DigitsWithHyphens, OutGuid);
}

void UBlueprintHelperBlueprintStructureUtils::ReadParsedPinType(const TSharedPtr<FJsonObject>& PinTypeObject, FParsedPinType& OutParsedPinType)
{
	FBlueprintHelperPinTypeSpecError Error;
	FBlueprintHelperPinTypeSpecUtils::ReadParsedPinType(PinTypeObject, OutParsedPinType, Error, TEXT("pin_type"));
}

FParsedPinType UBlueprintHelperBlueprintStructureUtils::ParsedPinTypeFromJson(const TSharedPtr<FJsonObject>& PinTypeObject)
{
	FParsedPinType ParsedPinType;
	FBlueprintHelperPinTypeSpecError Error;
	FBlueprintHelperPinTypeSpecUtils::ReadParsedPinType(PinTypeObject, ParsedPinType, Error, TEXT("pin_type"));
	return ParsedPinType;
}

bool UBlueprintHelperBlueprintStructureUtils::TryConvertPinTypeObject(
	const TSharedPtr<FJsonObject>& PinTypeObject,
	FEdGraphPinType& OutPinType,
	FString& OutError)
{
	return FBlueprintHelperPinTypeSpecUtils::TryConvertPinTypeObject(PinTypeObject, OutPinType, OutError);
}

void UBlueprintHelperBlueprintStructureUtils::ReadOptionalPinTypeOrDefault(
	const TSharedPtr<FJsonObject>& Payload,
	const FName DefaultCategory,
	FEdGraphPinType& OutPinType)
{
	OutPinType = FEdGraphPinType();
	OutPinType.PinCategory = DefaultCategory;

	const TSharedPtr<FJsonObject>* PinTypeObject = nullptr;
	if (Payload.IsValid() && Payload->TryGetObjectField(TEXT("pin_type"), PinTypeObject) && PinTypeObject && PinTypeObject->IsValid())
	{
		FString ConvertError;
		FBlueprintHelperPinTypeSpecUtils::TryConvertPinTypeObject(*PinTypeObject, OutPinType, ConvertError);
	}
}

bool UBlueprintHelperBlueprintStructureUtils::AddMemberVariableDirect(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Payload, FString& OutError)
{
	if (!Blueprint || !Payload.IsValid())
	{
		OutError = TEXT("add_member_variable failed: invalid Blueprint or payload.");
		return false;
	}

	FString VarName;
	if (!Payload->TryGetStringField(TEXT("name"), VarName) || VarName.IsEmpty())
	{
		OutError = TEXT("add_member_variable failed: missing name field.");
		return false;
	}

	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		if (Var.VarName == FName(*VarName))
		{
			return true;
		}
	}

	FEdGraphPinType PinType;
	const TSharedPtr<FJsonObject>* PinTypeObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("pin_type"), PinTypeObject) && PinTypeObject && PinTypeObject->IsValid())
	{
		FString ConvertError;
		if (!TryConvertPinTypeObject(*PinTypeObject, PinType, ConvertError))
		{
			OutError = FString::Printf(TEXT("add_member_variable '%s' failed: %s"), *VarName, *ConvertError);
			return false;
		}
	}
	else
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}

	if (!FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VarName), PinType))
	{
		OutError = FString::Printf(TEXT("add_member_variable '%s' failed: AddMemberVariable returned false."), *VarName);
		return false;
	}

	FString DefaultValue;
	if (Payload->TryGetStringField(TEXT("default_value"), DefaultValue) && !DefaultValue.IsEmpty())
	{
		for (FBPVariableDescription& Var : Blueprint->NewVariables)
		{
			if (Var.VarName == FName(*VarName))
			{
				Var.DefaultValue = DefaultValue;
				break;
			}
		}
	}

	FString Category;
	if (Payload->TryGetStringField(TEXT("category"), Category) && !Category.IsEmpty())
	{
		FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*VarName), nullptr, FText::FromString(Category));
	}

	const TSharedPtr<FJsonObject>* FlagsObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("flags"), FlagsObject) && FlagsObject && FlagsObject->IsValid())
	{
		bool bValue = false;
		if ((*FlagsObject)->TryGetBoolField(TEXT("blueprint_read_only"), bValue) && bValue)
		{
			FBlueprintEditorUtils::SetBlueprintVariableMetaData(Blueprint, FName(*VarName), nullptr, FBlueprintMetadata::MD_Private, TEXT("true"));
		}

		if ((*FlagsObject)->TryGetBoolField(TEXT("expose_on_spawn"), bValue) && bValue)
		{
			FBlueprintEditorUtils::SetBlueprintVariableMetaData(Blueprint, FName(*VarName), nullptr, FBlueprintMetadata::MD_ExposeOnSpawn, TEXT("true"));
		}
	}

	return true;
}

bool UBlueprintHelperBlueprintStructureUtils::RemoveMemberVariableDirect(UBlueprint* Blueprint, const FString& VarName, FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("remove_member_variable failed: invalid Blueprint.");
		return false;
	}

	if (VarName.IsEmpty())
	{
		OutError = TEXT("remove_member_variable failed: missing name field.");
		return false;
	}

	if (FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, FName(*VarName)) == INDEX_NONE)
	{
		return true;
	}

	FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, FName(*VarName));
	return true;
}

bool UBlueprintHelperBlueprintStructureUtils::AddFunctionGraphDirect(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Payload, FString& OutError)
{
	if (!Blueprint || !Payload.IsValid())
	{
		OutError = TEXT("add_function_graph failed: invalid Blueprint or payload.");
		return false;
	}

	FString FuncName;
	if (!Payload->TryGetStringField(TEXT("name"), FuncName) || FuncName.IsEmpty())
	{
		OutError = TEXT("add_function_graph failed: missing name field.");
		return false;
	}

	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetFName() == FName(*FuncName))
		{
			return true;
		}
	}

	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		FName(*FuncName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass());
	if (!NewGraph)
	{
		OutError = FString::Printf(TEXT("add_function_graph '%s' failed: could not create graph."), *FuncName);
		return false;
	}

	FBlueprintEditorUtils::AddFunctionGraph<UFunction>(Blueprint, NewGraph, true, nullptr);

	UK2Node_FunctionEntry* EntryNode = nullptr;
	for (UEdGraphNode* Node : NewGraph->Nodes)
	{
		EntryNode = Cast<UK2Node_FunctionEntry>(Node);
		if (EntryNode)
		{
			break;
		}
	}

	bool bIsPure = false;
	if (Payload->TryGetBoolField(TEXT("is_pure"), bIsPure) && bIsPure && EntryNode)
	{
		EntryNode->SetExtraFlags(EntryNode->GetExtraFlags() | FUNC_BlueprintPure);
	}

	const TArray<TSharedPtr<FJsonValue>>* InputsArray = nullptr;
	if (Payload->TryGetArrayField(TEXT("inputs"), InputsArray) && InputsArray && EntryNode)
	{
		AppendUserPins(EntryNode, InputsArray, EGPD_Output, UEdGraphSchema_K2::PC_Boolean, false);
	}

	const TArray<TSharedPtr<FJsonValue>>* OutputsArray = nullptr;
	if (Payload->TryGetArrayField(TEXT("outputs"), OutputsArray) && OutputsArray && OutputsArray->Num() > 0)
	{
		UK2Node_FunctionResult* ResultNode = nullptr;
		for (UEdGraphNode* Node : NewGraph->Nodes)
		{
			ResultNode = Cast<UK2Node_FunctionResult>(Node);
			if (ResultNode)
			{
				break;
			}
		}

		if (!ResultNode)
		{
			FGraphNodeCreator<UK2Node_FunctionResult> NodeCreator(*NewGraph);
			ResultNode = NodeCreator.CreateNode(true);
			ResultNode->NodePosX = 600;
			ResultNode->NodePosY = 0;
			NodeCreator.Finalize();
		}

		AppendUserPins(ResultNode, OutputsArray, EGPD_Input, UEdGraphSchema_K2::PC_Boolean, false);
	}

	return true;
}

bool UBlueprintHelperBlueprintStructureUtils::AddMacroGraphDirect(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Payload, FString& OutError)
{
	if (!Blueprint || !Payload.IsValid())
	{
		OutError = TEXT("add_macro_graph failed: invalid Blueprint or payload.");
		return false;
	}

	FString MacroName;
	if (!Payload->TryGetStringField(TEXT("name"), MacroName) || MacroName.IsEmpty())
	{
		OutError = TEXT("add_macro_graph failed: missing name field.");
		return false;
	}

	UEdGraph* NewGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (Graph && Graph->GetFName() == FName(*MacroName))
		{
			NewGraph = Graph;
			break;
		}
	}

	if (!NewGraph)
	{
		NewGraph = FBlueprintEditorUtils::CreateNewGraph(
			Blueprint,
			FName(*MacroName),
			UEdGraph::StaticClass(),
			UEdGraphSchema_K2::StaticClass());
		if (!NewGraph)
		{
			OutError = FString::Printf(TEXT("add_macro_graph '%s' failed: could not create graph."), *MacroName);
			return false;
		}

		FBlueprintEditorUtils::AddMacroGraph(Blueprint, NewGraph, true, nullptr);
	}

	if (!BlueprintHelperEnsureMacroTunnelBoundaries(NewGraph, OutError))
	{
		return false;
	}

	UK2Node_Tunnel* InputNode = nullptr;
	UK2Node_Tunnel* OutputNode = nullptr;
	BlueprintHelperFindMacroTunnelBoundaries(NewGraph, InputNode, OutputNode);

	const TArray<TSharedPtr<FJsonValue>>* InputsArray = nullptr;
	if (Payload->TryGetArrayField(TEXT("inputs"), InputsArray) && InputsArray && InputNode)
	{
		AppendUserPins(InputNode, InputsArray, EGPD_Output, UEdGraphSchema_K2::PC_Exec, true);
	}

	const TArray<TSharedPtr<FJsonValue>>* OutputsArray = nullptr;
	if (Payload->TryGetArrayField(TEXT("outputs"), OutputsArray) && OutputsArray && OutputNode)
	{
		AppendUserPins(OutputNode, OutputsArray, EGPD_Input, UEdGraphSchema_K2::PC_Exec, true);
	}

	if (!BlueprintHelperEnsureEmptyMacroGraphDefaultExecLink(NewGraph, OutError))
	{
		return false;
	}

	return true;
}

bool UBlueprintHelperBlueprintStructureUtils::RemoveGraphDirect(UBlueprint* Blueprint, const FString& GraphName, FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("remove_graph failed: invalid Blueprint.");
		return false;
	}

	if (GraphName.IsEmpty())
	{
		OutError = TEXT("remove_graph failed: missing name field.");
		return false;
	}

	if (GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
	{
		OutError = TEXT("remove_graph failed: deleting EventGraph is not allowed.");
		return false;
	}

	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph && Graph->GetFName() == FName(*GraphName))
		{
			FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph);
			return true;
		}
	}

	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetFName() == FName(*GraphName))
		{
			FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph);
			return true;
		}
	}

	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (Graph && Graph->GetFName() == FName(*GraphName))
		{
			FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph);
			return true;
		}
	}

	return true;
}

bool UBlueprintHelperBlueprintStructureUtils::AddEventDispatcherDirect(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Payload, FString& OutError)
{
	if (!Blueprint || !Payload.IsValid())
	{
		OutError = TEXT("add_event_dispatcher failed: invalid Blueprint or payload.");
		return false;
	}

	FString DispatcherName;
	if (!Payload->TryGetStringField(TEXT("name"), DispatcherName) || DispatcherName.IsEmpty())
	{
		OutError = TEXT("add_event_dispatcher failed: missing name field.");
		return false;
	}

	const FName DispatcherFName(*DispatcherName);
	const int32 ExistingVarIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, DispatcherFName);
	UEdGraph* DelegateSignatureGraph = FBlueprintEditorUtils::GetDelegateSignatureGraphByName(Blueprint, DispatcherFName);
	bool bDispatcherExists = false;

	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		if (Var.VarName == DispatcherFName && Var.VarType.PinCategory == UEdGraphSchema_K2::PC_MCDelegate)
		{
			bDispatcherExists = true;
			break;
		}
	}

	if (!bDispatcherExists && ExistingVarIndex != INDEX_NONE)
	{
		OutError = FString::Printf(TEXT("add_event_dispatcher '%s' failed: a non-dispatcher variable with the same name exists."), *DispatcherName);
		return false;
	}

	bool bCreatedVariableThisCall = false;
	if (!bDispatcherExists)
	{
		FEdGraphPinType DelegatePinType;
		DelegatePinType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;

		if (!FBlueprintEditorUtils::AddMemberVariable(Blueprint, DispatcherFName, DelegatePinType))
		{
			OutError = FString::Printf(TEXT("add_event_dispatcher '%s' failed: could not create dispatcher variable."), *DispatcherName);
			return false;
		}

		bCreatedVariableThisCall = true;
	}

	if (!DelegateSignatureGraph)
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		check(K2Schema != nullptr);

		DelegateSignatureGraph = FBlueprintEditorUtils::CreateNewGraph(
			Blueprint,
			DispatcherFName,
			UEdGraph::StaticClass(),
			UEdGraphSchema_K2::StaticClass());
		if (!DelegateSignatureGraph)
		{
			if (bCreatedVariableThisCall)
			{
				FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, DispatcherFName);
			}

			OutError = FString::Printf(TEXT("add_event_dispatcher '%s' failed: could not create delegate signature graph."), *DispatcherName);
			return false;
		}

		DelegateSignatureGraph->bEditable = false;
		K2Schema->CreateDefaultNodesForGraph(*DelegateSignatureGraph);
		K2Schema->CreateFunctionGraphTerminators(*DelegateSignatureGraph, (UClass*)nullptr);
		K2Schema->AddExtraFunctionFlags(DelegateSignatureGraph, (FUNC_BlueprintCallable | FUNC_BlueprintEvent | FUNC_Public));
		K2Schema->MarkFunctionEntryAsEditable(DelegateSignatureGraph, true);

		Blueprint->DelegateSignatureGraphs.Add(DelegateSignatureGraph);
	}

	const TArray<TSharedPtr<FJsonValue>>* ParamsArray = nullptr;
	if (Payload->TryGetArrayField(TEXT("params"), ParamsArray) && ParamsArray)
	{
		for (UEdGraphNode* Node : DelegateSignatureGraph->Nodes)
		{
			UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node);
			if (!EntryNode)
			{
				continue;
			}

			AppendUserPins(EntryNode, ParamsArray, EGPD_Output, UEdGraphSchema_K2::PC_Boolean, true);
			break;
		}
	}

	return true;
}
