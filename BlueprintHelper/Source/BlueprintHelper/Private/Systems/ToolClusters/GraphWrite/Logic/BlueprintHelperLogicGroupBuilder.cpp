// BlueprintHelper Service Layer — Logic Group Builder 实现

#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicGroupBuilder.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Systems/ToolClusters/GraphWrite/Logic/Utils/BlueprintHelperGraphWriteClassificationUtils.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

class FBlueprintHelperLogicGroupBuilderLocalUtils
{
public:
	static FString JsonValueToString(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid())
		{
			return TEXT("");
		}

		switch (Value->Type)
		{
		case EJson::String:
			return Value->AsString();
		case EJson::Number:
			return LexToString(Value->AsNumber());
		case EJson::Boolean:
			return Value->AsBool() ? TEXT("true") : TEXT("false");
		default:
			return TEXT("");
		}
	}

	static bool TryReadStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, FString& OutValue)
	{
		if (!Object.IsValid())
		{
			return false;
		}

		const TSharedPtr<FJsonValue> Value = Object->TryGetField(FieldName);
		if (!Value.IsValid() || Value->Type == EJson::Null)
		{
			return false;
		}

		OutValue = JsonValueToString(Value).TrimStartAndEnd();
		return !OutValue.IsEmpty();
	}

	static FString ReadFirstStringField(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* First,
		const TCHAR* Second = nullptr,
		const TCHAR* Third = nullptr)
	{
		FString Value;
		if (First && TryReadStringField(Object, First, Value))
		{
			return Value;
		}
		if (Second && TryReadStringField(Object, Second, Value))
		{
			return Value;
		}
		if (Third && TryReadStringField(Object, Third, Value))
		{
			return Value;
		}
		return TEXT("");
	}

	static FString SanitizeDisplayText(const FString& InValue)
	{
		FString Result;
		Result.Reserve(InValue.Len());
		for (const TCHAR Char : InValue)
		{
			if (Char == TEXT('\t') || Char == TEXT('\n') || Char == TEXT('\r') || Char >= 0x20)
			{
				Result.AppendChar(Char);
			}
		}
		Result.TrimStartAndEndInline();
		if (Result.Len() > 256)
		{
			FBlueprintHelperVersionCompat::LeftInlineNoShrink(Result, 256);
		}
		return Result;
	}

	static bool TryReadNestedStringField(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* ObjectFieldName,
		const TCHAR* FieldName,
		FString& OutValue)
	{
		const TSharedPtr<FJsonObject>* NestedObject = nullptr;
		if (Object.IsValid()
			&& Object->TryGetObjectField(ObjectFieldName, NestedObject)
			&& NestedObject
			&& NestedObject->IsValid())
		{
			return TryReadStringField(*NestedObject, FieldName, OutValue);
		}

		return false;
	}

	static FString MakeNodeRef(int32 NodeIndex)
	{
		return FString::Printf(TEXT("nodes[%d]"), NodeIndex);
	}

	static FString MakeLinkRef(int32 LinkIndex)
	{
		return FString::Printf(TEXT("links[%d]"), LinkIndex);
	}

	static FString MakeEndpointLinkRef(
		const FString& FromNodeRef,
		const FString& FromPinRef,
		const FString& ToNodeRef,
		const FString& ToPinRef)
	{
		return FString::Printf(
			TEXT("%s.%s->%s.%s"),
			*FromNodeRef,
			*FromPinRef,
			*ToNodeRef,
			*ToPinRef);
	}

	static FString MakeGraphNodePath(const FString& GraphName, const FString& NodeRef)
	{
		return FString::Printf(TEXT("$.graphs[%s].%s"), *GraphName, *NodeRef);
	}

	static const TCHAR* SyntheticFunctionEntryNodeRef()
	{
		return TEXT("__function_entry__");
	}

	static const TCHAR* SyntheticFunctionResultNodeRef()
	{
		return TEXT("__function_result__");
	}

	static bool IsSyntheticFunctionBoundaryNodeRef(const FString& NodeRef)
	{
		return NodeRef.Equals(SyntheticFunctionEntryNodeRef(), ESearchCase::IgnoreCase)
			|| NodeRef.Equals(SyntheticFunctionResultNodeRef(), ESearchCase::IgnoreCase);
	}

	static bool DoesGraphNameMatchTargetFunction(
		const FString& EffectiveGraphName,
		const FString& TargetName)
	{
		return !EffectiveGraphName.IsEmpty()
			&& !TargetName.IsEmpty()
			&& EffectiveGraphName.Equals(TargetName, ESearchCase::IgnoreCase);
	}

	static FBlueprintHelperLogicEntry MakeSyntheticFunctionEntry(
		const FString& EffectiveGraphName,
		const FString& TargetName)
	{
		FBlueprintHelperLogicEntry Entry;
		Entry.Kind = EBlueprintHelperLogicNodeKind::FunctionEntry;
		Entry.Name = TargetName;
		Entry.NodeRef = SyntheticFunctionEntryNodeRef();
		Entry.NodePath = MakeGraphNodePath(EffectiveGraphName, Entry.NodeRef);
		return Entry;
	}

	static FBlueprintHelperLogicNode MakeSyntheticFunctionEntryNode(const FString& TargetName)
	{
		FBlueprintHelperLogicNode Node;
		Node.Kind = EBlueprintHelperLogicNodeKind::FunctionEntry;
		Node.Name = TargetName;
		Node.NodeRef = SyntheticFunctionEntryNodeRef();
		return Node;
	}

	static FBlueprintHelperLogicNode MakeSyntheticFunctionResultNode()
	{
		FBlueprintHelperLogicNode Node;
		Node.Kind = EBlueprintHelperLogicNodeKind::Return;
		Node.Name = TEXT("Return");
		Node.NodeRef = SyntheticFunctionResultNodeRef();
		return Node;
	}

	static bool DoesGraphReferenceNodeId(
		const TSharedPtr<FJsonObject>& GraphObj,
		const FString& NodeId)
	{
		if (!GraphObj.IsValid() || NodeId.IsEmpty())
		{
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* LinksArray = nullptr;
		if (!GraphObj->TryGetArrayField(TEXT("links"), LinksArray) || !LinksArray)
		{
			return false;
		}

		for (const TSharedPtr<FJsonValue>& LinkVal : *LinksArray)
		{
			const TSharedPtr<FJsonObject>* LinkObjPtr = nullptr;
			if (!LinkVal.IsValid() || !LinkVal->TryGetObject(LinkObjPtr) || !LinkObjPtr || !LinkObjPtr->IsValid())
			{
				continue;
			}

			const FString SourceId = ReadFirstStringField(*LinkObjPtr, TEXT("from_id"), TEXT("source_id"), TEXT("from_node"));
			const FString TargetId = ReadFirstStringField(*LinkObjPtr, TEXT("to_id"), TEXT("target_id"), TEXT("to_node"));
			if (SourceId.Equals(NodeId, ESearchCase::IgnoreCase)
				|| TargetId.Equals(NodeId, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		return false;
	}

	static FString ExtractNodeId(const TSharedPtr<FJsonObject>& NodeObj, int32 NodeIndex)
	{
		const FString NodeId = ReadFirstStringField(NodeObj, TEXT("id"), TEXT("node_id"), TEXT("node_guid"));
		return NodeId.IsEmpty() ? FString::Printf(TEXT("Node_%d"), NodeIndex) : NodeId;
	}

	static FString ExtractStableNodeRef(const TSharedPtr<FJsonObject>& NodeObj, int32 NodeIndex)
	{
		const FString NodeGuid = ReadFirstStringField(NodeObj, TEXT("node_guid"));
		if (!NodeGuid.IsEmpty())
		{
			return NodeGuid;
		}

		return MakeNodeRef(NodeIndex);
	}

	static FString NormalizeLogicToken(const FString& InValue)
	{
		FString Result = InValue;
		Result.TrimStartAndEndInline();
		Result.ReplaceInline(TEXT("\""), TEXT(""));
		Result.ReplaceInline(TEXT("'"), TEXT(""));
		Result.ReplaceInline(TEXT(" "), TEXT(""));
		Result.ReplaceInline(TEXT("_"), TEXT(""));
		Result.ReplaceInline(TEXT("-"), TEXT(""));
		return Result.ToLower();
	}

	static bool IsTruthyMetadataValue(const FString& Value)
	{
		const FString Key = NormalizeLogicToken(Value);
		return Key.Equals(TEXT("true"))
			|| Key.Equals(TEXT("1"))
			|| Key.Equals(TEXT("yes"));
	}

	static bool TryReadNodeMetadataStringField(
		const TSharedPtr<FJsonObject>& NodeObj,
		const TCHAR* FieldName,
		FString& OutValue)
	{
		return TryReadStringField(NodeObj, FieldName, OutValue)
			|| TryReadNestedStringField(NodeObj, TEXT("metadata"), FieldName, OutValue)
			|| TryReadNestedStringField(NodeObj, TEXT("meta"), FieldName, OutValue)
			|| TryReadNestedStringField(NodeObj, TEXT("ownership"), FieldName, OutValue);
	}

	static FString ExtractBlueprintHelperBlockId(const TSharedPtr<FJsonObject>& NodeObj)
	{
		FString OwnedValue;
		if (!TryReadNodeMetadataStringField(NodeObj, TEXT("BlueprintHelperOwned"), OwnedValue)
			|| !IsTruthyMetadataValue(OwnedValue))
		{
			return TEXT("");
		}

		FString BlockId;
		if (!TryReadNodeMetadataStringField(NodeObj, TEXT("BlueprintHelperBlockId"), BlockId))
		{
			return TEXT("");
		}

		return BlockId.TrimStartAndEnd();
	}

	static bool IsGroupEntryNode(const FBlueprintHelperLogicNode& Node)
	{
		return Node.Kind == EBlueprintHelperLogicNodeKind::FunctionEntry
			|| Node.Kind == EBlueprintHelperLogicNodeKind::Event
			|| Node.Kind == EBlueprintHelperLogicNodeKind::CustomEvent;
	}

	static void AssignGroupEntry(FBlueprintHelperLogicGroup& Group, const FString& GraphName)
	{
		if (Group.Nodes.Num() == 0)
		{
			return;
		}

		int32 EntryIndex = 0;
		for (int32 NodeIndex = 0; NodeIndex < Group.Nodes.Num(); ++NodeIndex)
		{
			if (IsGroupEntryNode(Group.Nodes[NodeIndex]))
			{
				EntryIndex = NodeIndex;
				break;
			}
		}

		const FBlueprintHelperLogicNode& EntryNode = Group.Nodes[EntryIndex];
		Group.Entry.Kind = EntryNode.Kind;
		Group.Entry.Name = EntryNode.Name;
		Group.Entry.NodeRef = EntryNode.NodeRef;
		Group.Entry.NodePath = MakeGraphNodePath(GraphName, EntryNode.NodeRef);
		Group.GroupEntryNodePath = Group.Entry.NodePath;
	}

	static void NormalizeGroupLocalRefs(
		FBlueprintHelperLogicGroup& Group,
		const TMap<FString, FString>* StableNodeRefByOriginalRef = nullptr)
	{
		TMap<FString, FString> LocalNodeRefByOriginalRef;
		for (int32 NodeIndex = 0; NodeIndex < Group.Nodes.Num(); ++NodeIndex)
		{
			const FString OriginalNodeRef = Group.Nodes[NodeIndex].NodeRef;
			const FString* StableNodeRef = StableNodeRefByOriginalRef
				? StableNodeRefByOriginalRef->Find(OriginalNodeRef)
				: nullptr;
			LocalNodeRefByOriginalRef.Add(
				OriginalNodeRef,
				(StableNodeRef && !StableNodeRef->IsEmpty()) ? *StableNodeRef : MakeNodeRef(NodeIndex));
		}

		const FString OriginalEntryNodeRef = Group.Entry.NodeRef;
		for (int32 NodeIndex = 0; NodeIndex < Group.Nodes.Num(); ++NodeIndex)
		{
			if (const FString* LocalNodeRef = LocalNodeRefByOriginalRef.Find(Group.Nodes[NodeIndex].NodeRef))
			{
				Group.Nodes[NodeIndex].NodeRef = *LocalNodeRef;
			}
			else
			{
				Group.Nodes[NodeIndex].NodeRef = MakeNodeRef(NodeIndex);
			}
		}

		if (const FString* LocalEntryNodeRef = LocalNodeRefByOriginalRef.Find(OriginalEntryNodeRef))
		{
			Group.Entry.NodeRef = *LocalEntryNodeRef;
		}
		else if (Group.Nodes.Num() > 0)
		{
			Group.Entry.NodeRef = Group.Nodes[0].NodeRef;
		}

		int32 LocalLinkIndex = 0;
		for (FBlueprintHelperLogicNode& Node : Group.Nodes)
		{
			for (int32 LinkIndex = 0; LinkIndex < Node.Links.Num();)
			{
				FBlueprintHelperLogicLink& Link = Node.Links[LinkIndex];
				const FString* LocalTargetNodeRef = LocalNodeRefByOriginalRef.Find(Link.ToNode);
				if (!LocalTargetNodeRef)
				{
					Node.Links.RemoveAt(LinkIndex);
					continue;
				}

				Link.ToNode = *LocalTargetNodeRef;
				Link.LinkRef = MakeLinkRef(LocalLinkIndex++);
				if (Link.PinRef.IsEmpty())
				{
					Link.PinRef = Link.FromPin;
				}
				if (StableNodeRefByOriginalRef)
				{
					const FString SourcePinRef = Link.PinRef.IsEmpty() ? Link.FromPin : Link.PinRef;
					Link.LinkRef = MakeEndpointLinkRef(Node.NodeRef, SourcePinRef, Link.ToNode, Link.ToPin);
				}
				++LinkIndex;
			}
		}

		if (Group.GroupEntryNodePath.IsEmpty())
		{
			Group.GroupEntryNodePath = Group.Entry.NodePath;
		}
	}

	static bool TryBuildOwnedBlockGroups(
		const TArray<TSharedPtr<FJsonValue>>& NodesArray,
		const TArray<FBlueprintHelperLogicNode>& GraphNodes,
		const FString& GraphName,
		FBlueprintHelperLogicJsonPayload& Payload)
	{
		TMap<FString, FString> BlockIdByNodeRef;
		TMap<FString, FString> StableNodeRefByOriginalRef;
		for (int32 NodeIndex = 0; NodeIndex < NodesArray.Num(); ++NodeIndex)
		{
			const TSharedPtr<FJsonValue>& NodeValue = NodesArray[NodeIndex];
			const TSharedPtr<FJsonObject>* NodeObjPtr = nullptr;
			if (!NodeValue.IsValid() || !NodeValue->TryGetObject(NodeObjPtr) || !NodeObjPtr || !NodeObjPtr->IsValid())
			{
				continue;
			}

			const FString OriginalNodeRef = MakeNodeRef(NodeIndex);
			StableNodeRefByOriginalRef.Add(OriginalNodeRef, ExtractStableNodeRef(*NodeObjPtr, NodeIndex));

			const FString BlockId = ExtractBlueprintHelperBlockId(*NodeObjPtr);
			if (!BlockId.IsEmpty())
			{
				BlockIdByNodeRef.Add(OriginalNodeRef, BlockId);
			}
		}

		if (BlockIdByNodeRef.Num() == 0)
		{
			return false;
		}

		TArray<FBlueprintHelperLogicGroup> OwnedGroups;
		TMap<FString, int32> GroupIndexByBlockId;
		FBlueprintHelperLogicGroup UnownedGroup;
		UnownedGroup.GroupType = EBlueprintHelperLogicGroupType::GlobalEventFlow;
		UnownedGroup.Name = GraphName;

		for (const FBlueprintHelperLogicNode& Node : GraphNodes)
		{
			if (const FString* BlockId = BlockIdByNodeRef.Find(Node.NodeRef))
			{
				int32 GroupIndex = INDEX_NONE;
				if (const int32* ExistingGroupIndex = GroupIndexByBlockId.Find(*BlockId))
				{
					GroupIndex = *ExistingGroupIndex;
				}
				else
				{
					GroupIndex = OwnedGroups.Num();
					FBlueprintHelperLogicGroup NewGroup;
					NewGroup.GroupType = EBlueprintHelperLogicGroupType::BlueprintHelperBlock;
					NewGroup.BlockId = *BlockId;
					NewGroup.Name = *BlockId;
					OwnedGroups.Add(MoveTemp(NewGroup));
					GroupIndexByBlockId.Add(*BlockId, GroupIndex);
				}

				OwnedGroups[GroupIndex].Nodes.Add(Node);
			}
			else
			{
				UnownedGroup.Nodes.Add(Node);
			}
		}

		Payload.Groups.Reset();
		for (FBlueprintHelperLogicGroup& Group : OwnedGroups)
		{
			if (Group.Nodes.Num() == 0)
			{
				continue;
			}
			AssignGroupEntry(Group, GraphName);
			NormalizeGroupLocalRefs(Group, &StableNodeRefByOriginalRef);
			Payload.Groups.Add(MoveTemp(Group));
		}

		if (UnownedGroup.Nodes.Num() > 0)
		{
			AssignGroupEntry(UnownedGroup, GraphName);
			NormalizeGroupLocalRefs(UnownedGroup);
			Payload.Groups.Add(MoveTemp(UnownedGroup));
		}

		return Payload.Groups.Num() > 0;
	}

	static FString FindUniqueOwnedBlockId(const TArray<TSharedPtr<FJsonValue>>& NodesArray)
	{
		FString UniqueBlockId;
		for (int32 NodeIndex = 0; NodeIndex < NodesArray.Num(); ++NodeIndex)
		{
			const TSharedPtr<FJsonValue>& NodeValue = NodesArray[NodeIndex];
			const TSharedPtr<FJsonObject>* NodeObjPtr = nullptr;
			if (!NodeValue.IsValid() || !NodeValue->TryGetObject(NodeObjPtr) || !NodeObjPtr || !NodeObjPtr->IsValid())
			{
				continue;
			}

			const FString BlockId = ExtractBlueprintHelperBlockId(*NodeObjPtr);
			if (BlockId.IsEmpty())
			{
				continue;
			}

			if (UniqueBlockId.IsEmpty())
			{
				UniqueBlockId = BlockId;
			}
			else if (!UniqueBlockId.Equals(BlockId, ESearchCase::IgnoreCase))
			{
				return TEXT("");
			}
		}

		return UniqueBlockId;
	}

	static EBlueprintHelperLogicLinkType IdentifyGraphLinkType(
		const TSharedPtr<FJsonObject>& LinkObj,
		const FString& FromPin,
		const FString& ToPin)
	{
		const FString ExplicitKind = ReadFirstStringField(LinkObj, TEXT("kind"), TEXT("type"));
		const FString PinType = ReadFirstStringField(LinkObj, TEXT("from_pin_type"), TEXT("to_pin_type"));
		return FBlueprintHelperGraphWriteClassificationUtils::IdentifyGraphLinkType(
			ExplicitKind,
			PinType,
			FromPin,
			ToPin);
	}

	static bool HasEquivalentLink(
		const FBlueprintHelperLogicNode& Node,
		const FString& FromPin,
		const FString& ToNode,
		const FString& ToPin)
	{
		for (const FBlueprintHelperLogicLink& ExistingLink : Node.Links)
		{
			if (ExistingLink.FromPin.Equals(FromPin, ESearchCase::IgnoreCase)
				&& ExistingLink.ToNode.Equals(ToNode, ESearchCase::IgnoreCase)
				&& ExistingLink.ToPin.Equals(ToPin, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		return false;
	}

	static void AttachGraphLevelLinksToNodes(
		const TSharedPtr<FJsonObject>& GraphObj,
		TArray<FBlueprintHelperLogicNode>& Nodes)
	{
		if (!GraphObj.IsValid() || Nodes.Num() == 0)
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* GraphLinksArray = nullptr;
		if (!GraphObj->TryGetArrayField(TEXT("links"), GraphLinksArray) || !GraphLinksArray)
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* GraphNodesArray = nullptr;
		if (!GraphObj->TryGetArrayField(TEXT("nodes"), GraphNodesArray) || !GraphNodesArray)
		{
			return;
		}

		TMap<FString, int32> NodeIndexById;
		TMap<FString, FString> NodeRefById;
		TSet<FString> NodeRefsWithOutLinksField;
		TMap<FString, int32> PayloadNodeIndexByRef;

		for (int32 PayloadIndex = 0; PayloadIndex < Nodes.Num(); ++PayloadIndex)
		{
			PayloadNodeIndexByRef.Add(Nodes[PayloadIndex].NodeRef, PayloadIndex);
		}

		auto AddNodeAlias = [&NodeIndexById, &NodeRefById](const FString& Alias, int32 PayloadIndex, const FString& NodeRef)
		{
			if (Alias.IsEmpty())
			{
				return;
			}

			if (!NodeIndexById.Contains(Alias))
			{
				NodeIndexById.Add(Alias, PayloadIndex);
			}
			if (!NodeRefById.Contains(Alias))
			{
				NodeRefById.Add(Alias, NodeRef);
			}
		};

		for (int32 PayloadIndex = 0; PayloadIndex < Nodes.Num(); ++PayloadIndex)
		{
			if (IsSyntheticFunctionBoundaryNodeRef(Nodes[PayloadIndex].NodeRef))
			{
				AddNodeAlias(Nodes[PayloadIndex].NodeRef, PayloadIndex, Nodes[PayloadIndex].NodeRef);
			}
		}

		for (int32 NodeIndex = 0; NodeIndex < GraphNodesArray->Num(); ++NodeIndex)
		{
			const TSharedPtr<FJsonValue>& NodeVal = (*GraphNodesArray)[NodeIndex];
			const TSharedPtr<FJsonObject>* NodeObjPtr = nullptr;
			if (!NodeVal.IsValid() || !NodeVal->TryGetObject(NodeObjPtr) || !NodeObjPtr || !NodeObjPtr->IsValid())
			{
				continue;
			}

			const FString NodeRef = MakeNodeRef(NodeIndex);
			const int32* PayloadIndex = PayloadNodeIndexByRef.Find(NodeRef);
			if (!PayloadIndex)
			{
				continue;
			}

			const FString NodeId = ExtractNodeId(*NodeObjPtr, NodeIndex);
			AddNodeAlias(NodeId, *PayloadIndex, NodeRef);
			AddNodeAlias(NodeRef, *PayloadIndex, NodeRef);

			if ((*NodeObjPtr)->HasField(TEXT("out_links")))
			{
				NodeRefsWithOutLinksField.Add(NodeRef);
			}
		}

		for (int32 LinkIndex = 0; LinkIndex < GraphLinksArray->Num(); ++LinkIndex)
		{
			const TSharedPtr<FJsonValue>& LinkVal = (*GraphLinksArray)[LinkIndex];
			const TSharedPtr<FJsonObject>* LinkObjPtr = nullptr;
			if (!LinkVal.IsValid() || !LinkVal->TryGetObject(LinkObjPtr) || !LinkObjPtr || !LinkObjPtr->IsValid())
			{
				continue;
			}

			const FString SourceId = ReadFirstStringField(*LinkObjPtr, TEXT("from_id"), TEXT("source_id"), TEXT("from_node"));
			const FString TargetId = ReadFirstStringField(*LinkObjPtr, TEXT("to_id"), TEXT("target_id"), TEXT("to_node"));
			const int32* SourceNodeIndex = NodeIndexById.Find(SourceId);
			const FString* TargetNodeRef = NodeRefById.Find(TargetId);
			if (!SourceNodeIndex || !TargetNodeRef)
			{
				continue;
			}

			FBlueprintHelperLogicNode& SourceNode = Nodes[*SourceNodeIndex];
			if (NodeRefsWithOutLinksField.Contains(SourceNode.NodeRef))
			{
				continue;
			}

			FString FromPin;
			FString ToPin;
			(*LinkObjPtr)->TryGetStringField(TEXT("from_pin"), FromPin);
			(*LinkObjPtr)->TryGetStringField(TEXT("to_pin"), ToPin);

			if (HasEquivalentLink(SourceNode, FromPin, *TargetNodeRef, ToPin))
			{
				continue;
			}

			FBlueprintHelperLogicLink Link;
			Link.LinkRef = MakeLinkRef(LinkIndex);
			Link.Type = IdentifyGraphLinkType(*LinkObjPtr, FromPin, ToPin);
			Link.FromPin = FromPin;
			Link.PinRef = FromPin;
			Link.ToNode = *TargetNodeRef;
			Link.ToPin = ToPin;
			const TSharedPtr<FJsonObject>* ExternalAnchorObj = nullptr;
			if ((*LinkObjPtr)->TryGetObjectField(TEXT("external_anchor"), ExternalAnchorObj)
				&& ExternalAnchorObj
				&& ExternalAnchorObj->IsValid())
			{
				Link.ExternalAnchor = *ExternalAnchorObj;
			}
			SourceNode.Links.Add(MoveTemp(Link));
		}
	}

};

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

			const FString NodeRef = FBlueprintHelperLogicGroupBuilderLocalUtils::MakeNodeRef(i);
			const FString Name = ExtractNodeName(*NodeObjPtr);
			const EBlueprintHelperLogicNodeKind Kind = IdentifyNodeKind(*NodeObjPtr);

			FBlueprintHelperLogicNode Node = ConvertNode(*NodeObjPtr, i);

			if (!bFoundEntry && IsEntryNode(*NodeObjPtr))
			{
				Group.Entry.Kind = Kind;
				Group.Entry.Name = Name;
				Group.Entry.NodeRef = NodeRef;
				Group.Entry.NodePath = FBlueprintHelperLogicGroupBuilderLocalUtils::MakeGraphNodePath(GraphName, NodeRef);
				Group.GroupEntryNodePath = Group.Entry.NodePath;
				bFoundEntry = true;
			}

			Group.Nodes.Add(MoveTemp(Node));
		}

		FBlueprintHelperLogicGroupBuilderLocalUtils::AttachGraphLevelLinksToNodes(RawJson, Group.Nodes);
		if (FBlueprintHelperLogicGroupBuilderLocalUtils::TryBuildOwnedBlockGroups(*NodesArray, Group.Nodes, GraphName, Payload))
		{
			return Payload;
		}

		// 如果没找到明确入口，使用第一个节点
		if (!bFoundEntry && Group.Nodes.Num() > 0)
		{
			Group.Entry.Kind = Group.Nodes[0].Kind;
			Group.Entry.Name = Group.Nodes[0].Name;
			Group.Entry.NodeRef = Group.Nodes[0].NodeRef;
			Group.Entry.NodePath = FBlueprintHelperLogicGroupBuilderLocalUtils::MakeGraphNodePath(GraphName, Group.Nodes[0].NodeRef);
			Group.GroupEntryNodePath = Group.Entry.NodePath;
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

			const FString NodeRef = FBlueprintHelperLogicGroupBuilderLocalUtils::MakeNodeRef(i);
			const FString Name = ExtractNodeName(*NodeObjPtr);
			const EBlueprintHelperLogicNodeKind Kind = IdentifyNodeKind(*NodeObjPtr);

			FBlueprintHelperLogicNode Node = ConvertNode(*NodeObjPtr, i);

			if (!bFoundEntry && IsEntryNode(*NodeObjPtr))
			{
				FBlueprintHelperLogicEntry Entry;
				Entry.Kind = Kind;
				Entry.Name = Name;
				Entry.NodeRef = NodeRef;
				Entry.NodePath = FBlueprintHelperLogicGroupBuilderLocalUtils::MakeGraphNodePath(GraphName, NodeRef);
				Payload.Entry = Entry;
				bFoundEntry = true;
			}

			Payload.Nodes.Add(MoveTemp(Node));
		}

		FBlueprintHelperLogicGroupBuilderLocalUtils::AttachGraphLevelLinksToNodes(RawJson, Payload.Nodes);
		Payload.BlockId = FBlueprintHelperLogicGroupBuilderLocalUtils::FindUniqueOwnedBlockId(*NodesArray);

		if (!bFoundEntry && Payload.Nodes.Num() > 0)
		{
			FBlueprintHelperLogicEntry Entry;
			Entry.Kind = Payload.Nodes[0].Kind;
			Entry.Name = Payload.Nodes[0].Name;
			Entry.NodeRef = Payload.Nodes[0].NodeRef;
			Entry.NodePath = FBlueprintHelperLogicGroupBuilderLocalUtils::MakeGraphNodePath(GraphName, Payload.Nodes[0].NodeRef);
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
	else if (Scope == EBlueprintHelperLogicScope::TargetFunction)
	{
		Payload.Function = TargetName;
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
		case EBlueprintHelperLogicScope::TargetFunction:
			return Kind == EBlueprintHelperLogicNodeKind::FunctionEntry;
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

	auto TryBuildFromGraph = [this, &Payload, &MatchesScope, &MatchesTargetName, Scope, &TargetName](
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

		const bool bCanSynthesizeFunctionEntry =
			Scope == EBlueprintHelperLogicScope::TargetFunction
			&& FBlueprintHelperLogicGroupBuilderLocalUtils::DoesGraphNameMatchTargetFunction(EffectiveGraphName, TargetName)
			&& NodesArray->Num() > 0;

		if (EntryIndex == INDEX_NONE && !bCanSynthesizeFunctionEntry)
		{
			return false;
		}

		Payload.Graph = EffectiveGraphName;
		Payload.Entry.Reset();
		Payload.Nodes.Reset();
		Payload.Groups.Reset();

		if (EntryIndex == INDEX_NONE && bCanSynthesizeFunctionEntry)
		{
			Payload.Entry = FBlueprintHelperLogicGroupBuilderLocalUtils::MakeSyntheticFunctionEntry(
				EffectiveGraphName,
				TargetName);
			Payload.Nodes.Add(FBlueprintHelperLogicGroupBuilderLocalUtils::MakeSyntheticFunctionEntryNode(TargetName));
		}

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
				Entry.NodeRef = FBlueprintHelperLogicGroupBuilderLocalUtils::MakeNodeRef(i);
				Entry.NodePath = FBlueprintHelperLogicGroupBuilderLocalUtils::MakeGraphNodePath(EffectiveGraphName, Entry.NodeRef);
				Payload.Entry = Entry;
			}

			Payload.Nodes.Add(ConvertNode(*NodeObjPtr, i));
		}

		if (EntryIndex == INDEX_NONE
			&& bCanSynthesizeFunctionEntry
			&& FBlueprintHelperLogicGroupBuilderLocalUtils::DoesGraphReferenceNodeId(
				GraphObj,
				FBlueprintHelperLogicGroupBuilderLocalUtils::SyntheticFunctionResultNodeRef()))
		{
			Payload.Nodes.Add(FBlueprintHelperLogicGroupBuilderLocalUtils::MakeSyntheticFunctionResultNode());
		}

		FBlueprintHelperLogicGroupBuilderLocalUtils::AttachGraphLevelLinksToNodes(GraphObj, Payload.Nodes);
		Payload.BlockId = FBlueprintHelperLogicGroupBuilderLocalUtils::FindUniqueOwnedBlockId(*NodesArray);

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

	return FBlueprintHelperGraphWriteClassificationUtils::IdentifyNodeKind(ClassName, MemberName);
}

FBlueprintHelperLogicNode FBlueprintHelperLogicGroupBuilder::ConvertNode(const TSharedPtr<FJsonObject>& NodeObj, int32 Index)
{
	FBlueprintHelperLogicNode Node;
	Node.NodeRef = FBlueprintHelperLogicGroupBuilderLocalUtils::MakeNodeRef(Index);
	Node.Kind = IdentifyNodeKind(NodeObj);
	Node.Name = ExtractNodeName(NodeObj);
	Node.Owner = ExtractOwner(NodeObj);
	const TSharedPtr<FJsonObject>* ExternalAnchorObj = nullptr;
	if (NodeObj->TryGetObjectField(TEXT("external_anchor"), ExternalAnchorObj)
		&& ExternalAnchorObj
		&& ExternalAnchorObj->IsValid())
	{
		Node.ExternalAnchor = *ExternalAnchorObj;
	}

	const TArray<TSharedPtr<FJsonValue>>* ExternalAnchorArray = nullptr;
	if (NodeObj->TryGetArrayField(TEXT("external_anchors"), ExternalAnchorArray) && ExternalAnchorArray)
	{
		for (const TSharedPtr<FJsonValue>& AnchorValue : *ExternalAnchorArray)
		{
			const TSharedPtr<FJsonObject>* AnchorObj = nullptr;
			if (AnchorValue.IsValid()
				&& AnchorValue->TryGetObject(AnchorObj)
				&& AnchorObj
				&& AnchorObj->IsValid())
			{
				Node.ExternalAnchors.Add(*AnchorObj);
			}
		}
	}

	const TSharedPtr<FJsonObject>* InputsObj = nullptr;
	if (NodeObj->TryGetObjectField(TEXT("inputs"), InputsObj) && InputsObj && InputsObj->IsValid())
	{
		Node.Inputs = *InputsObj;
	}

	const TSharedPtr<FJsonObject>* InputDefaultsObj = nullptr;
	if (NodeObj->TryGetObjectField(TEXT("input_defaults"), InputDefaultsObj) && InputDefaultsObj && InputDefaultsObj->IsValid())
	{
		Node.InputDefaults = *InputDefaultsObj;
	}

	const TSharedPtr<FJsonObject>* OutputsObj = nullptr;
	if (NodeObj->TryGetObjectField(TEXT("outputs"), OutputsObj) && OutputsObj && OutputsObj->IsValid())
	{
		Node.Outputs = *OutputsObj;
	}

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
			Link.LinkRef = FBlueprintHelperLogicGroupBuilderLocalUtils::MakeLinkRef(j);

			(*LinkObjPtr)->TryGetStringField(TEXT("from_pin"), Link.FromPin);
			Link.PinRef = Link.FromPin;
			Link.ToNode = FBlueprintHelperLogicGroupBuilderLocalUtils::ReadFirstStringField(
				*LinkObjPtr,
				TEXT("to_node"),
				TEXT("target_id"),
				TEXT("to_id"));
			Link.ToPin = FBlueprintHelperLogicGroupBuilderLocalUtils::ReadFirstStringField(
				*LinkObjPtr,
				TEXT("to_pin"),
				TEXT("target_pin"));
			Link.Type = FBlueprintHelperLogicGroupBuilderLocalUtils::IdentifyGraphLinkType(*LinkObjPtr, Link.FromPin, Link.ToPin);
			const TSharedPtr<FJsonObject>* LinkExternalAnchorObj = nullptr;
			if ((*LinkObjPtr)->TryGetObjectField(TEXT("external_anchor"), LinkExternalAnchorObj)
				&& LinkExternalAnchorObj
				&& LinkExternalAnchorObj->IsValid())
			{
				Link.ExternalAnchor = *LinkExternalAnchorObj;
			}

			Node.Links.Add(MoveTemp(Link));
		}
	}

	return Node;
}

FString FBlueprintHelperLogicGroupBuilder::ExtractNodeName(const TSharedPtr<FJsonObject>& NodeObj)
{
	if (!NodeObj.IsValid()) return TEXT("Unknown");

	FString Name;
	if (NodeObj->TryGetStringField(TEXT("name"), Name) && !Name.IsEmpty()) return FBlueprintHelperLogicGroupBuilderLocalUtils::SanitizeDisplayText(Name);
	if (NodeObj->TryGetStringField(TEXT("member_name"), Name) && !Name.IsEmpty()) return FBlueprintHelperLogicGroupBuilderLocalUtils::SanitizeDisplayText(Name);
	if (NodeObj->TryGetStringField(TEXT("function_name"), Name) && !Name.IsEmpty()) return FBlueprintHelperLogicGroupBuilderLocalUtils::SanitizeDisplayText(Name);
	const TSharedPtr<FJsonObject>* EventObj = nullptr;
	if (NodeObj->TryGetObjectField(TEXT("event"), EventObj) && EventObj && EventObj->IsValid())
	{
		if ((*EventObj)->TryGetStringField(TEXT("event_name"), Name) && !Name.IsEmpty()) return FBlueprintHelperLogicGroupBuilderLocalUtils::SanitizeDisplayText(Name);
	}
	NodeObj->TryGetStringField(TEXT("class"), Name);
	if (Name.IsEmpty())
	{
		NodeObj->TryGetStringField(TEXT("type"), Name);
	}
	const FString SanitizedName = FBlueprintHelperLogicGroupBuilderLocalUtils::SanitizeDisplayText(Name);
	return SanitizedName.IsEmpty() ? TEXT("Unknown") : SanitizedName;
}

FString FBlueprintHelperLogicGroupBuilder::ExtractOwner(const TSharedPtr<FJsonObject>& NodeObj)
{
	if (!NodeObj.IsValid()) return TEXT("");

	FString Owner;
	NodeObj->TryGetStringField(TEXT("owner"), Owner);
	return FBlueprintHelperLogicGroupBuilderLocalUtils::SanitizeDisplayText(Owner);
}

bool FBlueprintHelperLogicGroupBuilder::IsEntryNode(const TSharedPtr<FJsonObject>& NodeObj)
{
	if (!NodeObj.IsValid()) return false;

	const EBlueprintHelperLogicNodeKind Kind = IdentifyNodeKind(NodeObj);
	switch (Kind)
	{
	case EBlueprintHelperLogicNodeKind::FunctionEntry:
	case EBlueprintHelperLogicNodeKind::Event:
	case EBlueprintHelperLogicNodeKind::CustomEvent:
		return true;
	default:
		return false;
	}
}
