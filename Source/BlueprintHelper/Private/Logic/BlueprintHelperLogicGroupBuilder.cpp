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

FBlueprintHelperLogicJsonPayload FBlueprintHelperLogicGroupBuilder::BuildTargetEntry(
	const TSharedPtr<FJsonObject>& RawJson,
	const FString& AssetPath,
	const FString& GraphName,
	const FString& TargetName,
	EBlueprintHelperLogicScope Scope) const
{
	FBlueprintHelperLogicJsonPayload Payload;
	Payload.AssetPath = AssetPath;
	Payload.Graph = GraphName;
	if (Scope == EBlueprintHelperLogicScope::TargetEvent ||
		Scope == EBlueprintHelperLogicScope::TargetCustomEvent)
	{
		Payload.Event = TargetName;
	}

	if (!RawJson.IsValid())
	{
		return Payload;
	}

	auto ExtractGraphName = [](const TSharedPtr<FJsonObject>& GraphObj, int32 GraphIndex) -> FString
	{
		if (!GraphObj.IsValid())
		{
			return GraphIndex == 0 ? TEXT("Graph") : FString::Printf(TEXT("Graph_%d"), GraphIndex + 1);
		}

		FString Name;
		if (GraphObj->TryGetStringField(TEXT("graph"), Name) && !Name.IsEmpty())
		{
			return Name;
		}
		if (GraphObj->TryGetStringField(TEXT("name"), Name) && !Name.IsEmpty())
		{
			return Name;
		}
		if (GraphObj->TryGetStringField(TEXT("graph_name"), Name) && !Name.IsEmpty())
		{
			return Name;
		}
		return GraphIndex == 0 ? TEXT("Graph") : FString::Printf(TEXT("Graph_%d"), GraphIndex + 1);
	};

	auto ExtractEventName = [](const TSharedPtr<FJsonObject>& NodeObj) -> FString
	{
		if (!NodeObj.IsValid())
		{
			return TEXT("");
		}

		const TSharedPtr<FJsonObject>* EventObj = nullptr;
		FString EventName;
		if (NodeObj->TryGetObjectField(TEXT("event"), EventObj) && EventObj && EventObj->IsValid())
		{
			if ((*EventObj)->TryGetStringField(TEXT("event_name"), EventName) && !EventName.IsEmpty())
			{
				return EventName;
			}
		}
		if (NodeObj->TryGetStringField(TEXT("event_name"), EventName) && !EventName.IsEmpty())
		{
			return EventName;
		}
		return TEXT("");
	};

	auto MatchesScope = [Scope](EBlueprintHelperLogicNodeKind Kind) -> bool
	{
		switch (Scope)
		{
		case EBlueprintHelperLogicScope::TargetCustomEvent:
			return Kind == EBlueprintHelperLogicNodeKind::CustomEvent;
		case EBlueprintHelperLogicScope::TargetEvent:
			return Kind == EBlueprintHelperLogicNodeKind::Event || Kind == EBlueprintHelperLogicNodeKind::CustomEvent;
		default:
			return true;
		}
	};

	auto MatchesTargetName = [&TargetName, &ExtractEventName](const TSharedPtr<FJsonObject>& NodeObj) -> bool
	{
		if (TargetName.IsEmpty())
		{
			return true;
		}

		const FString NodeName = ExtractNodeName(NodeObj);
		if (NodeName.Equals(TargetName, ESearchCase::IgnoreCase))
		{
			return true;
		}

		const FString EventName = ExtractEventName(NodeObj);
		return EventName.Equals(TargetName, ESearchCase::IgnoreCase);
	};

	auto TryBuildFromGraph = [this, &Payload, &MatchesScope, &MatchesTargetName](
		const TSharedPtr<FJsonObject>& GraphObj,
		const FString& EffectiveGraphName) -> bool
	{
		const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
		if (!GraphObj.IsValid() || !GraphObj->TryGetArrayField(TEXT("nodes"), NodesArray) || !NodesArray)
		{
			return false;
		}

		int32 EntryIndex = INDEX_NONE;
		for (int32 i = 0; i < NodesArray->Num(); ++i)
		{
			const TSharedPtr<FJsonValue>& NodeVal = (*NodesArray)[i];
			const TSharedPtr<FJsonObject>* NodeObjPtr = nullptr;
			if (!NodeVal.IsValid() || !NodeVal->TryGetObject(NodeObjPtr) || !NodeObjPtr) continue;

			const EBlueprintHelperLogicNodeKind Kind = IdentifyNodeKind(*NodeObjPtr);
			if (IsEntryNode(*NodeObjPtr) && MatchesScope(Kind) && MatchesTargetName(*NodeObjPtr))
			{
				EntryIndex = i;
				break;
			}
		}

		if (EntryIndex == INDEX_NONE)
		{
			return false;
		}

		Payload.Graph = EffectiveGraphName;
		Payload.Entry.Reset();
		Payload.Nodes.Reset();
		Payload.Groups.Reset();

		for (int32 i = 0; i < NodesArray->Num(); ++i)
		{
			const TSharedPtr<FJsonValue>& NodeVal = (*NodesArray)[i];
			const TSharedPtr<FJsonObject>* NodeObjPtr = nullptr;
			if (!NodeVal.IsValid() || !NodeVal->TryGetObject(NodeObjPtr) || !NodeObjPtr) continue;

			if (i == EntryIndex)
			{
				FBlueprintHelperLogicEntry Entry;
				Entry.Kind = IdentifyNodeKind(*NodeObjPtr);
				Entry.Name = ExtractNodeName(*NodeObjPtr);
				Entry.NodeRef = FString::Printf(TEXT("nodes[%d]"), i);
				Entry.NodePath = FString::Printf(TEXT("$.graphs[%s].%s"), *EffectiveGraphName, *Entry.NodeRef);
				Payload.Entry = Entry;
			}

			Payload.Nodes.Add(ConvertNode(*NodeObjPtr, i));
		}

		return true;
	};

	const TArray<TSharedPtr<FJsonValue>>* GraphsArray = nullptr;
	if (RawJson->TryGetArrayField(TEXT("graphs"), GraphsArray) && GraphsArray)
	{
		for (int32 GraphIndex = 0; GraphIndex < GraphsArray->Num(); ++GraphIndex)
		{
			const TSharedPtr<FJsonObject>* GraphObjPtr = nullptr;
			const TSharedPtr<FJsonValue>& GraphVal = (*GraphsArray)[GraphIndex];
			if (!GraphVal.IsValid() || !GraphVal->TryGetObject(GraphObjPtr) || !GraphObjPtr) continue;

			const FString EffectiveGraphName = ExtractGraphName(*GraphObjPtr, GraphIndex);
			if (!GraphName.IsEmpty() && !EffectiveGraphName.Equals(GraphName, ESearchCase::IgnoreCase))
			{
				continue;
			}

			if (TryBuildFromGraph(*GraphObjPtr, EffectiveGraphName))
			{
				return Payload;
			}
		}

		return Payload;
	}

	const FString EffectiveGraphName = GraphName.IsEmpty() ? TEXT("EventGraph") : GraphName;
	TryBuildFromGraph(RawJson, EffectiveGraphName);
	return Payload;
}

EBlueprintHelperLogicNodeKind FBlueprintHelperLogicGroupBuilder::IdentifyNodeKind(const TSharedPtr<FJsonObject>& NodeObj)
{
	if (!NodeObj.IsValid()) return EBlueprintHelperLogicNodeKind::Unknown;

	FString ClassName;
	NodeObj->TryGetStringField(TEXT("class"), ClassName);
	if (ClassName.IsEmpty())
	{
		NodeObj->TryGetStringField(TEXT("type"), ClassName);
	}
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
	const TSharedPtr<FJsonObject>* EventObj = nullptr;
	if (NodeObj->TryGetObjectField(TEXT("event"), EventObj) && EventObj && EventObj->IsValid())
	{
		if ((*EventObj)->TryGetStringField(TEXT("event_name"), Name) && !Name.IsEmpty()) return Name;
	}
	NodeObj->TryGetStringField(TEXT("class"), Name);
	if (Name.IsEmpty())
	{
		NodeObj->TryGetStringField(TEXT("type"), Name);
	}
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
