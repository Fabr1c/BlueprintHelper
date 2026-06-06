#include "Systems/ToolClusters/GraphWrite/GraphBody/Adapters/BlueprintHelperK2GraphBodyAdapterUtils.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Tunnel.h"

UEdGraph* FBlueprintHelperK2GraphBodyAdapterUtils::FindGraphByName(
	const TArray<UEdGraph*>& Graphs,
	const FString& GraphName)
{
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
		{
			return Graph;
		}
	}
	return nullptr;
}

FString FBlueprintHelperK2GraphBodyAdapterUtils::NodeRef(const UEdGraphNode* Node)
{
	if (!Node)
	{
		return TEXT("Node");
	}
	if (IsFunctionEntry(Node))
	{
		return TEXT("FunctionEntry");
	}
	if (IsFunctionResult(Node))
	{
		return TEXT("FunctionResult");
	}
	if (const UK2Node_Tunnel* Tunnel = Cast<UK2Node_Tunnel>(Node))
	{
		if (IsTunnelEntry(Tunnel))
		{
			return TEXT("TunnelEntry");
		}
		if (IsTunnelExit(Tunnel))
		{
			return TEXT("TunnelExit");
		}
	}
	if (const UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))
	{
		return FString::Printf(TEXT("CustomEvent:%s"), *CustomEvent->CustomFunctionName.ToString());
	}
	if (const UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
	{
		const FString EventName = EventNode->GetNodeTitle(ENodeTitleType::ListView).ToString().TrimStartAndEnd();
		return EventName.IsEmpty() ? TEXT("Event") : FString::Printf(TEXT("Event:%s"), *EventName);
	}
	if (Node->NodeGuid.IsValid())
	{
		return Node->NodeGuid.ToString(EGuidFormats::Digits);
	}
	return Node->GetName();
}

bool FBlueprintHelperK2GraphBodyAdapterUtils::IsFunctionEntry(const UEdGraphNode* Node)
{
	return Node && Node->IsA<UK2Node_FunctionEntry>();
}

bool FBlueprintHelperK2GraphBodyAdapterUtils::IsFunctionResult(const UEdGraphNode* Node)
{
	return Node && Node->IsA<UK2Node_FunctionResult>();
}

bool FBlueprintHelperK2GraphBodyAdapterUtils::IsTunnelEntry(const UK2Node_Tunnel* Tunnel)
{
	return Tunnel && (Tunnel->bCanHaveOutputs || HasExecPin(Tunnel, EGPD_Output));
}

bool FBlueprintHelperK2GraphBodyAdapterUtils::IsTunnelExit(const UK2Node_Tunnel* Tunnel)
{
	return Tunnel && (Tunnel->bCanHaveInputs || HasExecPin(Tunnel, EGPD_Input));
}

bool FBlueprintHelperK2GraphBodyAdapterUtils::IsCustomEventEntry(
	const UEdGraphNode* Node,
	const FString& EntryName)
{
	const UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node);
	if (!CustomEvent)
	{
		return false;
	}
	return EntryName.IsEmpty() || CustomEvent->CustomFunctionName.ToString().Equals(EntryName, ESearchCase::IgnoreCase);
}

bool FBlueprintHelperK2GraphBodyAdapterUtils::IsEventEntry(
	const UEdGraphNode* Node,
	const FString& EntryName)
{
	const UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
	if (!EventNode)
	{
		return false;
	}
	if (EntryName.IsEmpty())
	{
		return true;
	}
	return EventNode->GetNodeTitle(ENodeTitleType::ListView).ToString().Contains(EntryName);
}

bool FBlueprintHelperK2GraphBodyAdapterUtils::HasExecPin(
	const UEdGraphNode* Node,
	EEdGraphPinDirection Direction)
{
	if (!Node)
	{
		return false;
	}
	for (const UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->Direction == Direction && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			return true;
		}
	}
	return false;
}

void FBlueprintHelperK2GraphBodyAdapterUtils::AppendPinSemanticSources(
	const UEdGraphNode* Node,
	const FString& NodeRef,
	TArray<FString>& OutSemanticSourceRefs)
{
	if (!Node)
	{
		return;
	}

	bool bAddedExecSource = false;
	for (const UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin)
		{
			continue;
		}

		const FString PinName = Pin->PinName.ToString();
		if (!PinName.IsEmpty())
		{
			OutSemanticSourceRefs.AddUnique(FString::Printf(TEXT("%s.%s"), *NodeRef, *PinName));
		}
		if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			bAddedExecSource = true;
		}
	}

	if (!bAddedExecSource)
	{
		if (NodeRef == TEXT("TunnelEntry"))
		{
			OutSemanticSourceRefs.AddUnique(TEXT("TunnelEntry.Execute"));
		}
		else if (NodeRef == TEXT("TunnelExit"))
		{
			OutSemanticSourceRefs.AddUnique(TEXT("TunnelExit.Then"));
		}
		else if (NodeRef == TEXT("FunctionEntry"))
		{
			OutSemanticSourceRefs.AddUnique(TEXT("FunctionEntry.Execute"));
		}
		else if (NodeRef == TEXT("FunctionResult"))
		{
			OutSemanticSourceRefs.AddUnique(TEXT("FunctionResult.Execute"));
		}
	}
}
