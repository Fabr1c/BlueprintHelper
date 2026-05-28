// GraphWrite Fragment 函数包装器 — 供需要短名称调用方的 inline 包装

#pragma once

#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentUtils.h"

inline void AddOwnershipTagIfPresent(FBlueprintHelperNodeFragment& Fragment, const FString& Key, const FString& Value)
{
	FBlueprintHelperGraphFragmentUtils::AddOwnershipTagIfPresent(Fragment, Key, Value);
}
