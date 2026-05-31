// BlueprintHelper Service Layer — LogicMD 类型定义
// 第 3 簇：Agent 默认蓝图逻辑阅读格式的类型

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// ─── 逻辑输出格式枚举 ───

/** 逻辑输出格式（与现有 LogicProcessor 对齐但用于新协议）。 */
enum class EBlueprintHelperLogicFormat : uint8
{
	LogicMd,
	LogicJson,
	RawJsonRef,
	RawJson
};

/** LogicFormat → MCP snake_case string。 */
inline const TCHAR* LogicFormatToString(EBlueprintHelperLogicFormat Format)
{
	switch (Format)
	{
	case EBlueprintHelperLogicFormat::LogicMd:    return TEXT("logic_md");
	case EBlueprintHelperLogicFormat::LogicJson:  return TEXT("logic_json");
	case EBlueprintHelperLogicFormat::RawJsonRef: return TEXT("raw_json_ref");
	case EBlueprintHelperLogicFormat::RawJson:    return TEXT("raw_json");
	default:                                      return TEXT("unknown");
	}
}

// ─── 逻辑作用域枚举 ───

/** LogicMD 读取的目标范围。 */
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

/** LogicScope → MCP snake_case string。 */
inline const TCHAR* LogicScopeToString(EBlueprintHelperLogicScope Scope)
{
	switch (Scope)
	{
	case EBlueprintHelperLogicScope::Blueprint:        return TEXT("blueprint");
	case EBlueprintHelperLogicScope::TargetGraph:      return TEXT("target_graph");
	case EBlueprintHelperLogicScope::TargetFunction:    return TEXT("target_function");
	case EBlueprintHelperLogicScope::TargetEvent:       return TEXT("target_event");
	case EBlueprintHelperLogicScope::TargetCustomEvent: return TEXT("target_custom_event");
	case EBlueprintHelperLogicScope::TargetBlock:       return TEXT("target_block");
	case EBlueprintHelperLogicScope::MultiTarget:       return TEXT("multi_target");
	default:                                            return TEXT("unknown");
	}
}

// ─── 6.5 LogicMD Stats ───

/**
 * LogicMD 统计数据。
 * 字段必须根据 scope 收敛 —— 不返回当前 scope 外层级统计。
 * 不适用字段直接省略（不返回 0）。
 */
struct FBlueprintHelperLogicMdStats
{
	/** 全蓝图图表数（仅 scope=blueprint）。 */
	TOptional<int32> Graphs;

	/** 全蓝图函数数（仅 scope=blueprint）。 */
	TOptional<int32> Functions;

	/** 事件/入口点数（scope=blueprint 或 target_graph）。 */
	TOptional<int32> Events;

	/** 多目标读取的目标数（仅 scope=multi_target）。 */
	TOptional<int32> Targets;

	/** 分组数（多入口 scope: target_graph/blueprint/multi_target）。 */
	TOptional<int32> Groups;

	/** 当前 scope 内节点数。始终出现。 */
	int32 Nodes = 0;

	/** 当前 scope 内执行连线数。始终出现。 */
	int32 ExecLinks = 0;

	/** 当前 scope 内数据连线数。始终出现。 */
	int32 DataLinks = 0;

	/** 当前 scope 内孤立节点数。始终出现。 */
	int32 OrphanNodes = 0;

	/** 序列化到 JSON（只输出已赋值字段）。 */
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

// ─── 6.2 FBlueprintHelperLogicMdData ───

/**
 * LogicMD 实际数据。
 * 作为 FBlueprintHelperToolResultBase::Data 的 payload。
 */
struct FBlueprintHelperLogicMdData
{
	/** 固定为短名 LogicMd.v1（全局短名规则）。 */
	static constexpr const TCHAR* SchemaString = TEXT("LogicMd.v1");

	/** 格式，固定为 logic_md。 */
	EBlueprintHelperLogicFormat Format = EBlueprintHelperLogicFormat::LogicMd;

	/** 始终为 false。LogicMD 不可用于导入。 */
	bool bImportable = false;

	/** 当前 LogicMD 覆盖范围。 */
	EBlueprintHelperLogicScope Scope = EBlueprintHelperLogicScope::TargetGraph;

	/** LogicMD 文本。 */
	FString Markdown;

	/** 统计字段（随 scope 收敛）。 */
	FBlueprintHelperLogicMdStats Stats;

	/** 是否为分组输出（多入口 scope 时为 true，单入口 scope 不返回此字段）。 */
	TOptional<bool> bGrouped;

	/** 序列化到 JSON（即 data.* 的内容）。 */
	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), SchemaString);
		Json->SetStringField(TEXT("format"), LogicFormatToString(Format));
		Json->SetBoolField(TEXT("importable"), bImportable);
		Json->SetStringField(TEXT("scope"), LogicScopeToString(Scope));
		Json->SetStringField(TEXT("markdown"), Markdown);
		Json->SetObjectField(TEXT("stats"), Stats.ToJson());
		if (bGrouped.IsSet()) { Json->SetBoolField(TEXT("grouped"), bGrouped.GetValue()); }
		return Json;
	}
};

// ═══════════════════════════════════════════════════════════
// 分组结构 — LogicMD / LogicJson 共用
// ═══════════════════════════════════════════════════════════

// ─── 分组类型枚举 ───

/** 逻辑分组类型。 */
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

// ─── 节点语义类型枚举 ───

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
	case EBlueprintHelperLogicNodeKind::Event:        return TEXT("event");
	case EBlueprintHelperLogicNodeKind::CustomEvent:  return TEXT("custom_event");
	case EBlueprintHelperLogicNodeKind::CallFunction: return TEXT("call_function");
	case EBlueprintHelperLogicNodeKind::Branch:       return TEXT("branch");
	case EBlueprintHelperLogicNodeKind::Sequence:     return TEXT("sequence");
	case EBlueprintHelperLogicNodeKind::VariableGet:  return TEXT("variable_get");
	case EBlueprintHelperLogicNodeKind::VariableSet:  return TEXT("variable_set");
	case EBlueprintHelperLogicNodeKind::ComponentGet: return TEXT("component_get");
	case EBlueprintHelperLogicNodeKind::Literal:      return TEXT("literal");
	case EBlueprintHelperLogicNodeKind::Return:       return TEXT("return");
	case EBlueprintHelperLogicNodeKind::Macro:        return TEXT("macro");
	case EBlueprintHelperLogicNodeKind::DelegateBind: return TEXT("delegate_bind");
	case EBlueprintHelperLogicNodeKind::DelegateCall: return TEXT("delegate_call");
	case EBlueprintHelperLogicNodeKind::Unknown:      return TEXT("unknown");
	default:                                          return TEXT("unknown");
	}
}

// ─── 链接类型枚举 ───

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
	default:                                 return TEXT("unknown");
	}
}

// ─── 7.6 逻辑链接 ───

/** 单条 outgoing 逻辑链接。放在 source node 内部。 */
struct FBlueprintHelperLogicLink
{
	FString LinkRef;
	FString PinRef;
	EBlueprintHelperLogicLinkType Type = EBlueprintHelperLogicLinkType::Exec;
	FString FromPin;
	FString ToNode;
	FString ToPin;
	TSharedPtr<FJsonObject> ExternalAnchor;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("link_ref"), LinkRef);
		if (!PinRef.IsEmpty()) { Json->SetStringField(TEXT("pin_ref"), PinRef); }
		Json->SetStringField(TEXT("type"), LogicLinkTypeToString(Type));
		Json->SetStringField(TEXT("from_pin"), FromPin);
		Json->SetStringField(TEXT("to_node"), ToNode);
		Json->SetStringField(TEXT("to_pin"), ToPin);
		if (ExternalAnchor.IsValid()) { Json->SetObjectField(TEXT("external_anchor"), ExternalAnchor); }
		return Json;
	}
};

// ─── 7.5 逻辑节点 ───

/** 逻辑节点。links 放在 source node 内部。普通 node 默认只返回 node_ref。 */
struct FBlueprintHelperLogicNode
{
	FString NodeRef;
	EBlueprintHelperLogicNodeKind Kind = EBlueprintHelperLogicNodeKind::Unknown;
	FString Name;
	FString Owner;
	TSharedPtr<FJsonObject> Inputs;
	TSharedPtr<FJsonObject> InputDefaults;
	TSharedPtr<FJsonObject> Outputs;
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
		if (Inputs.IsValid()) { Json->SetObjectField(TEXT("inputs"), Inputs); }
		if (InputDefaults.IsValid()) { Json->SetObjectField(TEXT("input_defaults"), InputDefaults); }
		if (Outputs.IsValid()) { Json->SetObjectField(TEXT("outputs"), Outputs); }
		if (ExternalAnchor.IsValid()) { Json->SetObjectField(TEXT("external_anchor"), ExternalAnchor); }
		if (ExternalAnchors.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> AnchorArr;
			for (const TSharedPtr<FJsonObject>& Anchor : ExternalAnchors)
			{
				if (Anchor.IsValid())
				{
					AnchorArr.Add(MakeShared<FJsonValueObject>(Anchor));
				}
			}
			if (AnchorArr.Num() > 0)
			{
				Json->SetArrayField(TEXT("external_anchors"), AnchorArr);
			}
		}
		if (Links.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const auto& L : Links) { Arr.Add(MakeShared<FJsonValueObject>(L.ToJson())); }
			Json->SetArrayField(TEXT("links"), Arr);
		}
		return Json;
	}
};

// ─── 7.4 逻辑入口 ───

/** 分组入口节点。entry.node_path 是完整路径，也是 node_ref/link_ref 的反推锚点。 */
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

// ─── 7.3 逻辑分组 ───

/** 一个逻辑分组，包含 entry + nodes。 */
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
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const auto& N : Nodes) { Arr.Add(MakeShared<FJsonValueObject>(N.ToJson())); }
		Json->SetArrayField(TEXT("nodes"), Arr);
		return Json;
	}
};

// ─── 7.2 LogicJson Payload ───

/** LogicJson 的 logic.* 内容。单入口 scope 使用 entry+nodes，多入口 scope 使用 groups[]。 */
struct FBlueprintHelperLogicJsonPayload
{
	FString AssetPath;
	FString Graph;
	FString Function;
	FString Event;
	FString BlockId;

	/** 单入口 scope 的入口。 */
	TOptional<FBlueprintHelperLogicEntry> Entry;

	/** 单入口 scope 的节点。 */
	TArray<FBlueprintHelperLogicNode> Nodes;

	/** 多入口 scope 的分组。 */
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
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const auto& G : Groups) { Arr.Add(MakeShared<FJsonValueObject>(G.ToJson())); }
			Json->SetArrayField(TEXT("groups"), Arr);
		}
		else if (Entry.IsSet())
		{
			Json->SetObjectField(TEXT("entry"), Entry->ToJson());
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const auto& N : Nodes) { Arr.Add(MakeShared<FJsonValueObject>(N.ToJson())); }
			Json->SetArrayField(TEXT("nodes"), Arr);
		}

		return Json;
	}
};

// ─── 7.1 LogicJson Data ───

/** LogicJson 实际 data payload。 */
struct FBlueprintHelperLogicJsonData
{
	static constexpr const TCHAR* SchemaString = TEXT("LogicJson.v1");

	EBlueprintHelperLogicFormat Format = EBlueprintHelperLogicFormat::LogicJson;
	bool bImportable = false;
	EBlueprintHelperLogicScope Scope = EBlueprintHelperLogicScope::TargetGraph;
	FBlueprintHelperLogicJsonPayload Logic;
	FBlueprintHelperLogicMdStats Stats;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), SchemaString);
		Json->SetStringField(TEXT("format"), LogicFormatToString(Format));
		Json->SetBoolField(TEXT("importable"), bImportable);
		Json->SetStringField(TEXT("scope"), LogicScopeToString(Scope));
		Json->SetObjectField(TEXT("logic"), Logic.ToJson());
		Json->SetObjectField(TEXT("stats"), Stats.ToJson());
		return Json;
	}
};
