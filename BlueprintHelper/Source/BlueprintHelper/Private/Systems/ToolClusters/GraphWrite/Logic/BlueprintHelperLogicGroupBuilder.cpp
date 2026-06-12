// BlueprintHelper Service Layer — Logic Group Builder 实现

#include "Systems/ToolClusters/GraphWrite/Logic/BlueprintHelperLogicGroupBuilder.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "Systems/ToolClusters/GraphWrite/Logic/Utils/BlueprintHelperGraphWriteClassificationUtils.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

class FBlueprintHelperLogicGroupBuilderLocalUtils
{
public:
	struct FAdapterBoundaryRef
	{
		FString NodeRef;
		FString DisplayName;
	};

	struct FAdapterBoundaryProjection
	{
		bool bPresent = false;
		FString RuntimeAdapterId;
		FString BodyKind;
		FString GraphName;
		TArray<FAdapterBoundaryRef> EntryBoundaries;
		TArray<FAdapterBoundaryRef> ExitBoundaries;
		TSet<FString> FoldedBoundaryNodeRefs;
		TSet<FString> VisibleBoundaryNodeRefs;

		bool HasEntryBoundary() const
		{
			return EntryBoundaries.Num() > 0 && !EntryBoundaries[0].NodeRef.IsEmpty();
		}

		bool IsPresent() const
		{
			return bPresent;
		}

		bool HasExitBoundary() const
		{
			return ExitBoundaries.Num() > 0 && !ExitBoundaries[0].NodeRef.IsEmpty();
		}

		FAdapterBoundaryRef FirstEntryBoundary() const
		{
			return HasEntryBoundary() ? EntryBoundaries[0] : FAdapterBoundaryRef();
		}

		FAdapterBoundaryRef FirstExitBoundary() const
		{
			return HasExitBoundary() ? ExitBoundaries[0] : FAdapterBoundaryRef();
		}
	};

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

	static TSharedPtr<FJsonObject> CopyJsonObject(const TSharedPtr<FJsonObject>& Source)
	{
		if (!Source.IsValid())
		{
			return nullptr;
		}

		TSharedPtr<FJsonObject> Copy = MakeShared<FJsonObject>();
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Source->Values)
		{
			Copy->SetField(Field.Key, Field.Value);
		}
		return Copy;
	}

	static TArray<FString> ReadStringArrayField(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName)
	{
		TArray<FString> Values;
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (!Object.IsValid() || !Object->TryGetArrayField(FieldName, Array) || !Array)
		{
			return Values;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Array)
		{
			const FString StringValue = JsonValueToString(Value).TrimStartAndEnd();
			if (!StringValue.IsEmpty())
			{
				Values.Add(StringValue);
			}
		}
		return Values;
	}

	static TArray<FAdapterBoundaryRef> ReadBoundaryRefArray(
		const TSharedPtr<FJsonObject>& Boundary,
		const TCHAR* FieldName)
	{
		TArray<FAdapterBoundaryRef> Refs;
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (!Boundary.IsValid() || !Boundary->TryGetArrayField(FieldName, Array) || !Array)
		{
			return Refs;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Array)
		{
			const TSharedPtr<FJsonObject>* RefObj = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(RefObj) || !RefObj || !RefObj->IsValid())
			{
				continue;
			}

			FAdapterBoundaryRef Ref;
			Ref.NodeRef = ReadFirstStringField(*RefObj, TEXT("node_ref"), TEXT("nodeRef"), TEXT("ref"));
			Ref.DisplayName = ReadFirstStringField(*RefObj, TEXT("display_name"), TEXT("displayName"), TEXT("name"));
			if (!Ref.NodeRef.IsEmpty())
			{
				Refs.Add(MoveTemp(Ref));
			}
		}
		return Refs;
	}

	static FAdapterBoundaryProjection ReadAdapterBoundary(
		const TSharedPtr<FJsonObject>& RawJson)
	{
		FAdapterBoundaryProjection Projection;
		const TSharedPtr<FJsonObject>* Boundary = nullptr;
		if (!RawJson.IsValid()
			|| !RawJson->TryGetObjectField(TEXT("adapter_boundary"), Boundary)
			|| !Boundary
			|| !Boundary->IsValid())
		{
			return Projection;
		}

		Projection.bPresent = true;
		TryReadStringField(*Boundary, TEXT("runtime_adapter_id"), Projection.RuntimeAdapterId);
		TryReadStringField(*Boundary, TEXT("body_kind"), Projection.BodyKind);
		TryReadStringField(*Boundary, TEXT("graph_name"), Projection.GraphName);
		Projection.EntryBoundaries = ReadBoundaryRefArray(*Boundary, TEXT("entry_boundaries"));
		Projection.ExitBoundaries = ReadBoundaryRefArray(*Boundary, TEXT("exit_boundaries"));
		for (const FString& Ref : ReadStringArrayField(*Boundary, TEXT("folded_boundary_node_refs")))
		{
			Projection.FoldedBoundaryNodeRefs.Add(Ref);
		}
		for (const FString& Ref : ReadStringArrayField(*Boundary, TEXT("visible_boundary_node_refs")))
		{
			Projection.VisibleBoundaryNodeRefs.Add(Ref);
		}
		return Projection;
	}

	static TSharedPtr<FJsonObject> NormalizeExternalAnchorForNode(
		const TSharedPtr<FJsonObject>& AnchorObj,
		const TSharedPtr<FJsonObject>& NodeObj,
		const FString& AssetPath)
	{
		TSharedPtr<FJsonObject> Anchor = CopyJsonObject(AnchorObj);
		if (!Anchor.IsValid())
		{
			return nullptr;
		}

		FString ExistingSchema;
		if (!TryReadStringField(Anchor, TEXT("schema"), ExistingSchema))
		{
			Anchor->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.ExternalGraphAnchor.v1"));
		}

		if (!AssetPath.IsEmpty())
		{
			Anchor->SetStringField(TEXT("asset_path"), AssetPath);
		}

		FString ExistingNodeGuid;
		if (!TryReadStringField(Anchor, TEXT("node_guid"), ExistingNodeGuid))
		{
			const FString NodeGuid = ReadFirstStringField(NodeObj, TEXT("node_guid"), TEXT("id"), TEXT("node_id"));
			if (!NodeGuid.IsEmpty())
			{
				Anchor->SetStringField(TEXT("node_guid"), NodeGuid);
			}
		}
		return Anchor;
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

	static bool DoesAdapterBoundaryMatchGraph(
		const FAdapterBoundaryProjection& Projection,
		const FString& EffectiveGraphName,
		const FString& TargetName)
	{
		if (Projection.GraphName.IsEmpty())
		{
			return false;
		}
		if (!Projection.GraphName.Equals(EffectiveGraphName, ESearchCase::IgnoreCase))
		{
			return false;
		}
		if (Projection.BodyKind.Equals(TEXT("k2.function_body"), ESearchCase::IgnoreCase)
			&& !TargetName.IsEmpty()
			&& !Projection.GraphName.Equals(TargetName, ESearchCase::IgnoreCase))
		{
			return false;
		}
		return true;
	}

	static FBlueprintHelperLogicEntry MakeAdapterFunctionEntry(
		const FAdapterBoundaryProjection& Projection,
		const FString& EffectiveGraphName,
		const FString& TargetName)
	{
		const FAdapterBoundaryRef BoundaryRef = Projection.FirstEntryBoundary();
		FBlueprintHelperLogicEntry Entry;
		Entry.Kind = EBlueprintHelperLogicNodeKind::FunctionEntry;
		Entry.Name = BoundaryRef.DisplayName.IsEmpty() ? TargetName : BoundaryRef.DisplayName;
		Entry.NodeRef = BoundaryRef.NodeRef;
		Entry.NodePath = MakeGraphNodePath(EffectiveGraphName, Entry.NodeRef);
		return Entry;
	}

	static FBlueprintHelperLogicNode MakeAdapterFunctionEntryNode(
		const FAdapterBoundaryProjection& Projection,
		const FString& TargetName)
	{
		const FAdapterBoundaryRef BoundaryRef = Projection.FirstEntryBoundary();
		FBlueprintHelperLogicNode Node;
		Node.Kind = EBlueprintHelperLogicNodeKind::FunctionEntry;
		Node.Name = BoundaryRef.DisplayName.IsEmpty() ? TargetName : BoundaryRef.DisplayName;
		Node.NodeRef = BoundaryRef.NodeRef;
		return Node;
	}

	static FBlueprintHelperLogicNode MakeAdapterFunctionResultNode(
		const FAdapterBoundaryProjection& Projection)
	{
		const FAdapterBoundaryRef BoundaryRef = Projection.FirstExitBoundary();
		FBlueprintHelperLogicNode Node;
		Node.Kind = EBlueprintHelperLogicNodeKind::Return;
		Node.Name = BoundaryRef.DisplayName.IsEmpty() ? TEXT("Return") : BoundaryRef.DisplayName;
		Node.NodeRef = BoundaryRef.NodeRef;
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

	static FString NormalizeAdapterBoundaryName(const FString& NodeRef)
	{
		int32 ColonIndex = INDEX_NONE;
		if (NodeRef.FindChar(TEXT(':'), ColonIndex) && ColonIndex + 1 < NodeRef.Len())
		{
			return NodeRef.Mid(ColonIndex + 1);
		}
		return NodeRef;
	}

	static FString ExtractAdapterBoundaryComparableNodeName(const TSharedPtr<FJsonObject>& NodeObj)
	{
		if (!NodeObj.IsValid())
		{
			return TEXT("");
		}

		FString Name = ReadFirstStringField(
			NodeObj,
			TEXT("name"),
			TEXT("member_name"),
			TEXT("function_name"));
		if (!Name.IsEmpty())
		{
			return Name;
		}

		const TSharedPtr<FJsonObject>* EventObj = nullptr;
		if (NodeObj->TryGetObjectField(TEXT("event"), EventObj) && EventObj && EventObj->IsValid())
		{
			Name = ReadFirstStringField(*EventObj, TEXT("event_name"), TEXT("name"));
			if (!Name.IsEmpty())
			{
				return Name;
			}
		}

		return ReadFirstStringField(NodeObj, TEXT("event_name"), TEXT("class"), TEXT("type"));
	}

	static bool DoesNodeMatchAdapterEntryBoundary(
		const FAdapterBoundaryProjection& Projection,
		const TSharedPtr<FJsonObject>& NodeObj,
		int32 NodeIndex)
	{
		if (!Projection.HasEntryBoundary() || !NodeObj.IsValid())
		{
			return false;
		}

		const FString StableNodeRef = ExtractStableNodeRef(NodeObj, NodeIndex);
		const FString NodeName = ExtractAdapterBoundaryComparableNodeName(NodeObj);
		FString EventName;
		const TSharedPtr<FJsonObject>* EventObj = nullptr;
		if (NodeObj->TryGetObjectField(TEXT("event"), EventObj) && EventObj && EventObj->IsValid())
		{
			(*EventObj)->TryGetStringField(TEXT("event_name"), EventName);
		}
		if (EventName.IsEmpty())
		{
			NodeObj->TryGetStringField(TEXT("event_name"), EventName);
		}

		for (const FAdapterBoundaryRef& BoundaryRef : Projection.EntryBoundaries)
		{
			if (BoundaryRef.NodeRef.IsEmpty())
			{
				continue;
			}
			if (StableNodeRef.Equals(BoundaryRef.NodeRef, ESearchCase::IgnoreCase))
			{
				return true;
			}

			const FString BoundaryName = NormalizeAdapterBoundaryName(BoundaryRef.NodeRef);
			if (!BoundaryName.IsEmpty() &&
				(NodeName.Equals(BoundaryName, ESearchCase::IgnoreCase) ||
					EventName.Equals(BoundaryName, ESearchCase::IgnoreCase)))
			{
				return true;
			}
		}
		return false;
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
		const TMap<FString, FString>* StableNodeRefByOriginalRef = nullptr,
		const TMap<FString, FString>* CrossGroupNodeRefByOriginalRef = nullptr)
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
				const FString* CrossGroupTargetNodeRef = !LocalTargetNodeRef && CrossGroupNodeRefByOriginalRef
					? CrossGroupNodeRefByOriginalRef->Find(Link.ToNode)
					: nullptr;
				if (!LocalTargetNodeRef && !CrossGroupTargetNodeRef)
				{
					Node.Links.RemoveAt(LinkIndex);
					continue;
				}

				Link.ToNode = LocalTargetNodeRef ? *LocalTargetNodeRef : *CrossGroupTargetNodeRef;
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
			NormalizeGroupLocalRefs(Group, &StableNodeRefByOriginalRef, &StableNodeRefByOriginalRef);
			Payload.Groups.Add(MoveTemp(Group));
		}

		if (UnownedGroup.Nodes.Num() > 0)
		{
			AssignGroupEntry(UnownedGroup, GraphName);
			NormalizeGroupLocalRefs(UnownedGroup, &StableNodeRefByOriginalRef, &StableNodeRefByOriginalRef);
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
		TArray<FBlueprintHelperLogicNode>& Nodes,
		const FAdapterBoundaryProjection* AdapterBoundary = nullptr)
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
			if (!AdapterBoundary)
			{
				continue;
			}
			for (const FAdapterBoundaryRef& BoundaryRef : AdapterBoundary->EntryBoundaries)
			{
				if (Nodes[PayloadIndex].NodeRef.Equals(BoundaryRef.NodeRef, ESearchCase::IgnoreCase))
				{
					AddNodeAlias(Nodes[PayloadIndex].NodeRef, PayloadIndex, Nodes[PayloadIndex].NodeRef);
				}
			}
			for (const FAdapterBoundaryRef& BoundaryRef : AdapterBoundary->ExitBoundaries)
			{
				if (Nodes[PayloadIndex].NodeRef.Equals(BoundaryRef.NodeRef, ESearchCase::IgnoreCase))
				{
					AddNodeAlias(Nodes[PayloadIndex].NodeRef, PayloadIndex, Nodes[PayloadIndex].NodeRef);
				}
			}
			if (AdapterBoundary->FoldedBoundaryNodeRefs.Contains(Nodes[PayloadIndex].NodeRef)
				|| AdapterBoundary->VisibleBoundaryNodeRefs.Contains(Nodes[PayloadIndex].NodeRef))
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
	const FBlueprintHelperLogicGroupBuilderLocalUtils::FAdapterBoundaryProjection AdapterBoundary =
		FBlueprintHelperLogicGroupBuilderLocalUtils::ReadAdapterBoundary(RawJson);

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

			FBlueprintHelperLogicNode Node = ConvertNode(*NodeObjPtr, i, AssetPath);

			const bool bAdapterBoundaryEntry =
				AdapterBoundary.IsPresent() &&
				FBlueprintHelperLogicGroupBuilderLocalUtils::DoesNodeMatchAdapterEntryBoundary(
					AdapterBoundary,
					*NodeObjPtr,
					i);
			if (!bFoundEntry && (
				(!AdapterBoundary.IsPresent() && IsEntryNode(*NodeObjPtr)) ||
				bAdapterBoundaryEntry))
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

		FBlueprintHelperLogicGroupBuilderLocalUtils::AttachGraphLevelLinksToNodes(
			RawJson,
			Group.Nodes,
			&AdapterBoundary);
		if (FBlueprintHelperLogicGroupBuilderLocalUtils::TryBuildOwnedBlockGroups(*NodesArray, Group.Nodes, GraphName, Payload))
		{
			return Payload;
		}

		// 如果没找到明确入口，使用第一个节点
		if (!AdapterBoundary.IsPresent() && !bFoundEntry && Group.Nodes.Num() > 0)
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

			FBlueprintHelperLogicNode Node = ConvertNode(*NodeObjPtr, i, AssetPath);

			const bool bAdapterBoundaryEntry =
				AdapterBoundary.IsPresent() &&
				FBlueprintHelperLogicGroupBuilderLocalUtils::DoesNodeMatchAdapterEntryBoundary(
					AdapterBoundary,
					*NodeObjPtr,
					i);
			if (!bFoundEntry && (
				(!AdapterBoundary.IsPresent() && IsEntryNode(*NodeObjPtr)) ||
				bAdapterBoundaryEntry))
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

		FBlueprintHelperLogicGroupBuilderLocalUtils::AttachGraphLevelLinksToNodes(
			RawJson,
			Payload.Nodes,
			&AdapterBoundary);
		Payload.BlockId = FBlueprintHelperLogicGroupBuilderLocalUtils::FindUniqueOwnedBlockId(*NodesArray);

		if (!AdapterBoundary.IsPresent() && !bFoundEntry && Payload.Nodes.Num() > 0)
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
	const FBlueprintHelperLogicGroupBuilderLocalUtils::FAdapterBoundaryProjection AdapterBoundary =
		FBlueprintHelperLogicGroupBuilderLocalUtils::ReadAdapterBoundary(RawJson);

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

	auto TryBuildFromGraph = [
		this,
		&Payload,
		&MatchesScope,
		&MatchesTargetName,
		&AdapterBoundary,
		Scope,
		&TargetName,
		&AssetPath](
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
			const bool bAdapterBoundaryEntry =
				AdapterBoundary.IsPresent() &&
				FBlueprintHelperLogicGroupBuilderLocalUtils::DoesAdapterBoundaryMatchGraph(
					AdapterBoundary,
					EffectiveGraphName,
					TargetName) &&
				FBlueprintHelperLogicGroupBuilderLocalUtils::DoesNodeMatchAdapterEntryBoundary(
					AdapterBoundary,
					*NodeObjPtr,
					i);
			if ((!AdapterBoundary.IsPresent() && IsEntryNode(*NodeObjPtr) && MatchesScope(Kind) && MatchesTargetName(*NodeObjPtr)) ||
				bAdapterBoundaryEntry)
			{
				EntryIndex = i;
				break;
			}
		}

		const bool bCanSynthesizeFunctionEntry =
			Scope == EBlueprintHelperLogicScope::TargetFunction
			&& AdapterBoundary.HasEntryBoundary()
			&& FBlueprintHelperLogicGroupBuilderLocalUtils::DoesAdapterBoundaryMatchGraph(
				AdapterBoundary,
				EffectiveGraphName,
				TargetName)
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
			Payload.Entry = FBlueprintHelperLogicGroupBuilderLocalUtils::MakeAdapterFunctionEntry(
				AdapterBoundary,
				EffectiveGraphName,
				TargetName);
			Payload.Nodes.Add(FBlueprintHelperLogicGroupBuilderLocalUtils::MakeAdapterFunctionEntryNode(
				AdapterBoundary,
				TargetName));
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

			Payload.Nodes.Add(ConvertNode(*NodeObjPtr, i, AssetPath));
		}

		if (EntryIndex == INDEX_NONE
			&& bCanSynthesizeFunctionEntry
			&& AdapterBoundary.HasExitBoundary()
			&& FBlueprintHelperLogicGroupBuilderLocalUtils::DoesGraphReferenceNodeId(
				GraphObj,
				AdapterBoundary.FirstExitBoundary().NodeRef))
		{
			Payload.Nodes.Add(FBlueprintHelperLogicGroupBuilderLocalUtils::MakeAdapterFunctionResultNode(AdapterBoundary));
		}

		FBlueprintHelperLogicGroupBuilderLocalUtils::AttachGraphLevelLinksToNodes(
			GraphObj,
			Payload.Nodes,
			&AdapterBoundary);
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

FBlueprintHelperLogicNode FBlueprintHelperLogicGroupBuilder::ConvertNode(
	const TSharedPtr<FJsonObject>& NodeObj,
	int32 Index,
	const FString& AssetPath)
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
		Node.ExternalAnchor = FBlueprintHelperLogicGroupBuilderLocalUtils::NormalizeExternalAnchorForNode(
			*ExternalAnchorObj,
			NodeObj,
			AssetPath);
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
				TSharedPtr<FJsonObject> NormalizedAnchor =
					FBlueprintHelperLogicGroupBuilderLocalUtils::NormalizeExternalAnchorForNode(
						*AnchorObj,
						NodeObj,
						AssetPath);
				if (NormalizedAnchor.IsValid())
				{
					Node.ExternalAnchors.Add(NormalizedAnchor);
				}
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

	const TArray<TSharedPtr<FJsonValue>>* PinsArray = nullptr;
	if (NodeObj->TryGetArrayField(TEXT("pins"), PinsArray) && PinsArray)
	{
		for (const TSharedPtr<FJsonValue>& PinValue : *PinsArray)
		{
			const TSharedPtr<FJsonObject>* PinObj = nullptr;
			if (PinValue.IsValid()
				&& PinValue->TryGetObject(PinObj)
				&& PinObj
				&& PinObj->IsValid())
			{
				Node.Pins.Add(*PinObj);
			}
		}
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
				Link.ExternalAnchor = FBlueprintHelperLogicGroupBuilderLocalUtils::NormalizeExternalAnchorForNode(
					*LinkExternalAnchorObj,
					NodeObj,
					AssetPath);
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
