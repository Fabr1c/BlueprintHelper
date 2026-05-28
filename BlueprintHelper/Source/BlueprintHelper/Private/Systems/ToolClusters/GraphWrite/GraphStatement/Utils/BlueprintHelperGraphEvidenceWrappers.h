// GraphWrite Evidence 函数包装器 — 供需要短名称调用方的 inline 包装

#pragma once

#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphEvidenceUtils.h"

inline FString ContextEvidenceValue(const TMap<FString, FString>& Evidence, const FString& Key)
{
	return FBlueprintHelperGraphEvidenceUtils::ContextEvidenceValue(Evidence, Key);
}

inline FString StableHashString(const FString& S)
{
	return FBlueprintHelperGraphEvidenceUtils::StableHashString(S);
}
