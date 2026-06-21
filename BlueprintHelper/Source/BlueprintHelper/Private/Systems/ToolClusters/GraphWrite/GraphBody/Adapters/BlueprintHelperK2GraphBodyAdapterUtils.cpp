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
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperK2GraphEntryIdentityResolver.h"
#include "UObject/Package.h"

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
	const FBlueprintHelperK2GraphEntryIdentityResolver Resolver;
	FBlueprintHelperK2GraphEntryIdentity Identity;
	if (Resolver.TryResolveNodeIdentity(Node, Identity))
	{
		switch (Identity.Kind)
		{
		case EBlueprintHelperK2GraphEntryKind::FunctionEntry:
			return TEXT("FunctionEntry");
		case EBlueprintHelperK2GraphEntryKind::FunctionResult:
			return TEXT("FunctionResult");
		case EBlueprintHelperK2GraphEntryKind::MacroEntry:
			return TEXT("TunnelEntry");
		case EBlueprintHelperK2GraphEntryKind::MacroExit:
			return TEXT("TunnelExit");
		case EBlueprintHelperK2GraphEntryKind::CustomEvent:
			return FString::Printf(TEXT("CustomEvent:%s"), *Identity.StableName);
		case EBlueprintHelperK2GraphEntryKind::Event:
			return FString::Printf(TEXT("Event:%s"), *Identity.StableName);
		default:
			break;
		}
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

FString FBlueprintHelperK2GraphBodyAdapterUtils::PinRef(const UEdGraphPin* Pin)
{
	if (!Pin)
	{
		return TEXT("Pin");
	}
	return FString::Printf(
		TEXT("%s.%s"),
		*NodeRef(Pin->GetOwningNode()),
		*Pin->PinName.ToString());
}

void FBlueprintHelperK2GraphBodyAdapterUtils::AppendGraphLinkRefs(
	const UEdGraph* Graph,
	TArray<FString>& OutExecLinkRefs,
	TArray<FString>& OutDataLinkRefs)
{
	if (!Graph)
	{
		return;
	}

	for (const UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node)
		{
			continue;
		}
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output)
			{
				continue;
			}
			const FString SourceRef = PinRef(Pin);
			for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				if (!LinkedPin)
				{
					continue;
				}
				const FString LinkRef = FString::Printf(TEXT("%s->%s"), *SourceRef, *PinRef(LinkedPin));
				if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec
					|| LinkedPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
				{
					OutExecLinkRefs.AddUnique(LinkRef);
				}
				else
				{
					OutDataLinkRefs.AddUnique(LinkRef);
				}
			}
		}
	}
}

bool FBlueprintHelperK2GraphBodyAdapterUtils::IsFunctionEntry(const UEdGraphNode* Node)
{
	const FBlueprintHelperK2GraphEntryIdentityResolver Resolver;
	FBlueprintHelperK2GraphEntryIdentity Identity;
	return Resolver.TryResolveNodeIdentity(Node, Identity)
		&& Identity.Kind == EBlueprintHelperK2GraphEntryKind::FunctionEntry;
}

bool FBlueprintHelperK2GraphBodyAdapterUtils::IsFunctionResult(const UEdGraphNode* Node)
{
	const FBlueprintHelperK2GraphEntryIdentityResolver Resolver;
	FBlueprintHelperK2GraphEntryIdentity Identity;
	return Resolver.TryResolveNodeIdentity(Node, Identity)
		&& Identity.Kind == EBlueprintHelperK2GraphEntryKind::FunctionResult;
}

bool FBlueprintHelperK2GraphBodyAdapterUtils::IsFunctionEntryNodeClass(const UClass* NodeClass)
{
	return NodeClass && NodeClass->IsChildOf(UK2Node_FunctionEntry::StaticClass());
}

bool FBlueprintHelperK2GraphBodyAdapterUtils::IsFunctionResultNodeClass(const UClass* NodeClass)
{
	return NodeClass && NodeClass->IsChildOf(UK2Node_FunctionResult::StaticClass());
}

bool FBlueprintHelperK2GraphBodyAdapterUtils::IsProtectedFunctionBoundaryNode(const UEdGraphNode* Node)
{
	return IsFunctionEntry(Node) || IsFunctionResult(Node);
}

bool FBlueprintHelperK2GraphBodyAdapterUtils::IsTunnelEntry(const UK2Node_Tunnel* Tunnel)
{
	const FBlueprintHelperK2GraphEntryIdentityResolver Resolver;
	FBlueprintHelperK2GraphEntryIdentity Identity;
	return Resolver.TryResolveNodeIdentity(Tunnel, Identity)
		&& (Identity.Kind == EBlueprintHelperK2GraphEntryKind::MacroEntry
			|| Identity.Role == EBlueprintHelperK2GraphBoundaryRole::ExecBoundary);
}

bool FBlueprintHelperK2GraphBodyAdapterUtils::IsTunnelExit(const UK2Node_Tunnel* Tunnel)
{
	const FBlueprintHelperK2GraphEntryIdentityResolver Resolver;
	FBlueprintHelperK2GraphEntryIdentity Identity;
	return Resolver.TryResolveNodeIdentity(Tunnel, Identity)
		&& (Identity.Kind == EBlueprintHelperK2GraphEntryKind::MacroExit
			|| Identity.Role == EBlueprintHelperK2GraphBoundaryRole::ExecBoundary);
}

bool FBlueprintHelperK2GraphBodyAdapterUtils::IsCustomEventEntry(
	const UEdGraphNode* Node,
	const FString& EntryName)
{
	const FBlueprintHelperK2GraphEntryIdentityResolver Resolver;
	FBlueprintHelperK2GraphEntryIdentity Identity;
	FBlueprintHelperK2GraphEntryQuery Query;
	Query.TargetType = TEXT("custom_event");
	Query.TargetName = EntryName;
	Query.RequiredRole = EBlueprintHelperK2GraphBoundaryRole::BodyEntry;
	return Resolver.TryResolveNodeIdentity(Node, Identity)
		&& Resolver.DoesIdentityMatchQuery(Identity, Query);
}

bool FBlueprintHelperK2GraphBodyAdapterUtils::IsEventEntry(
	const UEdGraphNode* Node,
	const FString& EntryName)
{
	const FBlueprintHelperK2GraphEntryIdentityResolver Resolver;
	FBlueprintHelperK2GraphEntryIdentity Identity;
	FBlueprintHelperK2GraphEntryQuery Query;
	Query.TargetType = TEXT("event");
	Query.TargetName = EntryName;
	Query.RequiredRole = EBlueprintHelperK2GraphBoundaryRole::BodyEntry;
	return Resolver.TryResolveNodeIdentity(Node, Identity)
		&& Resolver.DoesIdentityMatchQuery(Identity, Query);
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

bool FBlueprintHelperK2GraphBodyAdapterUtils::TryReadBlueprintHelperBlockId(
	const UEdGraphNode* Node,
	FString& OutBlockId)
{
	OutBlockId.Reset();
	if (!Node)
	{
		return false;
	}

	UPackage* Package = Node->GetOutermost();
	if (!Package)
	{
		return false;
	}

	FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
	if (MetaData.GetValue(Node, TEXT("BlueprintHelperOwned")) != TEXT("true"))
	{
		return false;
	}

	OutBlockId = MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId"));
	return !OutBlockId.IsEmpty();
}

void FBlueprintHelperK2GraphBodyAdapterUtils::AppendOwnedBodyNodesForBlock(
	UEdGraph* Graph,
	const FString& BlockId,
	const UEdGraphNode* EntryNode,
	TArray<UEdGraphNode*>& OutNodes)
{
	if (!Graph || BlockId.IsEmpty())
	{
		return;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node || Node == EntryNode)
		{
			continue;
		}

		FString NodeBlockId;
		if (TryReadBlueprintHelperBlockId(Node, NodeBlockId) &&
			NodeBlockId.Equals(BlockId, ESearchCase::IgnoreCase))
		{
			OutNodes.AddUnique(Node);
		}
	}
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

void FBlueprintHelperK2GraphBodyAdapterUtils::AppendPinSemanticOutputs(
	const UEdGraphNode* Node,
	const FString& NodeRef,
	TArray<FString>& OutSemanticOutputRefs,
	TArray<FString>& OutReturnDataPinRefs)
{
	if (!Node)
	{
		return;
	}

	bool bAddedExecOutput = false;
	for (const UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin || Pin->Direction != EGPD_Input)
		{
			continue;
		}

		const FString PinName = Pin->PinName.ToString();
		if (PinName.IsEmpty())
		{
			continue;
		}

		const FString PinRef = FString::Printf(TEXT("%s.%s"), *NodeRef, *PinName);
		OutSemanticOutputRefs.AddUnique(PinRef);
		if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			bAddedExecOutput = true;
		}
		else
		{
			OutReturnDataPinRefs.AddUnique(PinRef);
		}
	}

	if (!bAddedExecOutput && NodeRef == TEXT("FunctionResult"))
	{
		OutSemanticOutputRefs.AddUnique(TEXT("FunctionResult.Execute"));
	}
}
