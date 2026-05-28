// GraphWrite Token 规范化工具

#pragma once

#include "CoreMinimal.h"

class FBlueprintHelperGraphTokenUtils
{
public:
	/** Trim + ToLower 通用 Token 规范化 */
	static FString NormalizeToken(const FString& Token);

	/** 操作名称规范化 */
	static FString NormalizeOperation(const FString& Operation);

	/** 字段 Token 规范化 */
	static FString NormalizeFieldToken(const FString& Token);

	/** 委托操作规范化 */
	static FString NormalizeDelegateOperation(const FString& Operation);

	/** Schedule 操作 Token 规范化 */
	static FString NormalizeScheduleOperationToken(const FString& ScheduleOperation);

	/** Singleton 控制查询规范化 */
	static FString NormalizeSingletonControlQuery(const FString& Query);

	/** 运算符 Token 规范化（含符号→名称映射） */
	static FString NormalizeOpOperationToken(const FString& Token);

	/** 返回第一个非空字符串（两参数） */
	static FString FirstNonEmptyString(const FString& First, const FString& Second);

	/** 返回第一个非空字符串（三参数） */
	static FString FirstNonEmptyString(const FString& First, const FString& Second, const FString& Third);

	/** 返回第一个非空字符串（四参数） */
	static FString FirstNonEmptyString(const FString& First, const FString& Second, const FString& Third, const FString& Fourth);
};
