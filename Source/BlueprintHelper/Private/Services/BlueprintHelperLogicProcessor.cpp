#include "Services/BlueprintHelperLogicProcessor.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include <initializer_list>

namespace
{
	struct FLogicNode
	{
		FString Id;
		FString Label;
		FString Category;
		FString RawType;
		double X = 0.0;
		double Y = 0.0;
		bool bHasPosition = false;
	};

	struct FLogicLink
	{
		FString SourceNodeId;
		FString SourcePin;
		FString TargetNodeId;
		FString TargetPin;
		FString Kind;
		FString Confidence;
	};

	struct FLogicGraph
	{
		FString Name;
		TArray<FLogicNode> Nodes;
		TArray<FLogicLink> ExecLinks;
		TArray<FLogicLink> DataLinks;
		TArray<FLogicLink> UnknownLinks;
		TArray<FString> EntryPointNodeIds;
		TArray<FString> OrphanNodeIds;
		TMap<FString, int32> NodeIndexById;
	};

	struct FLogicTotals
	{
		int32 NodeCount = 0;
		int32 ExecLinkCount = 0;
		int32 DataLinkCount = 0;
		int32 EntryPointCount = 0;
		int32 OrphanNodeCount = 0;
		int32 UnknownLinkCount = 0;
	};

	FString JsonValueToString(const TSharedPtr<FJsonValue>& Value)
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

	bool TryReadStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, FString& OutValue)
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

	FString ReadFirstStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* First, const TCHAR* Second = nullptr, const TCHAR* Third = nullptr)
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

	bool TryReadNestedStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* ObjectField, const TCHAR* StringField, FString& OutValue)
	{
		const TSharedPtr<FJsonObject>* NestedObject = nullptr;
		if (Object.IsValid()
			&& Object->TryGetObjectField(ObjectField, NestedObject)
			&& NestedObject
			&& NestedObject->IsValid())
		{
			return TryReadStringField(*NestedObject, StringField, OutValue);
		}

		return false;
	}

	FString NormalizeToken(const FString& InValue)
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

	FString NormalizeNodeTypeName(const FString& InValue)
	{
		FString Result = InValue;
		Result.TrimStartAndEndInline();
		Result.ReplaceInline(TEXT("\""), TEXT(""));

		const int32 LastSlashIndex = Result.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (LastSlashIndex != INDEX_NONE)
		{
			Result = Result.Mid(LastSlashIndex + 1);
		}

		const int32 LastDotIndex = Result.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (LastDotIndex != INDEX_NONE)
		{
			Result = Result.Mid(LastDotIndex + 1);
		}

		return Result.TrimStartAndEnd();
	}

	bool ContainsAny(const FString& Text, std::initializer_list<const TCHAR*> Needles)
	{
		for (const TCHAR* Needle : Needles)
		{
			if (Needle && Text.Contains(Needle, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	FString ResolveGraphName(const TSharedPtr<FJsonObject>& GraphObject, int32 GraphIndex)
	{
		FString Name = ReadFirstStringField(GraphObject, TEXT("name"), TEXT("graph"), TEXT("graph_name"));
		if (!Name.IsEmpty())
		{
			return Name;
		}

		return GraphIndex == 0 ? TEXT("Graph") : FString::Printf(TEXT("Graph_%d"), GraphIndex + 1);
	}

	FString ResolveNodeLabel(const TSharedPtr<FJsonObject>& NodeObject, const FString& NodeId)
	{
		FString Label = ReadFirstStringField(NodeObject, TEXT("name"), TEXT("display_name"), TEXT("function_name"));
		if (!Label.IsEmpty())
		{
			return Label;
		}

		if (TryReadNestedStringField(NodeObject, TEXT("variable"), TEXT("name"), Label)
			|| TryReadNestedStringField(NodeObject, TEXT("event"), TEXT("event_name"), Label)
			|| TryReadNestedStringField(NodeObject, TEXT("macro"), TEXT("name"), Label)
			|| TryReadNestedStringField(NodeObject, TEXT("delegate"), TEXT("delegate_property_name"), Label)
			|| TryReadNestedStringField(NodeObject, TEXT("delegate"), TEXT("property_name"), Label)
			|| TryReadNestedStringField(NodeObject, TEXT("delegate"), TEXT("function_name"), Label)
			|| TryReadNestedStringField(NodeObject, TEXT("component_event"), TEXT("delegate_property"), Label)
			|| TryReadNestedStringField(NodeObject, TEXT("timeline"), TEXT("name"), Label)
			|| TryReadNestedStringField(NodeObject, TEXT("comment"), TEXT("text"), Label))
		{
			return Label;
		}

		return NodeId;
	}

	FString ClassifyNode(const TSharedPtr<FJsonObject>& NodeObject, const FString& RawType)
	{
		const FString NormalizedType = NormalizeNodeTypeName(RawType);
		const FString TypeKey = NormalizeToken(NormalizedType);

		if (NodeObject->HasField(TEXT("comment")) || TypeKey.Contains(TEXT("comment")))
		{
			return TEXT("comment");
		}
		if (TypeKey.Contains(TEXT("ifthenelse")) || TypeKey.Contains(TEXT("branch")))
		{
			return TEXT("branch");
		}
		if (TypeKey.Contains(TEXT("switch")))
		{
			return TEXT("switch");
		}
		if (TypeKey.Contains(TEXT("executionsequence")) || TypeKey.Contains(TEXT("sequence")))
		{
			return TEXT("sequence");
		}
		if (TypeKey.Contains(TEXT("loop")) || TypeKey.Contains(TEXT("foreach")) || TypeKey.Contains(TEXT("while")))
		{
			return TEXT("loop");
		}
		if (TypeKey.Contains(TEXT("calldelegate")) || TypeKey.Contains(TEXT("broadcast")))
		{
			return TEXT("broadcast");
		}
		if (TypeKey.Contains(TEXT("adddelegate")) || TypeKey.Contains(TEXT("assigndelegate")) || TypeKey.Contains(TEXT("createdelegate")) || TypeKey.Contains(TEXT("binddelegate")))
		{
			return TEXT("bind_delegate");
		}
		if (TypeKey.Contains(TEXT("removedelegate")) || TypeKey.Contains(TEXT("cleardelegate")) || TypeKey.Contains(TEXT("unbinddelegate")))
		{
			return TEXT("unbind_delegate");
		}
		if (NodeObject->HasField(TEXT("timeline")) || TypeKey.Contains(TEXT("timeline")))
		{
			return TEXT("timeline");
		}
		if (NodeObject->HasField(TEXT("cast")) || TypeKey.Contains(TEXT("dynamiccast")) || TypeKey.Contains(TEXT("cast")))
		{
			return TEXT("cast");
		}
		if (TypeKey.Contains(TEXT("knot")) || TypeKey.Contains(TEXT("reroute")))
		{
			return TEXT("reroute");
		}
		if (NodeObject->HasField(TEXT("event"))
			|| NodeObject->HasField(TEXT("component_event"))
			|| NodeObject->HasField(TEXT("input_action_path"))
			|| TypeKey.Contains(TEXT("customevent"))
			|| TypeKey.Contains(TEXT("componentboundevent"))
			|| TypeKey.Contains(TEXT("enhancedinputaction"))
			|| TypeKey.Equals(TEXT("k2nodeevent"))
			|| TypeKey.Equals(TEXT("event")))
		{
			return TEXT("event");
		}
		if (NodeObject->HasField(TEXT("variable")) || TypeKey.Contains(TEXT("variableget")))
		{
			if (TypeKey.Contains(TEXT("variableset")))
			{
				return TEXT("set");
			}
			return TEXT("get");
		}
		if (TypeKey.Contains(TEXT("variableset")))
		{
			return TEXT("set");
		}
		if (ContainsAny(TypeKey, {TEXT("self"), TEXT("literal"), TEXT("getenumerator"), TEXT("getarrayitem")}))
		{
			return TEXT("get");
		}
		if (ContainsAny(TypeKey, {TEXT("callfunction"), TEXT("macroinstance"), TEXT("promotableoperator"), TEXT("commutativeassociativebinaryoperator"), TEXT("spawnactor"), TEXT("formattext"), TEXT("select")})
			|| NodeObject->HasField(TEXT("function_name"))
			|| NodeObject->HasField(TEXT("macro")))
		{
			return TEXT("call");
		}

		return TEXT("unknown");
	}

	void ResolveNodePosition(const TSharedPtr<FJsonObject>& NodeObject, FLogicNode& OutNode)
	{
		double X = 0.0;
		double Y = 0.0;
		if (NodeObject->TryGetNumberField(TEXT("x"), X) && NodeObject->TryGetNumberField(TEXT("y"), Y))
		{
			OutNode.X = X;
			OutNode.Y = Y;
			OutNode.bHasPosition = true;
			return;
		}

		const TSharedPtr<FJsonObject>* PositionObject = nullptr;
		if (NodeObject->TryGetObjectField(TEXT("position"), PositionObject)
			&& PositionObject
			&& PositionObject->IsValid()
			&& (*PositionObject)->TryGetNumberField(TEXT("x"), X)
			&& (*PositionObject)->TryGetNumberField(TEXT("y"), Y))
		{
			OutNode.X = X;
			OutNode.Y = Y;
			OutNode.bHasPosition = true;
		}
	}

	FLogicNode ParseNode(const TSharedPtr<FJsonObject>& NodeObject, int32 NodeIndex)
	{
		FLogicNode Node;
		Node.Id = ReadFirstStringField(NodeObject, TEXT("id"), TEXT("node_id"), TEXT("node_guid"));
		if (Node.Id.IsEmpty())
		{
			Node.Id = FString::Printf(TEXT("Node_%d"), NodeIndex);
		}

		TryReadStringField(NodeObject, TEXT("type"), Node.RawType);
		Node.Label = ResolveNodeLabel(NodeObject, Node.Id);
		Node.Category = ClassifyNode(NodeObject, Node.RawType);
		ResolveNodePosition(NodeObject, Node);
		return Node;
	}

	bool TryReadEndpointObjectField(const TSharedPtr<FJsonObject>& LinkObject, const TCHAR* ObjectField, const TCHAR* FieldName, FString& OutValue)
	{
		const TSharedPtr<FJsonObject>* EndpointObject = nullptr;
		if (LinkObject->TryGetObjectField(ObjectField, EndpointObject)
			&& EndpointObject
			&& EndpointObject->IsValid())
		{
			return TryReadStringField(*EndpointObject, FieldName, OutValue);
		}

		return false;
	}

	void ResolveEndpoint(const TSharedPtr<FJsonObject>& LinkObject, bool bSource, FString& OutNodeId, FString& OutPin)
	{
		if (bSource)
		{
			OutNodeId = ReadFirstStringField(LinkObject, TEXT("from_id"), TEXT("source_id"), TEXT("from_node"));
			if (OutNodeId.IsEmpty())
			{
				TryReadStringField(LinkObject, TEXT("source.node"), OutNodeId);
			}
			OutPin = ReadFirstStringField(LinkObject, TEXT("from_pin"), TEXT("source_pin"));
			if (OutPin.IsEmpty())
			{
				TryReadStringField(LinkObject, TEXT("source.pin"), OutPin);
			}
			if (OutNodeId.IsEmpty())
			{
				TryReadEndpointObjectField(LinkObject, TEXT("source"), TEXT("node"), OutNodeId)
					|| TryReadEndpointObjectField(LinkObject, TEXT("source"), TEXT("node_id"), OutNodeId)
					|| TryReadEndpointObjectField(LinkObject, TEXT("source"), TEXT("id"), OutNodeId);
			}
			if (OutPin.IsEmpty())
			{
				TryReadEndpointObjectField(LinkObject, TEXT("source"), TEXT("pin"), OutPin)
					|| TryReadEndpointObjectField(LinkObject, TEXT("source"), TEXT("pin_name"), OutPin);
			}
		}
		else
		{
			OutNodeId = ReadFirstStringField(LinkObject, TEXT("to_id"), TEXT("target_id"), TEXT("to_node"));
			if (OutNodeId.IsEmpty())
			{
				TryReadStringField(LinkObject, TEXT("target.node"), OutNodeId);
			}
			OutPin = ReadFirstStringField(LinkObject, TEXT("to_pin"), TEXT("target_pin"));
			if (OutPin.IsEmpty())
			{
				TryReadStringField(LinkObject, TEXT("target.pin"), OutPin);
			}
			if (OutNodeId.IsEmpty())
			{
				TryReadEndpointObjectField(LinkObject, TEXT("target"), TEXT("node"), OutNodeId)
					|| TryReadEndpointObjectField(LinkObject, TEXT("target"), TEXT("node_id"), OutNodeId)
					|| TryReadEndpointObjectField(LinkObject, TEXT("target"), TEXT("id"), OutNodeId);
			}
			if (OutPin.IsEmpty())
			{
				TryReadEndpointObjectField(LinkObject, TEXT("target"), TEXT("pin"), OutPin)
					|| TryReadEndpointObjectField(LinkObject, TEXT("target"), TEXT("pin_name"), OutPin);
			}
		}
	}

	FString NormalizeExplicitKind(const FString& RawKind)
	{
		const FString KindKey = NormalizeToken(RawKind);
		if (KindKey.Contains(TEXT("exec")) || KindKey.Contains(TEXT("execution")) || KindKey.Contains(TEXT("flow")) || KindKey.Contains(TEXT("control")))
		{
			return TEXT("exec");
		}
		if (KindKey.Contains(TEXT("data")) || KindKey.Contains(TEXT("value")) || KindKey.Contains(TEXT("dependency")) || KindKey.Contains(TEXT("property")))
		{
			return TEXT("data");
		}
		return TEXT("unknown");
	}

	bool TryReadPinTypeCategory(const TSharedPtr<FJsonObject>& Object, FString& OutCategory)
	{
		if (!Object.IsValid())
		{
			return false;
		}

		if (TryReadStringField(Object, TEXT("pin_type"), OutCategory)
			|| TryReadStringField(Object, TEXT("source_pin_type"), OutCategory)
			|| TryReadStringField(Object, TEXT("target_pin_type"), OutCategory)
			|| TryReadStringField(Object, TEXT("from_pin_type"), OutCategory)
			|| TryReadStringField(Object, TEXT("to_pin_type"), OutCategory))
		{
			return true;
		}

		const TSharedPtr<FJsonObject>* PinTypeObject = nullptr;
		if (Object->TryGetObjectField(TEXT("pin_type"), PinTypeObject)
			&& PinTypeObject
			&& PinTypeObject->IsValid()
			&& TryReadStringField(*PinTypeObject, TEXT("category"), OutCategory))
		{
			return true;
		}

		return false;
	}

	bool TryReadEndpointPinTypeCategory(const TSharedPtr<FJsonObject>& LinkObject, const TCHAR* EndpointField, FString& OutCategory)
	{
		const TSharedPtr<FJsonObject>* EndpointObject = nullptr;
		if (!LinkObject->TryGetObjectField(EndpointField, EndpointObject) || !EndpointObject || !EndpointObject->IsValid())
		{
			return false;
		}

		if (TryReadStringField(*EndpointObject, TEXT("pin_type"), OutCategory))
		{
			return true;
		}

		const TSharedPtr<FJsonObject>* PinTypeObject = nullptr;
		if ((*EndpointObject)->TryGetObjectField(TEXT("pin_type"), PinTypeObject)
			&& PinTypeObject
			&& PinTypeObject->IsValid())
		{
			return TryReadStringField(*PinTypeObject, TEXT("category"), OutCategory);
		}

		return false;
	}

	bool IsExecPinName(const FString& PinName)
	{
		const FString Key = NormalizeToken(PinName);
		return Key.Equals(TEXT("exec"))
			|| Key.Equals(TEXT("execute"))
			|| Key.Equals(TEXT("then"))
			|| Key.StartsWith(TEXT("then"))
			|| Key.Equals(TEXT("completed"))
			|| Key.Equals(TEXT("complete"))
			|| Key.Equals(TEXT("finished"))
			|| Key.Equals(TEXT("true"))
			|| Key.Equals(TEXT("false"))
			|| Key.Equals(TEXT("loopbody"))
			|| Key.Equals(TEXT("body"))
			|| Key.Equals(TEXT("castsucceeded"))
			|| Key.Equals(TEXT("castfailed"))
			|| Key.Equals(TEXT("valid"))
			|| Key.Equals(TEXT("notvalid"))
			|| Key.Equals(TEXT("isvalid"))
			|| Key.Equals(TEXT("isnotvalid"));
	}

	FLogicLink ParseLink(const TSharedPtr<FJsonObject>& LinkObject)
	{
		FLogicLink Link;
		ResolveEndpoint(LinkObject, true, Link.SourceNodeId, Link.SourcePin);
		ResolveEndpoint(LinkObject, false, Link.TargetNodeId, Link.TargetPin);

		FString ExplicitKind;
		if (TryReadStringField(LinkObject, TEXT("kind"), ExplicitKind) || TryReadStringField(LinkObject, TEXT("type"), ExplicitKind))
		{
			Link.Kind = NormalizeExplicitKind(ExplicitKind);
			Link.Confidence = Link.Kind == TEXT("unknown") ? TEXT("unknown") : TEXT("explicit");
			return Link;
		}

		FString PinTypeCategory;
		if (TryReadPinTypeCategory(LinkObject, PinTypeCategory)
			|| TryReadEndpointPinTypeCategory(LinkObject, TEXT("source"), PinTypeCategory)
			|| TryReadEndpointPinTypeCategory(LinkObject, TEXT("target"), PinTypeCategory))
		{
			Link.Kind = NormalizeToken(PinTypeCategory).Equals(TEXT("exec")) ? TEXT("exec") : TEXT("data");
			Link.Confidence = TEXT("explicit");
			return Link;
		}

		if (Link.SourceNodeId.IsEmpty() || Link.TargetNodeId.IsEmpty())
		{
			Link.Kind = TEXT("unknown");
			Link.Confidence = TEXT("unknown");
			return Link;
		}

		if (IsExecPinName(Link.SourcePin) || IsExecPinName(Link.TargetPin))
		{
			Link.Kind = TEXT("exec");
			Link.Confidence = TEXT("inferred");
		}
		else if (!Link.SourcePin.IsEmpty() || !Link.TargetPin.IsEmpty())
		{
			Link.Kind = TEXT("data");
			Link.Confidence = TEXT("inferred");
		}
		else
		{
			Link.Kind = TEXT("unknown");
			Link.Confidence = TEXT("unknown");
		}

		return Link;
	}

	const FLogicNode* FindNode(const FLogicGraph& Graph, const FString& NodeId)
	{
		const int32* NodeIndex = Graph.NodeIndexById.Find(NodeId);
		if (!NodeIndex || !Graph.Nodes.IsValidIndex(*NodeIndex))
		{
			return nullptr;
		}
		return &Graph.Nodes[*NodeIndex];
	}

	FString ResolveNodeReference(const FLogicGraph& Graph, const FString& NodeId, const FBlueprintHelperLogicOptions& Options)
	{
		const bool bDebug = Options.DetailLevel == EBlueprintHelperLogicDetailLevel::Debug;
		const FLogicNode* Node = FindNode(Graph, NodeId);
		if (!Node)
		{
			return NodeId.IsEmpty() ? TEXT("unknown") : NodeId;
		}

		if (Options.bIncludeNodeIds || bDebug)
		{
			return Node->Id;
		}

		return Node->Label.IsEmpty() ? Node->Id : Node->Label;
	}

	void AnalyzeGraph(FLogicGraph& Graph)
	{
		TSet<FString> LinkedNodeIds;
		TSet<FString> IncomingExecNodeIds;
		TSet<FString> OutgoingExecNodeIds;
		TSet<FString> EntryPointNodeIds;

		auto NoteEndpoint = [&Graph, &LinkedNodeIds](const FString& NodeId)
		{
			if (Graph.NodeIndexById.Contains(NodeId))
			{
				LinkedNodeIds.Add(NodeId);
			}
		};

		auto AnalyzeLinks = [&](const TArray<FLogicLink>& Links, bool bExec)
		{
			for (const FLogicLink& Link : Links)
			{
				NoteEndpoint(Link.SourceNodeId);
				NoteEndpoint(Link.TargetNodeId);

				if (bExec)
				{
					if (Graph.NodeIndexById.Contains(Link.SourceNodeId))
					{
						OutgoingExecNodeIds.Add(Link.SourceNodeId);
					}
					if (Graph.NodeIndexById.Contains(Link.TargetNodeId))
					{
						IncomingExecNodeIds.Add(Link.TargetNodeId);
					}
					if (Link.SourceNodeId.Equals(TEXT("__function_entry__"), ESearchCase::IgnoreCase)
						&& Graph.NodeIndexById.Contains(Link.TargetNodeId))
					{
						EntryPointNodeIds.Add(Link.TargetNodeId);
					}
				}
			}
		};

		AnalyzeLinks(Graph.ExecLinks, true);
		AnalyzeLinks(Graph.DataLinks, false);
		AnalyzeLinks(Graph.UnknownLinks, false);

		for (const FLogicNode& Node : Graph.Nodes)
		{
			if (Node.Category.Equals(TEXT("event"), ESearchCase::IgnoreCase)
				|| EntryPointNodeIds.Contains(Node.Id)
				|| (!IncomingExecNodeIds.Contains(Node.Id) && OutgoingExecNodeIds.Contains(Node.Id)))
			{
				EntryPointNodeIds.Add(Node.Id);
			}

			if (!LinkedNodeIds.Contains(Node.Id))
			{
				Graph.OrphanNodeIds.Add(Node.Id);
			}
		}

		for (const FLogicNode& Node : Graph.Nodes)
		{
			if (EntryPointNodeIds.Contains(Node.Id))
			{
				Graph.EntryPointNodeIds.Add(Node.Id);
			}
		}
	}

	FLogicGraph ParseGraph(const TSharedPtr<FJsonObject>& GraphObject, int32 GraphIndex)
	{
		FLogicGraph Graph;
		Graph.Name = ResolveGraphName(GraphObject, GraphIndex);

		const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
		if (GraphObject->TryGetArrayField(TEXT("nodes"), NodesArray) && NodesArray)
		{
			for (int32 NodeIndex = 0; NodeIndex < NodesArray->Num(); ++NodeIndex)
			{
				const TSharedPtr<FJsonObject>* NodeObject = nullptr;
				if (!(*NodesArray)[NodeIndex]->TryGetObject(NodeObject) || !NodeObject || !NodeObject->IsValid())
				{
					continue;
				}

				FLogicNode Node = ParseNode(*NodeObject, NodeIndex);
				Graph.NodeIndexById.Add(Node.Id, Graph.Nodes.Num());
				Graph.Nodes.Add(MoveTemp(Node));
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* LinksArray = nullptr;
		if (GraphObject->TryGetArrayField(TEXT("links"), LinksArray) && LinksArray)
		{
			for (const TSharedPtr<FJsonValue>& LinkValue : *LinksArray)
			{
				const TSharedPtr<FJsonObject>* LinkObject = nullptr;
				if (!LinkValue->TryGetObject(LinkObject) || !LinkObject || !LinkObject->IsValid())
				{
					continue;
				}

				FLogicLink Link = ParseLink(*LinkObject);
				if (Link.Kind.Equals(TEXT("exec"), ESearchCase::IgnoreCase))
				{
					Graph.ExecLinks.Add(MoveTemp(Link));
				}
				else if (Link.Kind.Equals(TEXT("data"), ESearchCase::IgnoreCase))
				{
					Graph.DataLinks.Add(MoveTemp(Link));
				}
				else
				{
					Graph.UnknownLinks.Add(MoveTemp(Link));
				}
			}
		}

		AnalyzeGraph(Graph);
		return Graph;
	}

	FLogicTotals CalculateTotals(const TArray<FLogicGraph>& Graphs)
	{
		FLogicTotals Totals;
		for (const FLogicGraph& Graph : Graphs)
		{
			Totals.NodeCount += Graph.Nodes.Num();
			Totals.ExecLinkCount += Graph.ExecLinks.Num();
			Totals.DataLinkCount += Graph.DataLinks.Num();
			Totals.EntryPointCount += Graph.EntryPointNodeIds.Num();
			Totals.OrphanNodeCount += Graph.OrphanNodeIds.Num();
			Totals.UnknownLinkCount += Graph.UnknownLinks.Num();
		}
		return Totals;
	}

	TSharedRef<FJsonObject> MakeStatsObject(const FLogicTotals& Totals)
	{
		TSharedRef<FJsonObject> StatsObject = MakeShared<FJsonObject>();
		StatsObject->SetNumberField(TEXT("nodes"), Totals.NodeCount);
		StatsObject->SetNumberField(TEXT("exec_links"), Totals.ExecLinkCount);
		StatsObject->SetNumberField(TEXT("data_links"), Totals.DataLinkCount);
		StatsObject->SetNumberField(TEXT("entry_points"), Totals.EntryPointCount);
		StatsObject->SetNumberField(TEXT("orphans"), Totals.OrphanNodeCount);
		if (Totals.UnknownLinkCount > 0)
		{
			StatsObject->SetNumberField(TEXT("unknown_links"), Totals.UnknownLinkCount);
		}
		return StatsObject;
	}

	TSharedRef<FJsonObject> MakeGraphStatsObject(const FLogicGraph& Graph)
	{
		FLogicTotals Totals;
		Totals.NodeCount = Graph.Nodes.Num();
		Totals.ExecLinkCount = Graph.ExecLinks.Num();
		Totals.DataLinkCount = Graph.DataLinks.Num();
		Totals.EntryPointCount = Graph.EntryPointNodeIds.Num();
		Totals.OrphanNodeCount = Graph.OrphanNodeIds.Num();
		Totals.UnknownLinkCount = Graph.UnknownLinks.Num();
		return MakeStatsObject(Totals);
	}

	TSharedRef<FJsonObject> MakeNodeObject(const FLogicNode& Node, const FBlueprintHelperLogicOptions& Options)
	{
		const bool bDebug = Options.DetailLevel == EBlueprintHelperLogicDetailLevel::Debug;
		TSharedRef<FJsonObject> NodeObject = MakeShared<FJsonObject>();
		NodeObject->SetStringField(TEXT("ref"), Node.Label.IsEmpty() ? Node.Id : Node.Label);
		NodeObject->SetStringField(TEXT("category"), Node.Category);

		if (Options.bIncludeNodeIds || bDebug)
		{
			NodeObject->SetStringField(TEXT("id"), Node.Id);
		}
		if (Options.bIncludeRawNodeTypes || bDebug)
		{
			NodeObject->SetStringField(TEXT("raw_type"), Node.RawType);
		}
		if (Options.bIncludePositions && Node.bHasPosition)
		{
			TSharedRef<FJsonObject> PositionObject = MakeShared<FJsonObject>();
			PositionObject->SetNumberField(TEXT("x"), Node.X);
			PositionObject->SetNumberField(TEXT("y"), Node.Y);
			NodeObject->SetObjectField(TEXT("position"), PositionObject);
		}

		return NodeObject;
	}

	TSharedRef<FJsonObject> MakeLinkObject(const FLogicGraph& Graph, const FLogicLink& Link, const FBlueprintHelperLogicOptions& Options)
	{
		const bool bDebug = Options.DetailLevel == EBlueprintHelperLogicDetailLevel::Debug;
		TSharedRef<FJsonObject> LinkObject = MakeShared<FJsonObject>();
		LinkObject->SetStringField(TEXT("from"), ResolveNodeReference(Graph, Link.SourceNodeId, Options));
		LinkObject->SetStringField(TEXT("from_pin"), Link.SourcePin);
		LinkObject->SetStringField(TEXT("to"), ResolveNodeReference(Graph, Link.TargetNodeId, Options));
		LinkObject->SetStringField(TEXT("to_pin"), Link.TargetPin);
		LinkObject->SetStringField(TEXT("kind"), Link.Kind);
		LinkObject->SetStringField(TEXT("confidence"), Link.Confidence);

		if (Options.bIncludeNodeIds || bDebug)
		{
			LinkObject->SetStringField(TEXT("from_id"), Link.SourceNodeId);
			LinkObject->SetStringField(TEXT("to_id"), Link.TargetNodeId);
		}

		return LinkObject;
	}

	TArray<TSharedPtr<FJsonValue>> MakeNodeRefArray(const FLogicGraph& Graph, const TArray<FString>& NodeIds, const FBlueprintHelperLogicOptions& Options)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		for (const FString& NodeId : NodeIds)
		{
			Values.Add(MakeShared<FJsonValueString>(ResolveNodeReference(Graph, NodeId, Options)));
		}
		return Values;
	}

	TArray<TSharedPtr<FJsonValue>> MakeLinkArray(const FLogicGraph& Graph, const TArray<FLogicLink>& Links, const FBlueprintHelperLogicOptions& Options)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		for (const FLogicLink& Link : Links)
		{
			Values.Add(MakeShared<FJsonValueObject>(MakeLinkObject(Graph, Link, Options)));
		}
		return Values;
	}

	FString BuildLogicJson(const TArray<FLogicGraph>& Graphs, const FLogicTotals& Totals, const FBlueprintHelperLogicOptions& Options)
	{
		TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
		RootObject->SetStringField(TEXT("version"), TEXT("1.0"));
		RootObject->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.LogicGraph"));
		RootObject->SetStringField(TEXT("source_schema"), TEXT("BlueprintHelper.JsonToBlueprint"));
		RootObject->SetObjectField(TEXT("stats"), MakeStatsObject(Totals));

		TArray<TSharedPtr<FJsonValue>> GraphValues;
		for (const FLogicGraph& Graph : Graphs)
		{
			TSharedRef<FJsonObject> GraphObject = MakeShared<FJsonObject>();
			GraphObject->SetStringField(TEXT("name"), Graph.Name);
			GraphObject->SetObjectField(TEXT("stats"), MakeGraphStatsObject(Graph));

			TArray<TSharedPtr<FJsonValue>> NodeValues;
			for (const FLogicNode& Node : Graph.Nodes)
			{
				NodeValues.Add(MakeShared<FJsonValueObject>(MakeNodeObject(Node, Options)));
			}
			GraphObject->SetArrayField(TEXT("nodes"), NodeValues);
			GraphObject->SetArrayField(TEXT("entry_points"), MakeNodeRefArray(Graph, Graph.EntryPointNodeIds, Options));
			GraphObject->SetArrayField(TEXT("exec_flow"), MakeLinkArray(Graph, Graph.ExecLinks, Options));

			if (Options.bIncludeDataDependencies)
			{
				GraphObject->SetArrayField(TEXT("data_dependencies"), MakeLinkArray(Graph, Graph.DataLinks, Options));
			}
			if (Graph.UnknownLinks.Num() > 0)
			{
				GraphObject->SetArrayField(TEXT("unknown_links"), MakeLinkArray(Graph, Graph.UnknownLinks, Options));
			}
			if (Options.bIncludeOrphanNodes)
			{
				GraphObject->SetArrayField(TEXT("orphans"), MakeNodeRefArray(Graph, Graph.OrphanNodeIds, Options));
			}

			GraphValues.Add(MakeShared<FJsonValueObject>(GraphObject));
		}
		RootObject->SetArrayField(TEXT("graphs"), GraphValues);

		FString OutputString;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
		FJsonSerializer::Serialize(RootObject, Writer);
		return OutputString;
	}

	void AppendLine(FString& Output, const FString& Line = TEXT(""))
	{
		Output += Line;
		Output += TEXT("\n");
	}

	FString FormatLinkMarkdown(const FLogicGraph& Graph, const FLogicLink& Link, const FBlueprintHelperLogicOptions& Options)
	{
		return FString::Printf(
			TEXT("%s.%s -> %s.%s (kind=%s, confidence=%s)"),
			*ResolveNodeReference(Graph, Link.SourceNodeId, Options),
			*Link.SourcePin,
			*ResolveNodeReference(Graph, Link.TargetNodeId, Options),
			*Link.TargetPin,
			*Link.Kind,
			*Link.Confidence);
	}

	void AppendNodeList(FString& Output, const FLogicGraph& Graph, const TArray<FString>& NodeIds, const FBlueprintHelperLogicOptions& Options)
	{
		if (NodeIds.Num() == 0)
		{
			AppendLine(Output, TEXT("- None"));
			return;
		}

		const bool bDebug = Options.DetailLevel == EBlueprintHelperLogicDetailLevel::Debug;
		for (const FString& NodeId : NodeIds)
		{
			const FLogicNode* Node = FindNode(Graph, NodeId);
			if (Node && bDebug)
			{
				AppendLine(Output, FString::Printf(TEXT("- %s [id=%s, raw_type=%s]"), *ResolveNodeReference(Graph, NodeId, Options), *Node->Id, *Node->RawType));
			}
			else
			{
				AppendLine(Output, FString::Printf(TEXT("- %s"), *ResolveNodeReference(Graph, NodeId, Options)));
			}
		}
	}

	void AppendLinkList(FString& Output, const FLogicGraph& Graph, const TArray<FLogicLink>& Links, const FBlueprintHelperLogicOptions& Options)
	{
		if (Links.Num() == 0)
		{
			AppendLine(Output, TEXT("- None"));
			return;
		}

		for (const FLogicLink& Link : Links)
		{
			AppendLine(Output, FString::Printf(TEXT("- %s"), *FormatLinkMarkdown(Graph, Link, Options)));
		}
	}

	void AppendDebugNodeList(FString& Output, const FLogicGraph& Graph)
	{
		if (Graph.Nodes.Num() == 0)
		{
			AppendLine(Output, TEXT("- None"));
			return;
		}

		for (const FLogicNode& Node : Graph.Nodes)
		{
			AppendLine(Output, FString::Printf(
				TEXT("- %s [id=%s, category=%s, raw_type=%s]"),
				*Node.Label,
				*Node.Id,
				*Node.Category,
				*Node.RawType));
		}
	}

	FString BuildMarkdown(const TArray<FLogicGraph>& Graphs, const FLogicTotals& Totals, const FBlueprintHelperLogicOptions& Options)
	{
		FString Output;
		AppendLine(Output, TEXT("# Logic Graph"));
		AppendLine(Output);
		AppendLine(Output, FString::Printf(
			TEXT("Nodes: %d | Exec Links: %d | Data Links: %d | Entry Points: %d | Orphans: %d"),
			Totals.NodeCount,
			Totals.ExecLinkCount,
			Totals.DataLinkCount,
			Totals.EntryPointCount,
			Totals.OrphanNodeCount));

		if (Totals.UnknownLinkCount > 0)
		{
			AppendLine(Output, FString::Printf(TEXT("Unknown Links: %d"), Totals.UnknownLinkCount));
		}

		for (const FLogicGraph& Graph : Graphs)
		{
			AppendLine(Output);
			AppendLine(Output, FString::Printf(TEXT("## Graph: %s"), *Graph.Name));
			AppendLine(Output, FString::Printf(
				TEXT("Nodes: %d | Exec Links: %d | Data Links: %d | Entry Points: %d | Orphans: %d"),
				Graph.Nodes.Num(),
				Graph.ExecLinks.Num(),
				Graph.DataLinks.Num(),
				Graph.EntryPointNodeIds.Num(),
				Graph.OrphanNodeIds.Num()));

			if (Options.DetailLevel == EBlueprintHelperLogicDetailLevel::Debug)
			{
				AppendLine(Output);
				AppendLine(Output, TEXT("### Nodes"));
				AppendDebugNodeList(Output, Graph);
			}

			AppendLine(Output);
			AppendLine(Output, TEXT("### Entry Points"));
			AppendNodeList(Output, Graph, Graph.EntryPointNodeIds, Options);

			AppendLine(Output);
			AppendLine(Output, TEXT("### Execution"));
			AppendLinkList(Output, Graph, Graph.ExecLinks, Options);

			if (Options.bIncludeDataDependencies)
			{
				AppendLine(Output);
				AppendLine(Output, TEXT("### Data Dependencies"));
				AppendLinkList(Output, Graph, Graph.DataLinks, Options);
			}

			if (Graph.UnknownLinks.Num() > 0)
			{
				AppendLine(Output);
				AppendLine(Output, TEXT("### Unknown Links"));
				AppendLinkList(Output, Graph, Graph.UnknownLinks, Options);
			}

			if (Options.bIncludeOrphanNodes)
			{
				AppendLine(Output);
				AppendLine(Output, TEXT("### Orphans"));
				AppendNodeList(Output, Graph, Graph.OrphanNodeIds, Options);
			}
		}

		return Output;
	}
}

FBlueprintHelperLogicResult FBlueprintHelperLogicProcessor::ProcessRawJson(
	const FString& RawJsonText,
	const FBlueprintHelperLogicOptions& Options)
{
	FBlueprintHelperLogicResult Result;

	if (RawJsonText.TrimStartAndEnd().IsEmpty())
	{
		Result.ErrorMessage = TEXT("JSON parse failed: input is empty.");
		return Result;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawJsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		Result.ErrorMessage = TEXT("JSON parse failed: input is not a valid JSON object.");
		return Result;
	}

	return ProcessRawJsonObject(RootObject, Options);
}

FBlueprintHelperLogicResult FBlueprintHelperLogicProcessor::ProcessRawJsonObject(
	const TSharedPtr<FJsonObject>& RawJsonObject,
	const FBlueprintHelperLogicOptions& Options)
{
	FBlueprintHelperLogicResult Result;

	if (!RawJsonObject.IsValid())
	{
		Result.ErrorMessage = TEXT("JSON parse failed: input is not a valid JSON object.");
		return Result;
	}

	const TSharedPtr<FJsonObject>& RootObject = RawJsonObject;

	TArray<FLogicGraph> Graphs;
	const TArray<TSharedPtr<FJsonValue>>* GraphsArray = nullptr;
	if (RootObject->TryGetArrayField(TEXT("graphs"), GraphsArray) && GraphsArray)
	{
		for (int32 GraphIndex = 0; GraphIndex < GraphsArray->Num(); ++GraphIndex)
		{
			const TSharedPtr<FJsonObject>* GraphObject = nullptr;
			if (!(*GraphsArray)[GraphIndex]->TryGetObject(GraphObject) || !GraphObject || !GraphObject->IsValid())
			{
				continue;
			}
			Graphs.Add(ParseGraph(*GraphObject, GraphIndex));
		}
	}
	else if (RootObject->HasField(TEXT("nodes")) || RootObject->HasField(TEXT("links")))
	{
		Graphs.Add(ParseGraph(RootObject, 0));
	}

	const FLogicTotals Totals = CalculateTotals(Graphs);
	Result.NodeCount = Totals.NodeCount;
	Result.ExecLinkCount = Totals.ExecLinkCount;
	Result.DataLinkCount = Totals.DataLinkCount;
	Result.EntryPointCount = Totals.EntryPointCount;
	Result.OrphanNodeCount = Totals.OrphanNodeCount;

	Result.OutputText = Options.Format == EBlueprintHelperLogicOutputFormat::Markdown
		? BuildMarkdown(Graphs, Totals, Options)
		: BuildLogicJson(Graphs, Totals, Options);
	Result.bSuccess = true;
	return Result;
}
