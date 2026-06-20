// BlueprintHelper Service Layer — 公共类型定义

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UObject/UnrealType.h"

class FJsonObject;

// ─── 图表定位 ───

/** 目标蓝图与图表的定位描述。 */
struct FBlueprintHelperGraphTarget
{
	/** 蓝图资产路径，例如 "/Game/BP/BP_Test.BP_Test"。为空时使用当前焦点蓝图。 */
	FString BlueprintPath;

	/** 图表名称，例如 "EventGraph"。为空时使用焦点图表或默认 EventGraph。 */
	FString GraphName;
};

/** Resolver fallback policy. Mutation paths must not infer target identity from editor focus. */
struct FBlueprintHelperResolvePolicy
{
	bool bAllowFocusedBlueprint = false;
	bool bAllowFocusedGraph = false;
	bool bDefaultToEventGraph = false;

	static FBlueprintHelperResolvePolicy Mutation()
	{
		return FBlueprintHelperResolvePolicy();
	}

	static FBlueprintHelperResolvePolicy FocusedEditorRead()
	{
		FBlueprintHelperResolvePolicy Policy;
		Policy.bAllowFocusedBlueprint = true;
		Policy.bAllowFocusedGraph = true;
		Policy.bDefaultToEventGraph = true;
		return Policy;
	}
};

// ─── 诊断信息 ───

/** 单条诊断消息的严重度。 */
enum class EBlueprintHelperDiagnosticSeverity : uint8
{
	Info,
	Warning,
	Error
};

/** 单条诊断消息。 */
struct FBlueprintHelperDiagnosticItem
{
	EBlueprintHelperDiagnosticSeverity Severity = EBlueprintHelperDiagnosticSeverity::Info;
	FString Code;
	FString Message;
	FString GraphName;
	FString NodeId;
	FString NodeName;
	FString NodeGuid;
	FString NodeTitle;
	FString NodeClass;
	FString ErrorType;
	FString BlockRef;
	FString TargetKey;
	FString CompileDiagnosticCorrelationKey;
	FString PinName;
	FString Field;
};

/** 诊断消息集合。 */
struct FBlueprintHelperDiagnosticSet
{
	TArray<FBlueprintHelperDiagnosticItem> Items;
	int32 ErrorCount = 0;
	int32 WarningCount = 0;

	bool HasErrors() const { return ErrorCount > 0; }

	void AddItem(FBlueprintHelperDiagnosticItem Item)
	{
		const EBlueprintHelperDiagnosticSeverity Severity = Item.Severity;
		Items.Add(MoveTemp(Item));
		if (Severity == EBlueprintHelperDiagnosticSeverity::Error) { ++ErrorCount; }
		else if (Severity == EBlueprintHelperDiagnosticSeverity::Warning) { ++WarningCount; }
	}

	void Add(
		EBlueprintHelperDiagnosticSeverity Severity,
		const FString& Message,
		const FString& NodeName = TEXT(""),
		const FString& Code = TEXT(""),
		const FString& Field = TEXT(""),
		const FString& PinName = TEXT(""),
		const FString& NodeId = TEXT(""))
	{
		FBlueprintHelperDiagnosticItem Item;
		Item.Severity = Severity;
		Item.Code = Code;
		Item.Message = Message;
		Item.NodeId = NodeId;
		Item.NodeName = NodeName;
		Item.PinName = PinName;
		Item.Field = Field;
		AddItem(MoveTemp(Item));
	}
};

inline const TCHAR* BlueprintHelperDiagnosticSeverityToString(EBlueprintHelperDiagnosticSeverity Severity)
{
	switch (Severity)
	{
	case EBlueprintHelperDiagnosticSeverity::Error:
		return TEXT("error");
	case EBlueprintHelperDiagnosticSeverity::Warning:
		return TEXT("warning");
	default:
		return TEXT("info");
	}
}

inline EBlueprintHelperDiagnosticSeverity BlueprintHelperDiagnosticSeverityFromString(const FString& Severity)
{
	if (Severity.Equals(TEXT("error"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperDiagnosticSeverity::Error;
	}
	if (Severity.Equals(TEXT("warning"), ESearchCase::IgnoreCase))
	{
		return EBlueprintHelperDiagnosticSeverity::Warning;
	}
	return EBlueprintHelperDiagnosticSeverity::Info;
}

inline void BlueprintHelperSetDiagnosticStringField(
	const TSharedRef<FJsonObject>& Json,
	const TCHAR* FieldName,
	const FString& Value)
{
	if (!Value.IsEmpty())
	{
		Json->SetStringField(FieldName, Value);
	}
}

inline FString BlueprintHelperDiagnosticCorrelationKey(const FBlueprintHelperDiagnosticItem& Item)
{
	if (!Item.CompileDiagnosticCorrelationKey.IsEmpty())
	{
		return Item.CompileDiagnosticCorrelationKey;
	}
	if (!Item.GraphName.IsEmpty() && !Item.NodeGuid.IsEmpty())
	{
		return FString::Printf(TEXT("%s:%s"), *Item.GraphName, *Item.NodeGuid);
	}
	return FString();
}

inline TSharedRef<FJsonObject> BlueprintHelperDiagnosticItemToJson(const FBlueprintHelperDiagnosticItem& Item)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("severity"), BlueprintHelperDiagnosticSeverityToString(Item.Severity));
	BlueprintHelperSetDiagnosticStringField(Json, TEXT("code"), Item.Code);
	BlueprintHelperSetDiagnosticStringField(Json, TEXT("message"), Item.Message);
	BlueprintHelperSetDiagnosticStringField(Json, TEXT("graph_name"), Item.GraphName);
	BlueprintHelperSetDiagnosticStringField(Json, TEXT("node_id"), Item.NodeId);
	BlueprintHelperSetDiagnosticStringField(Json, TEXT("node_name"), Item.NodeName);
	BlueprintHelperSetDiagnosticStringField(Json, TEXT("node_guid"), Item.NodeGuid);
	BlueprintHelperSetDiagnosticStringField(Json, TEXT("node_title"), Item.NodeTitle);
	BlueprintHelperSetDiagnosticStringField(Json, TEXT("node_class"), Item.NodeClass);
	BlueprintHelperSetDiagnosticStringField(Json, TEXT("error_type"), Item.ErrorType);
	BlueprintHelperSetDiagnosticStringField(Json, TEXT("block_ref"), Item.BlockRef);
	BlueprintHelperSetDiagnosticStringField(Json, TEXT("target_key"), Item.TargetKey);
	BlueprintHelperSetDiagnosticStringField(
		Json,
		TEXT("compile_diagnostic_correlation_key"),
		BlueprintHelperDiagnosticCorrelationKey(Item));
	BlueprintHelperSetDiagnosticStringField(Json, TEXT("pin_name"), Item.PinName);
	BlueprintHelperSetDiagnosticStringField(Json, TEXT("field"), Item.Field);
	return Json;
}

inline TArray<TSharedPtr<FJsonValue>> BlueprintHelperDiagnosticItemsToJsonArray(
	const TArray<FBlueprintHelperDiagnosticItem>& Items)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	for (const FBlueprintHelperDiagnosticItem& Item : Items)
	{
		Values.Add(MakeShared<FJsonValueObject>(BlueprintHelperDiagnosticItemToJson(Item)));
	}
	return Values;
}

inline bool BlueprintHelperDiagnosticItemFromJson(
	const TSharedPtr<FJsonObject>& Json,
	FBlueprintHelperDiagnosticItem& OutItem)
{
	if (!Json.IsValid())
	{
		return false;
	}

	FString Severity;
	Json->TryGetStringField(TEXT("severity"), Severity);
	OutItem.Severity = BlueprintHelperDiagnosticSeverityFromString(Severity);
	Json->TryGetStringField(TEXT("code"), OutItem.Code);
	Json->TryGetStringField(TEXT("message"), OutItem.Message);
	Json->TryGetStringField(TEXT("graph_name"), OutItem.GraphName);
	Json->TryGetStringField(TEXT("node_id"), OutItem.NodeId);
	Json->TryGetStringField(TEXT("node_name"), OutItem.NodeName);
	Json->TryGetStringField(TEXT("node_guid"), OutItem.NodeGuid);
	Json->TryGetStringField(TEXT("node_title"), OutItem.NodeTitle);
	Json->TryGetStringField(TEXT("node_class"), OutItem.NodeClass);
	Json->TryGetStringField(TEXT("error_type"), OutItem.ErrorType);
	Json->TryGetStringField(TEXT("block_ref"), OutItem.BlockRef);
	Json->TryGetStringField(TEXT("target_key"), OutItem.TargetKey);
	Json->TryGetStringField(
		TEXT("compile_diagnostic_correlation_key"),
		OutItem.CompileDiagnosticCorrelationKey);
	Json->TryGetStringField(TEXT("pin_name"), OutItem.PinName);
	Json->TryGetStringField(TEXT("field"), OutItem.Field);
	if (OutItem.CompileDiagnosticCorrelationKey.IsEmpty())
	{
		OutItem.CompileDiagnosticCorrelationKey = BlueprintHelperDiagnosticCorrelationKey(OutItem);
	}

	return !OutItem.Code.IsEmpty() ||
		!OutItem.Message.IsEmpty() ||
		!OutItem.GraphName.IsEmpty() ||
		!OutItem.NodeGuid.IsEmpty() ||
		!OutItem.CompileDiagnosticCorrelationKey.IsEmpty() ||
		!OutItem.TargetKey.IsEmpty() ||
		!OutItem.BlockRef.IsEmpty();
}

inline void BlueprintHelperReadDiagnosticArrayField(
	const TSharedPtr<FJsonObject>& Json,
	const TCHAR* FieldName,
	TArray<FBlueprintHelperDiagnosticItem>& OutItems)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Json.IsValid() || !Json->TryGetArrayField(FieldName, Values) || !Values)
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		FBlueprintHelperDiagnosticItem Item;
		if (BlueprintHelperDiagnosticItemFromJson(Value.IsValid() ? Value->AsObject() : nullptr, Item))
		{
			OutItems.Add(MoveTemp(Item));
		}
	}
}

// ─── 属性写入策略 ───

/** 编辑器资产写入只允许普通可编辑属性，拒绝只读、常量和临时属性。 */
struct FBlueprintHelperEditablePropertyPolicy
{
	static bool AllowsWrite(const FProperty* Property)
	{
		return Property
			&& Property->HasAnyPropertyFlags(CPF_Edit)
			&& !Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly | CPF_EditConst | CPF_Transient);
	}

	static bool AllowsClassDefaultWrite(const FProperty* Property)
	{
		return Property
			&& Property->HasAnyPropertyFlags(CPF_Edit)
			&& !Property->HasAnyPropertyFlags(CPF_EditConst | CPF_Transient | CPF_DisableEditOnTemplate);
	}

	static bool AllowsInstanceWrite(const FProperty* Property)
	{
		return Property
			&& Property->HasAnyPropertyFlags(CPF_Edit)
			&& !Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly | CPF_EditConst | CPF_Transient | CPF_DisableEditOnInstance);
	}

	static bool AllowsGraphWrite(const FProperty* Property)
	{
		return Property
			&& Property->HasAnyPropertyFlags(CPF_BlueprintVisible)
			&& !Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly | CPF_EditConst | CPF_Transient);
	}

	static FString BuildFlagsSummary(uint64 PropertyFlags)
	{
		TArray<FString> Tags;
		if (PropertyFlags & CPF_Edit)              Tags.Add(TEXT("Edit"));
		if (PropertyFlags & CPF_BlueprintVisible)  Tags.Add(TEXT("BlueprintVisible"));
		if (PropertyFlags & CPF_BlueprintReadOnly) Tags.Add(TEXT("BlueprintReadOnly"));
		if (PropertyFlags & CPF_EditConst)         Tags.Add(TEXT("EditConst"));
		if (PropertyFlags & CPF_Config)            Tags.Add(TEXT("Config"));
		if (PropertyFlags & CPF_Transient)         Tags.Add(TEXT("Transient"));
		if (PropertyFlags & CPF_SaveGame)          Tags.Add(TEXT("SaveGame"));
		return FString::Join(Tags, TEXT(", "));
	}
};

// ─── 导入 ───

/** 导入请求。 */
struct FBlueprintHelperImportRequest
{
	FBlueprintHelperGraphTarget Target;
	TSharedPtr<FJsonObject> JsonObject;
	bool bAutoCompile = false;
	bool bStrict = true;
	bool bAllowPartial = false;
};

/** 导入结果。 */
struct FBlueprintHelperImportResult
{
	bool bSuccess = false;
	FString Status;
	int32 GeneratedNodeCount = 0;
	int32 UnresolvedNodeCount = 0;
	int32 OperationsApplied = 0;
	int32 LinksConnected = 0;
	bool bRolledBack = false;
	TArray<FString> CreatedNodeNames;
	TArray<FString> UnresolvedNodeSummaries;
	FBlueprintHelperDiagnosticSet Diagnostics;

	FString GetSummaryText() const
	{
		if (bSuccess)
		{
			return FString::Printf(TEXT("导入成功：状态 %s，生成 %d 个节点，%d 个未匹配。"),
				*Status, GeneratedNodeCount, UnresolvedNodeCount);
		}
		return FString::Printf(TEXT("导入失败：状态 %s，%d 个错误。"), *Status, Diagnostics.ErrorCount);
	}
};

// ─── 导出 ───

/** 导出范围枚举。 */
enum class EBlueprintHelperExportScope : uint8
{
	/** 导出单个图表的节点和连线。 */
	SingleGraph,
	/** 导出完整蓝图（变量 + 函数签名 + 所有图表）。 */
	FullBlueprint,
	/** 导出当前选择集。当前服务层会按图表导出并返回 effective_scope。 */
	Selection
};

/** 导出请求。 */
struct FBlueprintHelperExportRequest
{
	FBlueprintHelperGraphTarget Target;
	EBlueprintHelperExportScope Scope = EBlueprintHelperExportScope::SingleGraph;
};

/** 导出结果。 */
struct FBlueprintHelperExportResult
{
	bool bSuccess = false;
	TSharedPtr<FJsonObject> JsonObject;
	FString EffectiveScope;
	FBlueprintHelperDiagnosticSet Diagnostics;
};

// ─── 校验 ───

/** 校验结果。 */
struct FBlueprintHelperValidationResult
{
	bool bValid = false;
	FString DetectedVersion;
	FBlueprintHelperDiagnosticSet Diagnostics;
};

// ─── 编译 ───

/** 编译结果。 */
struct FBlueprintHelperCompileResult
{
	bool bSuccess = false;
	/** 使用 int32 存储 EBlueprintStatus，避免在纯类型头文件中引入引擎头文件。 */
	int32 BlueprintStatus = 0;
	FBlueprintHelperDiagnosticSet Diagnostics;
};
