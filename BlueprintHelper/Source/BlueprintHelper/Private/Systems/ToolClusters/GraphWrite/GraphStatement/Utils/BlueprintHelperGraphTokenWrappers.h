// GraphWrite Token 函数包装器 — 供需要短名称调用方的 inline 包装
// 所有文件应包含此头文件而非自行定义 NormalizeOperation / FirstNonEmpty 等 static 函数

#pragma once

#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphTokenUtils.h"

inline FString NormalizeOperation(const FString& S)
{
	return FBlueprintHelperGraphTokenUtils::NormalizeOperation(S);
}

inline FString NormalizeFieldToken(const FString& S)
{
	return FBlueprintHelperGraphTokenUtils::NormalizeFieldToken(S);
}

inline FString NormalizeDelegateOperation(const FString& S)
{
	return FBlueprintHelperGraphTokenUtils::NormalizeDelegateOperation(S);
}

inline FString NormalizeScheduleOperationToken(const FString& S)
{
	return FBlueprintHelperGraphTokenUtils::NormalizeScheduleOperationToken(S);
}

inline FString NormalizeSingletonControlQuery(const FString& S)
{
	return FBlueprintHelperGraphTokenUtils::NormalizeSingletonControlQuery(S);
}

inline FString NormalizeOpOperationToken(const FString& S)
{
	return FBlueprintHelperGraphTokenUtils::NormalizeOpOperationToken(S);
}

inline FString FirstNonEmpty(const FString& A, const FString& B)
{
	return FBlueprintHelperGraphTokenUtils::FirstNonEmptyString(A, B);
}

inline FString FirstNonEmpty(const FString& A, const FString& B, const FString& C)
{
	return FBlueprintHelperGraphTokenUtils::FirstNonEmptyString(A, B, C);
}

inline FString FirstNonEmpty(const FString& A, const FString& B, const FString& C, const FString& D)
{
	return FBlueprintHelperGraphTokenUtils::FirstNonEmptyString(A, B, C, D);
}

inline FString FirstNonEmpty(const FString& A, const FString& B, const FString& C, const FString& D, const FString& E)
{
	return FirstNonEmpty(FirstNonEmpty(A, B, C, D), E);
}

inline FString FirstNonEmpty(const FString& A, const FString& B, const FString& C, const FString& D, const FString& E, const FString& F)
{
	return FirstNonEmpty(FirstNonEmpty(A, B, C, D, E), F);
}

// 通用字符串辅助函数

inline FString Clean(const FString& Value)
{
	return Value.TrimStartAndEnd();
}

// FirstNonEmptyString aliases for backward compatibility
inline FString FirstNonEmptyString(const FString& A, const FString& B)
{
	return FirstNonEmpty(A, B);
}

inline FString FirstNonEmptyString(const FString& A, const FString& B, const FString& C)
{
	return FirstNonEmpty(A, B, C);
}

inline FString FirstNonEmptyString(const FString& A, const FString& B, const FString& C, const FString& D)
{
	return FirstNonEmpty(A, B, C, D);
}
