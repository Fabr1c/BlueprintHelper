// GraphWrite Fragment 工具

#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

class FBlueprintHelperGraphFragmentUtils
{
public:
	/** 若 Key 和 Value 均非空则添加到 Fragment 的 OwnershipTags */
	static void AddOwnershipTagIfPresent(
		FBlueprintHelperNodeFragment& Fragment,
		const FString& Key,
		const FString& Value);
};
