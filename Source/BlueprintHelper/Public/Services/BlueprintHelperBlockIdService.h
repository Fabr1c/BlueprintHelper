// BlueprintHelper Service Layer — BlockId 生成服务

#pragma once

#include "CoreMinimal.h"

class UBlueprint;
class UEdGraph;

/**
 * BlockId 生成服务。
 * 负责生成 block_ref 和 full_block_id，避免 Agent-facing 返回体泄漏完整 block_id。
 */
class BLUEPRINTHELPER_API FBlueprintHelperBlockIdService
{
public:
	/** 为指定入口名生成唯一的 block_ref。递增作用域：同一蓝图+同一图表+同一入口名。 */
	FString MakeBlockRef(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const FString& EntryName) const;

	/** 由 graph_id + block_ref 反推完整 block_id。 */
	FString MakeFullBlockId(
		const FString& GraphId,
		const FString& BlockRef) const;
};
