// BlueprintHelper Service Layer — 公共类型定义

#pragma once

#include "CoreMinimal.h"

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
	FString Message;
	FString NodeName;
	FString PinName;
};

/** 诊断消息集合。 */
struct FBlueprintHelperDiagnosticSet
{
	TArray<FBlueprintHelperDiagnosticItem> Items;
	int32 ErrorCount = 0;
	int32 WarningCount = 0;

	bool HasErrors() const { return ErrorCount > 0; }

	void Add(EBlueprintHelperDiagnosticSeverity Severity, const FString& Message, const FString& NodeName = TEXT(""))
	{
		Items.Add({Severity, Message, NodeName});
		if (Severity == EBlueprintHelperDiagnosticSeverity::Error) { ++ErrorCount; }
		else if (Severity == EBlueprintHelperDiagnosticSeverity::Warning) { ++WarningCount; }
	}
};

// ─── 导入 ───

/** 导入请求。 */
struct FBlueprintHelperImportRequest
{
	FBlueprintHelperGraphTarget Target;
	FString JsonText;
	bool bAutoCompile = false;
};

/** 导入结果。 */
struct FBlueprintHelperImportResult
{
	bool bSuccess = false;
	int32 GeneratedNodeCount = 0;
	int32 UnresolvedNodeCount = 0;
	TArray<FString> CreatedNodeNames;
	TArray<FString> UnresolvedNodeSummaries;
	FBlueprintHelperDiagnosticSet Diagnostics;

	FString GetSummaryText() const
	{
		if (bSuccess)
		{
			return FString::Printf(TEXT("导入成功：生成 %d 个节点，%d 个未匹配。"), GeneratedNodeCount, UnresolvedNodeCount);
		}
		return FString::Printf(TEXT("导入失败：%d 个错误。"), Diagnostics.ErrorCount);
	}
};

// ─── 导出 ───

/** 导出范围枚举。 */
enum class EBlueprintHelperExportScope : uint8
{
	/** 导出单个图表的节点和连线。 */
	SingleGraph,
	/** 导出完整蓝图（变量 + 函数签名 + 所有图表）。 */
	FullBlueprint
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
	FString JsonText;
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
