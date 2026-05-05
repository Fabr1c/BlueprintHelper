// BlueprintHelper Service Layer — Diagnostics 类型定义
// 第 2 簇：只读诊断工具的数据类型

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// ─── 诊断模式枚举 ───

/** 诊断模式。 */
enum class EBlueprintHelperDiagnosticsMode : uint8
{
	Static,    // 不要求 UE Editor 运行，检查安装、配置、文件
	Runtime    // 要求 UE Editor Bridge，检查链路状态
};

/** DiagnosticsMode → MCP snake_case string。 */
inline const TCHAR* DiagnosticsModeToString(EBlueprintHelperDiagnosticsMode Mode)
{
	switch (Mode)
	{
	case EBlueprintHelperDiagnosticsMode::Static:  return TEXT("static");
	case EBlueprintHelperDiagnosticsMode::Runtime: return TEXT("runtime");
	default:                                       return TEXT("unknown");
	}
}

// ─── 诊断输出格式枚举 ───

/** 诊断输出格式。 */
enum class EBlueprintHelperDiagnosticsFormat : uint8
{
	Markdown
};

/** DiagnosticsFormat → MCP snake_case string。 */
inline const TCHAR* DiagnosticsFormatToString(EBlueprintHelperDiagnosticsFormat Format)
{
	switch (Format)
	{
	case EBlueprintHelperDiagnosticsFormat::Markdown: return TEXT("markdown");
	default:                                          return TEXT("unknown");
	}
}

// ─── 诊断代码行结构 ───

/** 单条诊断代码行，用于构建 Markdown 中的 - code 列表。 */
struct FBlueprintHelperDiagnosticsCodeLine
{
	/** 稳定 code，如 version.match。 */
	FString Code;

	/** 额外信息（如 reason / blocked_commands）。 */
	FString Extra;

	/** 格式化为 Markdown 行："- code\\n  - extra_line1\\n  - extra_line2"。 */
	FString ToMarkdown() const
	{
		FString Line = FString::Printf(TEXT("- %s"), *Code);
		if (!Extra.IsEmpty())
		{
			Line += TEXT("\n");
			TArray<FString> Parts;
			Extra.ParseIntoArray(Parts, TEXT("\n"), true);
			for (const FString& Part : Parts)
			{
				Line += FString::Printf(TEXT("  - %s\n"), *Part);
			}
		}
		return Line;
	}
};

// ─── Markdown 诊断报告 ───

/** Markdown 格式的诊断报告。保证 Blocking / Warning / Info 分段。 */
struct FBlueprintHelperDiagnosticsMarkdownReport
{
	/** Blocking 代码行集合。 */
	TArray<FBlueprintHelperDiagnosticsCodeLine> Blocking;

	/** Warning 代码行集合。 */
	TArray<FBlueprintHelperDiagnosticsCodeLine> Warnings;

	/** Info 代码行集合。 */
	TArray<FBlueprintHelperDiagnosticsCodeLine> Info;

	/** 生成 Markdown。 */
	FString ToMarkdown() const
	{
		FString Md;

		// Blocking
		Md += TEXT("## Blocking\n");
		if (Blocking.Num() == 0)
		{
			Md += TEXT("None\n");
		}
		else
		{
			for (const auto& Item : Blocking) { Md += Item.ToMarkdown() + TEXT("\n"); }
		}

		Md += TEXT("\n");

		// Warning
		Md += TEXT("## Warning\n");
		if (Warnings.Num() == 0)
		{
			Md += TEXT("None\n");
		}
		else
		{
			for (const auto& Item : Warnings) { Md += Item.ToMarkdown() + TEXT("\n"); }
		}

		// Info
		if (Info.Num() > 0)
		{
			Md += TEXT("\n## Info\n");
			for (const auto& Item : Info) { Md += Item.ToMarkdown() + TEXT("\n"); }
		}

		return Md;
	}

	/** 添加一条 Blocking。 */
	void AddBlocking(const FString& Code, const FString& Extra = TEXT(""))
	{
		Blocking.Add({ Code, Extra });
	}

	/** 添加一条 Warning。 */
	void AddWarning(const FString& Code, const FString& Extra = TEXT(""))
	{
		Warnings.Add({ Code, Extra });
	}

	/** 添加一条 Info。 */
	void AddInfo(const FString& Code, const FString& Extra = TEXT(""))
	{
		Info.Add({ Code, Extra });
	}
};

// ─── 8.2 FBlueprintHelperDiagnosticsData ───

/**
 * Diagnostics 实际数据。
 * 作为 FBlueprintHelperToolResultBase::Data 的 payload。
 */
struct FBlueprintHelperDiagnosticsData
{
	/** 固定为 BlueprintHelper.Diagnostics.v1。 */
	static constexpr const TCHAR* SchemaString = TEXT("Diagnostics.v1");

	/** 诊断模式。 */
	EBlueprintHelperDiagnosticsMode Mode = EBlueprintHelperDiagnosticsMode::Static;

	/** 输出格式，固定为 markdown。 */
	EBlueprintHelperDiagnosticsFormat Format = EBlueprintHelperDiagnosticsFormat::Markdown;

	/** Markdown 诊断报告。 */
	FString Markdown;

	/** 序列化到 JSON（即 data.* 的内容）。 */
	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), SchemaString);
		Json->SetStringField(TEXT("mode"), DiagnosticsModeToString(Mode));
		Json->SetStringField(TEXT("format"), DiagnosticsFormatToString(Format));
		Json->SetStringField(TEXT("markdown"), Markdown);
		return Json;
	}
};
