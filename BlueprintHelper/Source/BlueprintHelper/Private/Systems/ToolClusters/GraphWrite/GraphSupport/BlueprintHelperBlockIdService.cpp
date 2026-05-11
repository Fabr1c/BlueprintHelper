// BlueprintHelper Service Layer — BlockId 生成服务实现

#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperBlockIdService.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

FString FBlueprintHelperBlockIdService::MakeBlockRef(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FString& EntryName) const
{
	if (!Blueprint || !Graph || EntryName.IsEmpty())
	{
		return FString();
	}

	int32 MaxIndex = -1;

	// 扫描当前图表已有 BlueprintHelperBlockId metadata
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node)
		{
			continue;
		}

		UPackage* Package = Node->GetOutermost();
		if (!Package)
		{
			continue;
		}

		FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
		const FString BlockId = MetaData.GetValue(Node, TEXT("BlueprintHelperBlockId"));
		if (BlockId.IsEmpty())
		{
			continue;
		}

		// full_block_id = graph_id + "_" + block_ref
		// 从 BlockId 中提取 EntryName 前缀
		if (BlockId.StartsWith(EntryName))
		{
			const FString Suffix = BlockId.Mid(EntryName.Len());
			if (Suffix.IsNumeric())
			{
				const int32 Index = FCString::Atoi(*Suffix);
				if (Index > MaxIndex)
				{
					MaxIndex = Index;
				}
			}
		}
	}

	const int32 NewIndex = MaxIndex + 1;
	return FString::Printf(TEXT("%s%d"), *EntryName, NewIndex);
}

FString FBlueprintHelperBlockIdService::MakeFullBlockId(
	const FString& GraphId,
	const FString& BlockRef) const
{
	if (GraphId.IsEmpty() || BlockRef.IsEmpty())
	{
		return FString();
	}

	return FString::Printf(TEXT("%s_%s"), *GraphId, *BlockRef);
}
