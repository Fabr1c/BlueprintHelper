// BlueprintHelper Service Layer — 公共类型定义

#pragma once

#include "CoreMinimal.h"
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
	FString NodeId;
	FString NodeName;
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
		Items.Add(MoveTemp(Item));
		if (Severity == EBlueprintHelperDiagnosticSeverity::Error) { ++ErrorCount; }
		else if (Severity == EBlueprintHelperDiagnosticSeverity::Warning) { ++WarningCount; }
	}
};

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
