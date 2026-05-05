// BlueprintHelper Service Layer — Logic Group Builder 实现

#include "Logic/BlueprintHelperLogicGroupBuilder.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

FBlueprintHelperLogicGroupBuilder::FBlueprintHelperLogicGroupBuilder() = default;

bool FBlueprintHelperLogicGroupBuilder::IsMultiEntryScope(EBlueprintHelperLogicScope Scope)
{
	switch (Scope)
	{
	case EBlueprintHelperLogicScope::TargetGraph:
	case EBlueprintHelperLogicScope::Blueprint:
	case EBlueprintHelperLogicScope::MultiTarget:
		return true;
	default:
		return false;
	}
}

FBlueprintHelperLogicJsonPayload FBlueprintHelperLogicGroupBuilder::BuildGroups(
	const TSharedPtr<FJsonObject>& RawJson,
	const FString& AssetPath,
	const FString& GraphName,
	EBlueprintHelperLogicScope Scope) const
{
	FBlueprintHelperLogicJsonPayload Payload;
	Payload.AssetPath = AssetPath;
	Payload.Graph = GraphName;

	if (!RawJson.IsValid())
	{
		return Payload;
	}

	// 提取 nodes 数组
	const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
	if (!RawJson->TryGetArrayField(TEXT("nodes"), NodesArray) || !NodesArray)
	{
		return Payload;
	}

	const int32 NodeCount = NodesArray->Num();
	if (NodeCount == 0)
	{
		return Payload;
	}

	if (IsMultiEntryScope(Scope))
	{
		// ─── 多入口 scope：单分组（简化实现）───
		// 将整个 graph 作为一个 group
		FBlueprintHelperLogicGroup Group;
		Group.GroupType = EBlueprintHelperLogicGroupType::GlobalEventFlow;
		Group.Name = GraphName;

		// 找第一个入口节点
		bool bFoundEntry = false;
		for (int32 i = 0; i < NodeCount; ++i)
		{
			const TSharedPtr<FJsonValue>& NodeVal = (*NodesArray)[i];
			const TSharedPtr<FJsonObject>* NodeObjPtr = nullptr;
			if (!NodeVal.IsValid() || !NodeVal->TryGetObject(NodeObjPtr) || !NodeObjPtr) continue;

			const FString NodeRef = FString::Printf(TEXT("nodes[%d]"), i);
			const FString Name = ExtractNodeName(*NodeObjPtr);
			const EBlueprintHelperLogicNodeKind Kind = IdentifyNodeKind(*NodeObjPtr);

			FBlueprintHelperLogicNode Node = ConvertNode(*NodeObjPtr, i);

			if (!bFoundEntry && IsEntryNode(*NodeObjPtr))
			{
				Group.Entry.Kind = Kind;
				Group.Entry.Name = Name;
				Group.Entry.NodeRef = NodeRef;
				Group.Entry.NodePath = FString::Printf(TEXT("$.graphs[%s].%s"), *GraphName, *NodeRef);
				bFoundEntry = true;
			}

			Group.Nodes.Add(MoveTemp(Node));
		}

		// 如果没找到明确入口，使用第一个节点
		if (!bFoundEntry && Group.Nodes.Num() > 0)
		{
			Group.Entry.Kind = Group.Nodes[0].Kind;
			Group.Entry.Name = Group.Nodes[0].Name;
			Group.Entry.NodeRef = Group.Nodes[0].NodeRef;
			Group.Entry.NodePath = FString::Printf(TEXT("$.graphs[%s].%s"), *GraphName, *Group.Nodes[0].NodeRef);
		}

		Payload.Groups.Add(MoveTemp(Group));
	}
	else
	{
		// ─── 单入口 scope ───
		bool bFoundEntry = false;
		for (int32 i = 0; i < NodeCount; ++i)
		{
			const TSharedPtr<FJsonValue>& NodeVal = (*NodesArray)[i];
			const TSharedPtr<FJsonObject>* NodeObjPtr = nullptr;
			if (!NodeVal.IsValid() || !NodeVal->TryGetObject(NodeObjPtr) || !NodeObjPtr) continue;

			const FString NodeRef = FString::Printf(TEXT("nodes[%d]"), i);
			const FString Name = ExtractNodeName(*NodeObjPtr);
			const EBlueprintHelperLogicNodeKind Kind = IdentifyNodeKind(*NodeObjPtr);

			FBlueprintHelperLogicNode Node = ConvertNode(*NodeObjPtr, i);

			if (!bFoundEntry && IsEntryNode(*NodeObjPtr))
			{
				FBlueprintHelperLogicEntry Entry;
				Entry.Kind = Kind;
				Entry.Name = Name;
				Entry.NodeRef = NodeRef;
				Entry.NodePath = FString::Printf(TEXT("$.graphs[%s].%s"), *GraphName, *NodeRef);
				Payload.Entry = Entry;
				bFoundEntry = true;
			}

			Payload.Nodes.Add(MoveTemp(Node));
		}

		if (!bFoundEntry && Payload.Nodes.Num() > 0)
		{
			FBlueprintHelperLogicEntry Entry;
			Entry.Kind = Payload.Nodes[0].Kind;
			Entry.Name = Payload.Nodes[0].Name;
			Entry.NodeRef = Payload.Nodes[0].NodeRef;
			Entry.NodePath = FString::Printf(TEXT("$.graphs[%s].%s"), *GraphName, *Payload.Nodes[0].NodeRef);
			Payload.Entry = Entry;
		}
	}

	return Payload;
}

EBlueprintHelperLogicNodeKind FBlueprintHelperLogicGroupBuilder::IdentifyNodeKind(const TSharedPtr<FJsonObject>& NodeObj)
{
	if (!NodeObj.IsValid()) return EBlueprintHelperLogicNodeKind::Unknown;

	FString ClassName;
	NodeObj->TryGetStringField(TEXT("class"), ClassName);
	FString MemberName;
	NodeObj->TryGetStringField(TEXT("member_name"), MemberName);

	// 根据 class 名或 member_name 推断
	if (ClassName.Contains(TEXT("K2Node_CustomEvent")) || MemberName.Contains(TEXT("CustomEvent")))
		return EBlueprintHelperLogicNodeKind::CustomEvent;
	if (ClassName.Contains(TEXT("K2Node_Event")))
		return EBlueprintHelperLogicNodeKind::Event;
	if (ClassName.Contains(TEXT("K2Node_CallFunction")))
		return EBlueprintHelperLogicNodeKind::CallFunction;
	if (ClassName.Contains(TEXT("K2Node_IfThenElse")))
		return EBlueprintHelperLogicNodeKind::Branch;
	if (ClassName.Contains(TEXT("K2Node_ExecutionSequence")))
		return EBlueprintHelperLogicNodeKind::Sequence;
	if (ClassName.Contains(TEXT("K2Node_VariableGet")))
		return EBlueprintHelperLogicNodeKind::VariableGet;
	if (ClassName.Contains(TEXT("K2Node_VariableSet")))
		return EBlueprintHelperLogicNodeKind::VariableSet;
	if (ClassName.Contains(TEXT("K2Node_MacroInstance")))
		return EBlueprintHelperLogicNodeKind::Macro;
	if (ClassName.Contains(TEXT("K2Node_CreateDelegate")))
		return EBlueprintHelperLogicNodeKind::DelegateBind;
	if (ClassName.Contains(TEXT("Timeline")))
		return EBlueprintHelperLogicNodeKind::Timeline;

	return EBlueprintHelperLogicNodeKind::Unknown;
}

FBlueprintHelperLogicNode FBlueprintHelperLogicGroupBuilder::ConvertNode(const TSharedPtr<FJsonObject>& NodeObj, int32 Index)
{
	FBlueprintHelperLogicNode Node;
	Node.NodeRef = FString::Printf(TEXT("nodes[%d]"), Index);
	Node.Kind = IdentifyNodeKind(NodeObj);
	Node.Name = ExtractNodeName(NodeObj);
	Node.Owner = ExtractOwner(NodeObj);

	// 提取 outgoing links
	const TArray<TSharedPtr<FJsonValue>>* LinksArray = nullptr;
	if (NodeObj->TryGetArrayField(TEXT("out_links"), LinksArray) && LinksArray)
	{
		for (int32 j = 0; j < LinksArray->Num(); ++j)
		{
			const TSharedPtr<FJsonValue>& LinkVal = (*LinksArray)[j];
			const TSharedPtr<FJsonObject>* LinkObjPtr = nullptr;
			if (!LinkVal.IsValid() || !LinkVal->TryGetObject(LinkObjPtr) || !LinkObjPtr) continue;

			FBlueprintHelperLogicLink Link;
			Link.LinkRef = FString::Printf(TEXT("links[%d]"), j);

			FString LinkTypeStr;
			(*LinkObjPtr)->TryGetStringField(TEXT("type"), LinkTypeStr);
			Link.Type = LinkTypeStr.Equals(TEXT("data"), ESearchCase::IgnoreCase)
				? EBlueprintHelperLogicLinkType::Data : EBlueprintHelperLogicLinkType::Exec;

			(*LinkObjPtr)->TryGetStringField(TEXT("from_pin"), Link.FromPin);
			(*LinkObjPtr)->TryGetStringField(TEXT("to_node"), Link.ToNode);
			(*LinkObjPtr)->TryGetStringField(TEXT("to_pin"), Link.ToPin);

			Node.Links.Add(MoveTemp(Link));
		}
	}

	return Node;
}

FString FBlueprintHelperLogicGroupBuilder::ExtractNodeName(const TSharedPtr<FJsonObject>& NodeObj)
{
	if (!NodeObj.IsValid()) return TEXT("Unknown");

	FString Name;
	if (NodeObj->TryGetStringField(TEXT("name"), Name) && !Name.IsEmpty()) return Name;
	if (NodeObj->TryGetStringField(TEXT("member_name"), Name) && !Name.IsEmpty()) return Name;
	NodeObj->TryGetStringField(TEXT("class"), Name);
	return Name.IsEmpty() ? TEXT("Unknown") : Name;
}

FString FBlueprintHelperLogicGroupBuilder::ExtractOwner(const TSharedPtr<FJsonObject>& NodeObj)
{
	if (!NodeObj.IsValid()) return TEXT("");

	FString Owner;
	NodeObj->TryGetStringField(TEXT("owner"), Owner);
	return Owner;
}

bool FBlueprintHelperLogicGroupBuilder::IsEntryNode(const TSharedPtr<FJsonObject>& NodeObj)
{
	if (!NodeObj.IsValid()) return false;

	const EBlueprintHelperLogicNodeKind Kind = IdentifyNodeKind(NodeObj);
	switch (Kind)
	{
	case EBlueprintHelperLogicNodeKind::Event:
	case EBlueprintHelperLogicNodeKind::CustomEvent:
		return true;
	default:
		return false;
	}
}
