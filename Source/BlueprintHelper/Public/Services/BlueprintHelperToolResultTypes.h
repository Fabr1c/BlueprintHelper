// BlueprintHelper Service Layer — Tool Result Base 公共类型定义
// 第 0 簇：统一所有 MCP 工具返回体的基础字段与类型映射

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Services/BlueprintHelperServiceTypes.h"

// ─── 协议常量 ───

namespace BlueprintHelperProtocol
{
	static const TCHAR* ToolResultSchema = TEXT("BlueprintHelper.McpToolResult.v1");
	namespace Status { static const TCHAR* Completed = TEXT("completed"); static const TCHAR* Applied = TEXT("applied"); static const TCHAR* NoOp = TEXT("no_op"); static const TCHAR* DryRun = TEXT("dry_run"); static const TCHAR* Failed = TEXT("failed"); }
	namespace DryRunResult { static const TCHAR* Passed = TEXT("passed"); static const TCHAR* Blocked = TEXT("blocked"); static const TCHAR* Failed = TEXT("failed"); }
}

// ─── 状态枚举 ───

/** 工具调用结果状态。 */
enum class EBlueprintHelperToolStatus : uint8
{
	Completed,      // 只读、编译、保存、验证等已完成
	Applied,        // 写操作已应用
	DryRun,         // 只执行预检，未修改资产
	Failed,         // 失败
	NoOp,           // 成功但无需修改
	Skipped,        // 被策略跳过
	RolledBack      // 已回滚
};

/** ToolStatus → MCP snake_case string。 */
inline const TCHAR* ToolStatusToString(EBlueprintHelperToolStatus Status)
{
	switch (Status)
	{
	case EBlueprintHelperToolStatus::Completed:  return TEXT("completed");
	case EBlueprintHelperToolStatus::Applied:    return TEXT("applied");
	case EBlueprintHelperToolStatus::DryRun:     return TEXT("dry_run");
	case EBlueprintHelperToolStatus::Failed:     return TEXT("failed");
	case EBlueprintHelperToolStatus::NoOp:       return TEXT("no_op");
	case EBlueprintHelperToolStatus::Skipped:    return TEXT("skipped");
	case EBlueprintHelperToolStatus::RolledBack: return TEXT("rolled_back");
	default:                                     return TEXT("unknown");
	}
}

// ─── 目标类型枚举 ───

/** 工具操作的目标类型。 */
enum class EBlueprintHelperTargetType : uint8
{
	None,
	Asset,
	Blueprint,
	Graph,
	Function,
	Event,
	CustomEvent,
	Block,
	Node,
	Pin,
	Link,
	Component,
	Property,
	MappingContext,
	DataTable,
	DataTableRow,
	Widget
};

/** TargetType → MCP snake_case string。 */
inline const TCHAR* TargetTypeToString(EBlueprintHelperTargetType Type)
{
	switch (Type)
	{
	case EBlueprintHelperTargetType::None:           return TEXT("none");
	case EBlueprintHelperTargetType::Asset:          return TEXT("asset");
	case EBlueprintHelperTargetType::Blueprint:      return TEXT("blueprint");
	case EBlueprintHelperTargetType::Graph:          return TEXT("graph");
	case EBlueprintHelperTargetType::Function:       return TEXT("function");
	case EBlueprintHelperTargetType::Event:          return TEXT("event");
	case EBlueprintHelperTargetType::CustomEvent:    return TEXT("custom_event");
	case EBlueprintHelperTargetType::Block:          return TEXT("block");
	case EBlueprintHelperTargetType::Node:           return TEXT("node");
	case EBlueprintHelperTargetType::Pin:            return TEXT("pin");
	case EBlueprintHelperTargetType::Link:           return TEXT("link");
	case EBlueprintHelperTargetType::Component:      return TEXT("component");
	case EBlueprintHelperTargetType::Property:       return TEXT("property");
	case EBlueprintHelperTargetType::MappingContext: return TEXT("mapping_context");
	case EBlueprintHelperTargetType::DataTable:      return TEXT("data_table");
	case EBlueprintHelperTargetType::DataTableRow:   return TEXT("data_table_row");
	case EBlueprintHelperTargetType::Widget:         return TEXT("widget");
	default:                                         return TEXT("unknown");
	}
}

// ─── 风险等级枚举 ───

/** 写操作风险等级。不等于 SafetyProfile。 */
enum class EBlueprintHelperRiskLevel : uint8
{
	None,
	Low,
	Medium,
	High,
	Destructive
};

/** RiskLevel → MCP snake_case string。 */
inline const TCHAR* RiskLevelToString(EBlueprintHelperRiskLevel Level)
{
	switch (Level)
	{
	case EBlueprintHelperRiskLevel::None:        return TEXT("none");
	case EBlueprintHelperRiskLevel::Low:         return TEXT("low");
	case EBlueprintHelperRiskLevel::Medium:      return TEXT("medium");
	case EBlueprintHelperRiskLevel::High:        return TEXT("high");
	case EBlueprintHelperRiskLevel::Destructive: return TEXT("destructive");
	default:                                     return TEXT("unknown");
	}
}

// ─── 事务状态枚举 ───

/** 事务状态。 */
enum class EBlueprintHelperTransactionStatus : uint8
{
	Applied,
	Failed,
	RolledBack,
	NoOp
};

/** TransactionStatus → MCP snake_case string。 */
inline const TCHAR* TransactionStatusToString(EBlueprintHelperTransactionStatus Status)
{
	switch (Status)
	{
	case EBlueprintHelperTransactionStatus::Applied:    return TEXT("applied");
	case EBlueprintHelperTransactionStatus::Failed:     return TEXT("failed");
	case EBlueprintHelperTransactionStatus::RolledBack: return TEXT("rolled_back");
	case EBlueprintHelperTransactionStatus::NoOp:       return TEXT("no_op");
	default:                                            return TEXT("unknown");
	}
}

// ─── 审阅状态枚举 ───

/** Review 状态。 */
enum class EBlueprintHelperReviewStatus : uint8
{
	None,
	Pending,
	Accepted,
	Rejected,
	RolledBack,
	Archived
};

/** ReviewStatus → MCP snake_case string。 */
inline const TCHAR* ReviewStatusToString(EBlueprintHelperReviewStatus Status)
{
	switch (Status)
	{
	case EBlueprintHelperReviewStatus::None:       return TEXT("none");
	case EBlueprintHelperReviewStatus::Pending:    return TEXT("pending");
	case EBlueprintHelperReviewStatus::Accepted:   return TEXT("accepted");
	case EBlueprintHelperReviewStatus::Rejected:   return TEXT("rejected");
	case EBlueprintHelperReviewStatus::RolledBack: return TEXT("rolled_back");
	case EBlueprintHelperReviewStatus::Archived:   return TEXT("archived");
	default:                                       return TEXT("unknown");
	}
}

// ─── 严重度枚举 ───

/** Validation 消息严重度。 */
enum class EBlueprintHelperValidationSeverity : uint8
{
	Info,
	Warning,
	Error
};

/** ValidationSeverity → MCP snake_case string。 */
inline const TCHAR* ValidationSeverityToString(EBlueprintHelperValidationSeverity Severity)
{
	switch (Severity)
	{
	case EBlueprintHelperValidationSeverity::Info:    return TEXT("info");
	case EBlueprintHelperValidationSeverity::Warning: return TEXT("warning");
	case EBlueprintHelperValidationSeverity::Error:   return TEXT("error");
	default:                                          return TEXT("unknown");
	}
}

// ─── 工具阶段枚举 ───

/** 工具执行阶段。 */
enum class EBlueprintHelperToolStage : uint8
{
	ParseInput,
	Auth,
	RuntimeProfile,
	ResolveTarget,
	Preflight,
	DryRun,
	Execute,
	Validate,
	Review,
	Rollback,
	Bridge,
	McpWrap
};

/** ToolStage → MCP snake_case string。 */
inline const TCHAR* ToolStageToString(EBlueprintHelperToolStage Stage)
{
	switch (Stage)
	{
	case EBlueprintHelperToolStage::ParseInput:     return TEXT("parse_input");
	case EBlueprintHelperToolStage::Auth:           return TEXT("auth");
	case EBlueprintHelperToolStage::RuntimeProfile: return TEXT("runtime_profile");
	case EBlueprintHelperToolStage::ResolveTarget:  return TEXT("resolve_target");
	case EBlueprintHelperToolStage::Preflight:      return TEXT("preflight");
	case EBlueprintHelperToolStage::DryRun:         return TEXT("dry_run");
	case EBlueprintHelperToolStage::Execute:        return TEXT("execute");
	case EBlueprintHelperToolStage::Validate:       return TEXT("validate");
	case EBlueprintHelperToolStage::Review:         return TEXT("review");
	case EBlueprintHelperToolStage::Rollback:       return TEXT("rollback");
	case EBlueprintHelperToolStage::Bridge:         return TEXT("bridge");
	case EBlueprintHelperToolStage::McpWrap:        return TEXT("mcp_wrap");
	default:                                        return TEXT("unknown");
	}
}

// ─── 回滚结果枚举 ───

/** 回滚结果。 */
enum class EBlueprintHelperRollbackResult : uint8
{
	NotNeeded,
	RolledBack,
	Blocked,
	Failed,
	Unavailable
};

/** RollbackResult → MCP snake_case string。 */
inline const TCHAR* RollbackResultToString(EBlueprintHelperRollbackResult Result)
{
	switch (Result)
	{
	case EBlueprintHelperRollbackResult::NotNeeded:      return TEXT("not_needed");
	case EBlueprintHelperRollbackResult::RolledBack:     return TEXT("rolled_back");
	case EBlueprintHelperRollbackResult::Blocked:        return TEXT("blocked");
	case EBlueprintHelperRollbackResult::Failed:         return TEXT("failed");
	case EBlueprintHelperRollbackResult::Unavailable:    return TEXT("unavailable");
	default:                                             return TEXT("unknown");
	}
}

#pragma region Tool Result Structs

// ─── 7.2 FBlueprintHelperTargetRef ───

/** 工具操作的目标引用。 */
struct FBlueprintHelperTargetRef
{
	/** 资产路径，例如 /Game/BP/BP_Test。 */
	FString AssetPath;

	/** 资产类型，例如 Blueprint、InputAction、DataTable。 */
	FString AssetClass;

	/** 蓝图路径（如需要区分 package path / generated class path）。 */
	FString BlueprintPath;

	/** 目标类型。 */
	EBlueprintHelperTargetType TargetType = EBlueprintHelperTargetType::None;

	/** 图表名，例如 EventGraph / EG_xxx / 函数图名。 */
	FString Graph;

	/** 函数名。 */
	FString Function;

	/** 事件名。 */
	FString Event;

	/** BlueprintHelper-owned block ID。 */
	FString BlockId;

	/** LogicJson path 或稳定定位路径。 */
	FString NodePath;

	/** 精确 Pin 定位。 */
	FString PinPath;

	/** 精确 Link 定位。 */
	FString LinkPath;

	/** 组件名。 */
	FString ComponentName;

	/** UObject / Class Default / Component property path。 */
	FString PropertyPath;

	/** WidgetTree 内路径。 */
	FString WidgetPath;

	/** DataTable 行名。 */
	FString RowName;

	/** 序列化到 JSON。 */
	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		if (!AssetPath.IsEmpty()) Json->SetStringField(TEXT("asset_path"), AssetPath);
		if (!AssetClass.IsEmpty()) Json->SetStringField(TEXT("asset_class"), AssetClass);
		if (!BlueprintPath.IsEmpty()) Json->SetStringField(TEXT("blueprint_path"), BlueprintPath);
		Json->SetStringField(TEXT("target_type"), TargetTypeToString(TargetType));
		if (!Graph.IsEmpty()) Json->SetStringField(TEXT("graph"), Graph);
		if (!Function.IsEmpty()) Json->SetStringField(TEXT("function"), Function);
		if (!Event.IsEmpty()) Json->SetStringField(TEXT("event"), Event);
		if (!BlockId.IsEmpty()) Json->SetStringField(TEXT("block_id"), BlockId);
		if (!NodePath.IsEmpty()) Json->SetStringField(TEXT("node_path"), NodePath);
		if (!PinPath.IsEmpty()) Json->SetStringField(TEXT("pin_path"), PinPath);
		if (!LinkPath.IsEmpty()) Json->SetStringField(TEXT("link_path"), LinkPath);
		if (!ComponentName.IsEmpty()) Json->SetStringField(TEXT("component_name"), ComponentName);
		if (!PropertyPath.IsEmpty()) Json->SetStringField(TEXT("property_path"), PropertyPath);
		if (!WidgetPath.IsEmpty()) Json->SetStringField(TEXT("widget_path"), WidgetPath);
		if (!RowName.IsEmpty()) Json->SetStringField(TEXT("row_name"), RowName);
		return Json;
	}
};

// ─── 7.3 FBlueprintHelperSafetySummary ───

/** 安全检查摘要。不包含 SafetyProfile。 */
struct FBlueprintHelperSafetySummary
{
	/** 风险等级。 */
	EBlueprintHelperRiskLevel RiskLevel = EBlueprintHelperRiskLevel::None;

	/** 是否策略要求 dry_run。 */
	bool bDryRunRequired = false;

	/** 是否已执行 dry_run。 */
	bool bDryRunPerformed = false;

	/** dry_run 或安全检查是否允许执行。 */
	bool bCanExecute = true;

	/** 阻断原因 code 列表（不放长文本）。 */
	TArray<FString> BlockedBy;

	/** 序列化到 JSON。 */
	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("risk_level"), RiskLevelToString(RiskLevel));
		Json->SetBoolField(TEXT("dry_run_required"), bDryRunRequired);
		Json->SetBoolField(TEXT("dry_run_performed"), bDryRunPerformed);
		Json->SetBoolField(TEXT("can_execute"), bCanExecute);
		if (BlockedBy.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const FString& Item : BlockedBy) { Arr.Add(MakeShared<FJsonValueString>(Item)); }
			Json->SetArrayField(TEXT("blocked_by"), Arr);
		}
		return Json;
	}
};

// ─── 7.5 FBlueprintHelperOwnershipSummary ───

/** Ownership 变更摘要。只出现在 transaction.ownership_summary。 */
struct FBlueprintHelperOwnershipSummary
{
	/** 新增 owned 节点数。 */
	int32 OwnedNodesCount = 0;

	/** 新增 owned 连线数。 */
	int32 OwnedLinksCount = 0;

	/** 修改 owned 节点数。 */
	int32 AffectedOwnedNodesCount = 0;

	/** 修改 owned 连线数。 */
	int32 AffectedOwnedLinksCount = 0;

	/** 删除 owned 节点数。 */
	int32 DeletedOwnedNodesCount = 0;

	/** 删除 owned 连线数。 */
	int32 DeletedOwnedLinksCount = 0;

	/** 是否写入 Metadata。 */
	bool bMetadataWritten = false;

	/** 是否写入 NodeComment。 */
	bool bNodeCommentsWritten = false;

	/** 是否保留原 Metadata。 */
	bool bMetadataPreserved = false;

	/** 是否保留原 NodeComment。 */
	bool bNodeCommentsPreserved = false;

	/** 是否移除 Metadata。 */
	bool bMetadataRemoved = false;

	/** 是否移除 NodeComment。 */
	bool bNodeCommentsRemoved = false;

	/** 序列化到 JSON。 */
	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("owned_nodes_count"), OwnedNodesCount);
		Json->SetNumberField(TEXT("owned_links_count"), OwnedLinksCount);
		Json->SetNumberField(TEXT("affected_owned_nodes_count"), AffectedOwnedNodesCount);
		Json->SetNumberField(TEXT("affected_owned_links_count"), AffectedOwnedLinksCount);
		Json->SetNumberField(TEXT("deleted_owned_nodes_count"), DeletedOwnedNodesCount);
		Json->SetNumberField(TEXT("deleted_owned_links_count"), DeletedOwnedLinksCount);
		Json->SetBoolField(TEXT("metadata_written"), bMetadataWritten);
		Json->SetBoolField(TEXT("node_comments_written"), bNodeCommentsWritten);
		Json->SetBoolField(TEXT("metadata_preserved"), bMetadataPreserved);
		Json->SetBoolField(TEXT("node_comments_preserved"), bNodeCommentsPreserved);
		Json->SetBoolField(TEXT("metadata_removed"), bMetadataRemoved);
		Json->SetBoolField(TEXT("node_comments_removed"), bNodeCommentsRemoved);
		return Json;
	}
};

// ─── 7.4 FBlueprintHelperTransactionSummary ───

/** 事务摘要（正式写工具返回）。 */
struct FBlueprintHelperTransactionSummary
{
	/** 事务 ID。 */
	FString TransactionId;

	/** 事务状态。 */
	EBlueprintHelperTransactionStatus Status = EBlueprintHelperTransactionStatus::Applied;

	/** 被修改资产列表。 */
	TArray<FString> AffectedAssets;

	/** 涉及的 block ID 列表。 */
	TArray<FString> BlockIds;

	/** 是否可回滚。 */
	bool bRollbackAvailable = false;

	/** Ownership 变更摘要。 */
	TOptional<FBlueprintHelperOwnershipSummary> OwnershipSummary;

	/** 序列化到 JSON。 */
	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("transaction_id"), TransactionId);
		Json->SetStringField(TEXT("status"), TransactionStatusToString(Status));
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const FString& Asset : AffectedAssets) { Arr.Add(MakeShared<FJsonValueString>(Asset)); }
			Json->SetArrayField(TEXT("affected_assets"), Arr);
		}
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const FString& Id : BlockIds) { Arr.Add(MakeShared<FJsonValueString>(Id)); }
			Json->SetArrayField(TEXT("block_ids"), Arr);
		}
		Json->SetBoolField(TEXT("rollback_available"), bRollbackAvailable);
		if (OwnershipSummary.IsSet())
		{
			Json->SetObjectField(TEXT("ownership_summary"), OwnershipSummary->ToJson());
		}
		return Json;
	}
};

// ─── 7.7 FBlueprintHelperValidationMessage ───

/** 单条 Validation 消息。 */
struct FBlueprintHelperValidationMessage
{
	/** 稳定 code。 */
	FString Code;

	/** 简短说明。 */
	FString Message;

	/** 严重度。 */
	EBlueprintHelperValidationSeverity Severity = EBlueprintHelperValidationSeverity::Info;

	/** 关联节点路径（可选）。 */
	FString NodePath;

	/** 关联 Pin 路径（可选）。 */
	FString PinPath;

	/** 序列化到 JSON。 */
	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		if (!Code.IsEmpty()) Json->SetStringField(TEXT("code"), Code);
		if (!Message.IsEmpty()) Json->SetStringField(TEXT("message"), Message);
		Json->SetStringField(TEXT("severity"), ValidationSeverityToString(Severity));
		if (!NodePath.IsEmpty()) Json->SetStringField(TEXT("node_path"), NodePath);
		if (!PinPath.IsEmpty()) Json->SetStringField(TEXT("pin_path"), PinPath);
		return Json;
	}
};

// ─── 7.7 FBlueprintHelperValidationSummary ───

/** Validation 摘要。 */
struct FBlueprintHelperValidationSummary
{
	/** 是否建议编译。 */
	bool bShouldCompile = false;

	/** 是否建议保存。 */
	bool bShouldSave = false;

	/** 本工具是否已执行编译。 */
	bool bCompiled = false;

	/** 本工具是否已执行保存。 */
	bool bSaved = false;

	/** 编译是否成功。 */
	bool bCompileSuccess = false;

	/** Validation 级错误（不是通用 diagnostics）。 */
	TArray<FBlueprintHelperValidationMessage> Errors;

	/** Validation 级警告。 */
	TArray<FBlueprintHelperValidationMessage> Warnings;

	/** 序列化到 JSON。Common Envelope 协议：仅输出 should_compile/should_save。 */
	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetBoolField(TEXT("should_compile"), bShouldCompile);
		Json->SetBoolField(TEXT("should_save"), bShouldSave);
		return Json;
	}
};

// ─── 7.8 FBlueprintHelperToolError ───

/** 工具错误信息（失败时返回）。 */
struct FBlueprintHelperToolError
{
	/** 稳定错误码。 */
	FString Code;

	/** 失败阶段。 */
	EBlueprintHelperToolStage Stage = EBlueprintHelperToolStage::Execute;

	/** 简短错误说明。 */
	FString Message;

	/** 是否可重试。 */
	bool bRetryable = false;

	/** 回滚结果。 */
	EBlueprintHelperRollbackResult RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;

	/** 关联输入字段（可选）。 */
	FString Field;

	/** 预期值摘要（可选）。 */
	FString Expected;

	/** 实际值摘要（可选）。 */
	FString Actual;

	/** 序列化到 JSON。 */
	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		if (!Code.IsEmpty()) Json->SetStringField(TEXT("code"), Code);
		Json->SetStringField(TEXT("stage"), ToolStageToString(Stage));
		if (!Message.IsEmpty()) Json->SetStringField(TEXT("message"), Message);
		Json->SetBoolField(TEXT("retryable"), bRetryable);
		Json->SetStringField(TEXT("rollback_result"), RollbackResultToString(RollbackResult));
		if (!Field.IsEmpty()) Json->SetStringField(TEXT("field"), Field);
		if (!Expected.IsEmpty()) Json->SetStringField(TEXT("expected"), Expected);
		if (!Actual.IsEmpty()) Json->SetStringField(TEXT("actual"), Actual);
		return Json;
	}
};

// ─── 7.6 FBlueprintHelperReviewSummary ───

/** Review 摘要。 */
struct FBlueprintHelperReviewSummary
{
	/** 是否需要进入审阅。 */
	bool bReviewRequired = false;

	/** 当前审阅状态。 */
	EBlueprintHelperReviewStatus ReviewStatus = EBlueprintHelperReviewStatus::None;

	/** 审阅原因，例如 graph_write。 */
	FString ReviewReason;

	/** 推荐分组方式。 */
	TArray<FString> ReviewGrouping;

	/** 序列化到 JSON。 */
	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetBoolField(TEXT("review_required"), bReviewRequired);
		Json->SetStringField(TEXT("review_status"), ReviewStatusToString(ReviewStatus));
		if (!ReviewReason.IsEmpty()) Json->SetStringField(TEXT("review_reason"), ReviewReason);
		if (ReviewGrouping.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const FString& G : ReviewGrouping) { Arr.Add(MakeShared<FJsonValueString>(G)); }
			Json->SetArrayField(TEXT("review_grouping"), Arr);
		}
		return Json;
	}
};

// ─── 7.9 FBlueprintHelperInternalDiagnostics ───

/**
 * 内部诊断信息。
 * 暂不映射到 Agent 可见公共返回体。
 * 未来可能进入 Transaction Journal 或 Review Store。
 */
struct FBlueprintHelperInternalDiagnostics
{
	/** 诊断项列表。 */
	TArray<FBlueprintHelperDiagnosticItem> Items;

	/** verbose 模式是否可用（待定）。 */
	bool bVerboseAvailable = false;

	/** 调试引用，用于 UE log / MCP debug log。 */
	FString DebugRef;
};

// ─── 7.1 FBlueprintHelperToolResultBase ───

/**
 * 统一 MCP 工具返回基础结构。
 * 所有工具返回体使用此类作为公共协议层。
 */
struct FBlueprintHelperToolResultBase
{
	/** 工具调用是否成功。 */
	bool bOk = false;

	/** Schema 版本标识，例如 BlueprintHelper.McpToolResult.v1。 */
	FString Schema;

	/** 公共操作名，不暴露 MCP tool name / Bridge command。 */
	FString Operation;

	/** Agent 可见唯一追踪 ID。 */
	FString TraceId;

	/** 工具状态。 */
	EBlueprintHelperToolStatus Status = EBlueprintHelperToolStatus::Completed;

	/** 是否修改 UE 资产或项目状态。 */
	bool bModified = false;

	/** 目标引用。多数工具需设置，runtime_profile 可省略。 */
	TOptional<FBlueprintHelperTargetRef> Target;

	/** 安全检查摘要。写工具 / dry_run 设置。不包含 safety_profile。 */
	TOptional<FBlueprintHelperSafetySummary> Safety;

	/** 事务摘要。正式写工具设置。读、compile、save 不生成。 */
	TOptional<FBlueprintHelperTransactionSummary> Transaction;

	/** 各工具簇专属 payload。 */
	TSharedPtr<FJsonObject> Data;

	/** Review 摘要。写工具按需设置。 */
	TOptional<FBlueprintHelperReviewSummary> Review;

	/** Validation 提示。写工具成功时按需设置。仅 should_compile/should_save。 */
	TOptional<FBlueprintHelperValidationSummary> Validation;

	/** 错误信息。失败时设置，成功时不出现。 */
	TOptional<FBlueprintHelperToolError> Error;

	/** 自定义 target JSON（当 TargetRef 字段不匹配工具契约时使用）。 */
	TSharedPtr<FJsonObject> CustomTargetJson;

	/** 内部诊断（不进入公共 JSON）。 */
	FBlueprintHelperInternalDiagnostics InternalDiagnostics;

	/** 序列化到 JSON（仅包含 Agent 可见字段：transaction/review/safety 不默认输出）。 */
	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetBoolField(TEXT("ok"), bOk);
		Json->SetStringField(TEXT("schema"), Schema);
		Json->SetStringField(TEXT("operation"), Operation);
		Json->SetStringField(TEXT("trace_id"), TraceId);
		Json->SetStringField(TEXT("status"), ToolStatusToString(Status));
		Json->SetBoolField(TEXT("modified"), bModified);

		if (CustomTargetJson.IsValid()) { Json->SetObjectField(TEXT("target"), CustomTargetJson); }
		else if (Target.IsSet()) { Json->SetObjectField(TEXT("target"), Target->ToJson()); }
		if (Data.IsValid()) { Json->SetObjectField(TEXT("data"), Data); }
		if (Validation.IsSet()) { Json->SetObjectField(TEXT("validation"), Validation->ToJson()); }
		if (Error.IsSet()) { Json->SetObjectField(TEXT("error"), Error->ToJson()); }

		return Json;
	}

	/** 序列化为 JSON 字符串。 */
	FString ToJsonString() const
	{
		FString Output;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(ToJson(), Writer);
		return Output;
	}
};

#pragma endregion Tool Result Structs

// ─── Builder ───

/**
 * 工具结果构造器。
 * 统一生成 ok / schema / operation / trace_id / status / modified 等基础字段，
 * 并按工具类型正确填充 target / transaction / safety / data / review / validation / error。
 */
class BLUEPRINTHELPER_API FBlueprintHelperToolResultBuilder
{
public:
	/** 设置 Schema 版本。 */
	static constexpr const TCHAR* DefaultSchema = TEXT("BlueprintHelper.McpToolResult.v1");

	/** 构造成功结果。 */
	static FBlueprintHelperToolResultBase Success(const FString& Operation, const FString& TraceId);

	/** 构造失败结果。 */
	static FBlueprintHelperToolResultBase Failure(const FString& Operation, const FString& TraceId, const FBlueprintHelperToolError& Error);

	/** 构造 DryRun 结果。 */
	static FBlueprintHelperToolResultBase DryRun(const FString& Operation, const FString& TraceId);

	/** 构造 NoOp 结果。 */
	static FBlueprintHelperToolResultBase NoOp(const FString& Operation, const FString& TraceId);

	/** 构造默认 Applied 结果（写工具成功）。 */
	static FBlueprintHelperToolResultBase Applied(const FString& Operation, const FString& TraceId);

	/** 构造默认 Completed 结果（读工具成功）。 */
	static FBlueprintHelperToolResultBase Completed(const FString& Operation, const FString& TraceId);

	/** 生成唯一 TraceId。 */
	static FString GenerateTraceId();

	/** 生成唯一 TransactionId。 */
	static FString GenerateTransactionId(const FString& Prefix = TEXT("tx"));

private:
	static int32 TraceCounter;
	static int32 TransactionCounter;
};
