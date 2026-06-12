// BlueprintHelper shared logic read DTOs.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

enum class EBlueprintHelperLogicFormat : uint8
{
	LogicJson,
	RawJsonRef,
	RawJson
};

inline const TCHAR* LogicFormatToString(EBlueprintHelperLogicFormat Format)
{
	switch (Format)
	{
	case EBlueprintHelperLogicFormat::LogicJson:  return TEXT("logic_json");
	case EBlueprintHelperLogicFormat::RawJsonRef: return TEXT("raw_json_ref");
	case EBlueprintHelperLogicFormat::RawJson:    return TEXT("raw_json");
	default:                                      return TEXT("unknown");
	}
}

enum class EBlueprintHelperLogicScope : uint8
{
	Blueprint,
	TargetGraph,
	TargetFunction,
	TargetEvent,
	TargetCustomEvent,
	TargetBlock,
	MultiTarget
};

inline const TCHAR* LogicScopeToString(EBlueprintHelperLogicScope Scope)
{
	switch (Scope)
	{
	case EBlueprintHelperLogicScope::Blueprint:        return TEXT("blueprint");
	case EBlueprintHelperLogicScope::TargetGraph:      return TEXT("target_graph");
	case EBlueprintHelperLogicScope::TargetFunction:   return TEXT("target_function");
	case EBlueprintHelperLogicScope::TargetEvent:      return TEXT("target_event");
	case EBlueprintHelperLogicScope::TargetCustomEvent: return TEXT("target_custom_event");
	case EBlueprintHelperLogicScope::TargetBlock:      return TEXT("target_block");
	case EBlueprintHelperLogicScope::MultiTarget:      return TEXT("multi_target");
	default:                                           return TEXT("unknown");
	}
}

struct FBlueprintHelperLogicStats
{
	TOptional<int32> Graphs;
	TOptional<int32> Functions;
	TOptional<int32> Events;
	TOptional<int32> Targets;
	TOptional<int32> Groups;
	int32 Nodes = 0;
	int32 ExecLinks = 0;
	int32 DataLinks = 0;
	int32 OrphanNodes = 0;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("nodes"), Nodes);
		Json->SetNumberField(TEXT("exec_links"), ExecLinks);
		Json->SetNumberField(TEXT("data_links"), DataLinks);
		Json->SetNumberField(TEXT("orphan_nodes"), OrphanNodes);
		if (Graphs.IsSet()) { Json->SetNumberField(TEXT("graphs"), Graphs.GetValue()); }
		if (Functions.IsSet()) { Json->SetNumberField(TEXT("functions"), Functions.GetValue()); }
		if (Events.IsSet()) { Json->SetNumberField(TEXT("events"), Events.GetValue()); }
		if (Targets.IsSet()) { Json->SetNumberField(TEXT("targets"), Targets.GetValue()); }
		if (Groups.IsSet()) { Json->SetNumberField(TEXT("groups"), Groups.GetValue()); }
		return Json;
	}
};

enum class EBlueprintHelperLogicGroupType : uint8
{
	BlueprintHelperBlock,
	UserRegion,
	GlobalEventFlow,
	FunctionLikeRegion,
	OrphanGroup,
	Unknown
};

inline const TCHAR* LogicGroupTypeToString(EBlueprintHelperLogicGroupType Type)
{
	switch (Type)
	{
	case EBlueprintHelperLogicGroupType::BlueprintHelperBlock: return TEXT("blueprinthelper_block");
	case EBlueprintHelperLogicGroupType::UserRegion:          return TEXT("user_region");
	case EBlueprintHelperLogicGroupType::GlobalEventFlow:     return TEXT("global_event_flow");
	case EBlueprintHelperLogicGroupType::FunctionLikeRegion:  return TEXT("function_like_region");
	case EBlueprintHelperLogicGroupType::OrphanGroup:         return TEXT("orphan_group");
	case EBlueprintHelperLogicGroupType::Unknown:             return TEXT("unknown");
	default:                                                  return TEXT("unknown");
	}
}

enum class EBlueprintHelperLogicNodeKind : uint8
{
	FunctionEntry,
	Event,
	CustomEvent,
	CallFunction,
	Branch,
	Sequence,
	VariableGet,
	VariableSet,
	ComponentGet,
	Literal,
	Return,
	Macro,
	DelegateBind,
	DelegateCall,
	Unknown
};

inline const TCHAR* LogicNodeKindToString(EBlueprintHelperLogicNodeKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperLogicNodeKind::FunctionEntry: return TEXT("function");
	case EBlueprintHelperLogicNodeKind::Event:         return TEXT("event");
	case EBlueprintHelperLogicNodeKind::CustomEvent:   return TEXT("custom_event");
	case EBlueprintHelperLogicNodeKind::CallFunction:  return TEXT("call_function");
	case EBlueprintHelperLogicNodeKind::Branch:        return TEXT("branch");
	case EBlueprintHelperLogicNodeKind::Sequence:      return TEXT("sequence");
	case EBlueprintHelperLogicNodeKind::VariableGet:   return TEXT("variable_get");
	case EBlueprintHelperLogicNodeKind::VariableSet:   return TEXT("variable_set");
	case EBlueprintHelperLogicNodeKind::ComponentGet:  return TEXT("component_get");
	case EBlueprintHelperLogicNodeKind::Literal:       return TEXT("literal");
	case EBlueprintHelperLogicNodeKind::Return:        return TEXT("return");
	case EBlueprintHelperLogicNodeKind::Macro:         return TEXT("macro");
	case EBlueprintHelperLogicNodeKind::DelegateBind:  return TEXT("delegate_bind");
	case EBlueprintHelperLogicNodeKind::DelegateCall:  return TEXT("delegate_call");
	case EBlueprintHelperLogicNodeKind::Unknown:       return TEXT("unknown");
	default:                                           return TEXT("unknown");
	}
}

enum class EBlueprintHelperLogicLinkType : uint8
{
	Exec,
	Data
};

inline const TCHAR* LogicLinkTypeToString(EBlueprintHelperLogicLinkType Type)
{
	switch (Type)
	{
	case EBlueprintHelperLogicLinkType::Exec: return TEXT("exec");
	case EBlueprintHelperLogicLinkType::Data: return TEXT("data");
	default:                                  return TEXT("unknown");
	}
}

struct FBlueprintHelperLogicLink
{
	FString LinkRef;
	FString PinRef;
	EBlueprintHelperLogicLinkType Type = EBlueprintHelperLogicLinkType::Exec;
	FString Ownership;
	FString FromPin;
	FString ToNode;
	FString ToPin;
	FString AnchorType;
	FString AnchorRef;
	FString AnchorKind;
	FString AnchorLabel;
	TSharedPtr<FJsonObject> ExternalAnchor;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("link_ref"), LinkRef);
		if (!PinRef.IsEmpty()) { Json->SetStringField(TEXT("pin_ref"), PinRef); }
		Json->SetStringField(TEXT("type"), LogicLinkTypeToString(Type));
		if (!Ownership.IsEmpty()) { Json->SetStringField(TEXT("ownership"), Ownership); }
		Json->SetStringField(TEXT("from_pin"), FromPin);
		Json->SetStringField(TEXT("to_node"), ToNode);
		Json->SetStringField(TEXT("to_pin"), ToPin);
		if (!AnchorType.IsEmpty()) { Json->SetStringField(TEXT("anchor_type"), AnchorType); }
		if (!AnchorKind.IsEmpty()) { Json->SetStringField(TEXT("kind"), AnchorKind); }
		if (!AnchorLabel.IsEmpty()) { Json->SetStringField(TEXT("label"), AnchorLabel); }
		if (!AnchorRef.IsEmpty()) { Json->SetStringField(TEXT("anchor_ref"), AnchorRef); }
		if (ExternalAnchor.IsValid()) { Json->SetObjectField(TEXT("external_anchor"), ExternalAnchor); }
		return Json;
	}
};

struct FBlueprintHelperLogicNode
{
	FString NodeRef;
	EBlueprintHelperLogicNodeKind Kind = EBlueprintHelperLogicNodeKind::Unknown;
	FString Name;
	FString Owner;
	FString NodeComment;
	TSharedPtr<FJsonObject> Inputs;
	TSharedPtr<FJsonObject> InputDefaults;
	TSharedPtr<FJsonObject> Outputs;
	TArray<TSharedPtr<FJsonObject>> Pins;
	TArray<FBlueprintHelperLogicLink> Links;
	TSharedPtr<FJsonObject> ExternalAnchor;
	TArray<TSharedPtr<FJsonObject>> ExternalAnchors;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("node_ref"), NodeRef);
		Json->SetStringField(TEXT("kind"), LogicNodeKindToString(Kind));
		Json->SetStringField(TEXT("name"), Name);
		if (!Owner.IsEmpty()) { Json->SetStringField(TEXT("owner"), Owner); }
		if (!NodeComment.IsEmpty()) { Json->SetStringField(TEXT("node_comment"), NodeComment); }
		if (Inputs.IsValid()) { Json->SetObjectField(TEXT("inputs"), Inputs); }
		if (InputDefaults.IsValid()) { Json->SetObjectField(TEXT("input_defaults"), InputDefaults); }
		if (Outputs.IsValid()) { Json->SetObjectField(TEXT("outputs"), Outputs); }
		if (Pins.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> PinValues;
			for (const TSharedPtr<FJsonObject>& Pin : Pins)
			{
				if (Pin.IsValid())
				{
					PinValues.Add(MakeShared<FJsonValueObject>(Pin));
				}
			}
			if (PinValues.Num() > 0)
			{
				Json->SetArrayField(TEXT("pins"), PinValues);
			}
		}
		if (ExternalAnchor.IsValid()) { Json->SetObjectField(TEXT("external_anchor"), ExternalAnchor); }
		if (ExternalAnchors.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> AnchorValues;
			for (const TSharedPtr<FJsonObject>& Anchor : ExternalAnchors)
			{
				if (Anchor.IsValid())
				{
					AnchorValues.Add(MakeShared<FJsonValueObject>(Anchor));
				}
			}
			if (AnchorValues.Num() > 0)
			{
				Json->SetArrayField(TEXT("external_anchors"), AnchorValues);
			}
		}
		if (Links.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> LinkValues;
			for (const FBlueprintHelperLogicLink& Link : Links)
			{
				LinkValues.Add(MakeShared<FJsonValueObject>(Link.ToJson()));
			}
			Json->SetArrayField(TEXT("links"), LinkValues);
		}
		return Json;
	}
};

struct FBlueprintHelperLogicEntry
{
	EBlueprintHelperLogicNodeKind Kind = EBlueprintHelperLogicNodeKind::Unknown;
	FString Name;
	FString NodePath;
	FString NodeRef;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("kind"), LogicNodeKindToString(Kind));
		Json->SetStringField(TEXT("name"), Name);
		Json->SetStringField(TEXT("node_path"), NodePath);
		Json->SetStringField(TEXT("node_ref"), NodeRef);
		return Json;
	}
};

struct FBlueprintHelperLogicGroup
{
	EBlueprintHelperLogicGroupType GroupType = EBlueprintHelperLogicGroupType::Unknown;
	FString BlockId;
	FString GroupEntryNodePath;
	FString Name;
	FBlueprintHelperLogicEntry Entry;
	TArray<FBlueprintHelperLogicNode> Nodes;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("group_type"), LogicGroupTypeToString(GroupType));
		if (!BlockId.IsEmpty()) { Json->SetStringField(TEXT("block_id"), BlockId); }
		if (!GroupEntryNodePath.IsEmpty()) { Json->SetStringField(TEXT("group_entry_node_path"), GroupEntryNodePath); }
		if (!Name.IsEmpty()) { Json->SetStringField(TEXT("name"), Name); }
		Json->SetObjectField(TEXT("entry"), Entry.ToJson());
		TArray<TSharedPtr<FJsonValue>> NodeValues;
		for (const FBlueprintHelperLogicNode& Node : Nodes)
		{
			NodeValues.Add(MakeShared<FJsonValueObject>(Node.ToJson()));
		}
		Json->SetArrayField(TEXT("nodes"), NodeValues);
		return Json;
	}
};

struct FBlueprintHelperLogicJsonPayload
{
	FString AssetPath;
	FString Graph;
	FString Function;
	FString Event;
	FString BlockId;
	TOptional<FBlueprintHelperLogicEntry> Entry;
	TArray<FBlueprintHelperLogicNode> Nodes;
	TArray<FBlueprintHelperLogicGroup> Groups;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("asset_path"), AssetPath);
		if (!Graph.IsEmpty()) { Json->SetStringField(TEXT("graph"), Graph); }
		if (!Function.IsEmpty()) { Json->SetStringField(TEXT("function"), Function); }
		if (!Event.IsEmpty()) { Json->SetStringField(TEXT("event"), Event); }
		if (!BlockId.IsEmpty()) { Json->SetStringField(TEXT("block_id"), BlockId); }

		if (Groups.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> GroupValues;
			for (const FBlueprintHelperLogicGroup& Group : Groups)
			{
				GroupValues.Add(MakeShared<FJsonValueObject>(Group.ToJson()));
			}
			Json->SetArrayField(TEXT("groups"), GroupValues);
		}
		else if (Entry.IsSet())
		{
			Json->SetObjectField(TEXT("entry"), Entry->ToJson());
			TArray<TSharedPtr<FJsonValue>> NodeValues;
			for (const FBlueprintHelperLogicNode& Node : Nodes)
			{
				NodeValues.Add(MakeShared<FJsonValueObject>(Node.ToJson()));
			}
			Json->SetArrayField(TEXT("nodes"), NodeValues);
		}

		return Json;
	}
};

struct FBlueprintHelperLogicJsonData
{
	static constexpr const TCHAR* SchemaString = TEXT("LogicJson.v1");

	EBlueprintHelperLogicFormat Format = EBlueprintHelperLogicFormat::LogicJson;
	bool bImportable = false;
	EBlueprintHelperLogicScope Scope = EBlueprintHelperLogicScope::TargetGraph;
	FBlueprintHelperLogicJsonPayload Logic;
	FBlueprintHelperLogicStats Stats;
	TSharedPtr<FJsonObject> AdapterBoundaryJson;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), SchemaString);
		Json->SetStringField(TEXT("format"), LogicFormatToString(Format));
		Json->SetBoolField(TEXT("importable"), bImportable);
		Json->SetStringField(TEXT("scope"), LogicScopeToString(Scope));
		Json->SetObjectField(TEXT("logic"), Logic.ToJson());
		Json->SetObjectField(TEXT("stats"), Stats.ToJson());
		if (AdapterBoundaryJson.IsValid()) { Json->SetObjectField(TEXT("adapter_boundary"), AdapterBoundaryJson); }
		return Json;
	}
};
