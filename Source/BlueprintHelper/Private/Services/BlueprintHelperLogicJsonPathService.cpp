// BlueprintHelper Service Layer — LogicJson 路径定位服务实现

#include "Services/BlueprintHelperLogicJsonPathService.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node.h"

bool FBlueprintHelperLogicJsonPathService::ResolveNode(
	UEdGraph* Graph,
	const FString& NodeRef,
	const FString& NodePath,
	UEdGraphNode*& OutNode,
	FBlueprintHelperPatchResolveError& OutError) const
{
	OutNode = nullptr;

	if (!Graph)
	{
		OutError = {TEXT("target_graph_not_found"), TEXT("图表为空。"), TEXT("graph")};
		return false;
	}

	// 1. 优先 node_path（完整路径）
	if (!NodePath.IsEmpty())
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->GetName() == NodePath)
			{
				OutNode = Node;
				return true;
			}
		}
	}

	// 2. node_ref（节点标题匹配）
	if (!NodeRef.IsEmpty())
	{
		TArray<UEdGraphNode*> Matches;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;
			const FString Title = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
			if (Title.Equals(NodeRef, ESearchCase::IgnoreCase) || Node->GetName().Equals(NodeRef, ESearchCase::IgnoreCase))
			{
				Matches.Add(Node);
			}
		}

		if (Matches.Num() == 1)
		{
			OutNode = Matches[0];
			return true;
		}
		if (Matches.Num() > 1)
		{
			OutError = {TEXT("target_ambiguous"),
				FString::Printf(TEXT("node_ref '%s' 匹配了 %d 个节点。"), *NodeRef, Matches.Num()), NodeRef};
			return false;
		}
	}

	// 3. 搜索所有节点按 GetName()
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node && (Node->GetName() == NodeRef || Node->GetName() == NodePath))
		{
			OutNode = Node;
			return true;
		}
	}

	OutError = {TEXT("target_node_not_found"),
		FString::Printf(TEXT("无法定位节点：node_ref=%s, node_path=%s"), *NodeRef, *NodePath), NodeRef};
	return false;
}

bool FBlueprintHelperLogicJsonPathService::ResolvePin(
	UEdGraph* Graph,
	UEdGraphNode* OwningNode,
	const FString& PinRef,
	const FString& PinPath,
	UEdGraphPin*& OutPin,
	FBlueprintHelperPatchResolveError& OutError) const
{
	OutPin = nullptr;

	if (!OwningNode)
	{
		OutError = {TEXT("target_node_not_found"), TEXT("需先定位 owning node。"), TEXT("node")};
		return false;
	}

	// 搜索 Pin
	for (UEdGraphPin* Pin : OwningNode->Pins)
	{
		if (!Pin) continue;

		if (!PinPath.IsEmpty() && Pin->PinName.ToString() == PinPath)
		{
			OutPin = Pin;
			return true;
		}

		if (!PinRef.IsEmpty() && Pin->PinName.ToString() == PinRef)
		{
			if (!OutPin)
			{
				OutPin = Pin;
			}
			else
			{
				OutError = {TEXT("target_ambiguous"),
					FString::Printf(TEXT("pin_ref '%s' 匹配了多个 Pin。请使用更精确的 pin_path。"), *PinRef), PinRef};
				return false;
			}
		}
	}

	if (OutPin)
	{
		return true;
	}

	OutError = {TEXT("target_pin_not_found"),
		FString::Printf(TEXT("在节点 %s 上找不到 Pin：pin_ref=%s, pin_path=%s"),
			*OwningNode->GetName(), *PinRef, *PinPath), PinRef};
	return false;
}

bool FBlueprintHelperLogicJsonPathService::ResolveLink(
	UEdGraph* Graph,
	const FString& LinkRef,
	const FString& LinkPath,
	FBlueprintHelperResolvedLink& OutLink,
	FBlueprintHelperPatchResolveError& OutError) const
{
	if (!Graph)
	{
		OutError = {TEXT("target_graph_not_found"), TEXT("图表为空。"), TEXT("graph")};
		return false;
	}

	// 解析 link_path: "SourceNode.SourcePin->TargetNode.TargetPin"
	if (!LinkPath.IsEmpty())
	{
		int32 ArrowPos = INDEX_NONE;
		if (LinkPath.FindChar(TEXT('>'), ArrowPos) && ArrowPos > 0 && ArrowPos < LinkPath.Len() - 1)
		{
			FString FromPart = LinkPath.Left(ArrowPos - 1);
			FString ToPart = LinkPath.Mid(ArrowPos + 1);

			FString FromNode, FromPin, ToNode, ToPin;
			int32 FromDot = INDEX_NONE, ToDot = INDEX_NONE;
			if (FromPart.FindLastChar(TEXT('.'), FromDot) && ToPart.FindLastChar(TEXT('.'), ToDot))
			{
				FromNode = FromPart.Left(FromDot);
				FromPin = FromPart.Mid(FromDot + 1);
				ToNode = ToPart.Left(ToDot);
				ToPin = ToPart.Mid(ToDot + 1);

				UEdGraphNode* SourceNode = nullptr;
				UEdGraphNode* TargetNode = nullptr;
				FBlueprintHelperPatchResolveError Ignored;

				if (ResolveNode(Graph, FromNode, FromNode, SourceNode, Ignored) &&
					ResolveNode(Graph, ToNode, ToNode, TargetNode, Ignored))
				{
					UEdGraphPin* FromPinPtr = nullptr;
					UEdGraphPin* ToPinPtr = nullptr;
					if (ResolvePin(Graph, SourceNode, FromPin, FromPin, FromPinPtr, Ignored) &&
						ResolvePin(Graph, TargetNode, ToPin, ToPin, ToPinPtr, Ignored))
					{
						OutLink.SourceNode = SourceNode;
						OutLink.SourcePin = FromPinPtr;
						OutLink.TargetNode = TargetNode;
						OutLink.TargetPin = ToPinPtr;
						return true;
					}
				}
			}
		}
	}

	// link_ref: 扫描所有节点的 outgoing links
	if (!LinkRef.IsEmpty())
	{
		TArray<FBlueprintHelperResolvedLink> Matches;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output) continue;
				for (int32 i = 0; i < Pin->LinkedTo.Num(); ++i)
				{
					UEdGraphPin* LinkedTo = Pin->LinkedTo[i];
					if (!LinkedTo) continue;
					FString Candidate = FString::Printf(TEXT("%s.%s->%s.%s"),
						*Node->GetName(), *Pin->PinName.ToString(),
						*LinkedTo->GetOwningNode()->GetName(), *LinkedTo->PinName.ToString());
					if (Candidate == LinkRef)
					{
						Matches.Add({Node, Pin, LinkedTo->GetOwningNode(), LinkedTo, LinkRef});
					}
				}
			}
		}

		if (Matches.Num() == 1)
		{
			OutLink = Matches[0];
			return true;
		}
		if (Matches.Num() > 1)
		{
			OutError = {TEXT("target_ambiguous"),
				FString::Printf(TEXT("link_ref '%s' 匹配了 %d 条连接。"), *LinkRef, Matches.Num()), LinkRef};
			return false;
		}
	}

	OutError = {TEXT("target_link_not_found"),
		FString::Printf(TEXT("无法定位 link：link_ref=%s"), *LinkRef), LinkRef};
	return false;
}
