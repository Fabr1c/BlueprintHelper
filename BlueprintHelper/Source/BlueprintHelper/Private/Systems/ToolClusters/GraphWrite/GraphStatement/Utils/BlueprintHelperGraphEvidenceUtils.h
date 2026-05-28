// GraphWrite Evidence 取值和哈希工具

#pragma once

#include "CoreMinimal.h"

class FBlueprintHelperGraphEvidenceUtils
{
public:
	/** 从 Evidence Map 中按 Key 取值并 Trim */
	static FString ContextEvidenceValue(const TMap<FString, FString>& Evidence, const FString& Key);

	/** 稳定哈希字符串 */
	static FString StableHashString(const FString& Stable);
};
