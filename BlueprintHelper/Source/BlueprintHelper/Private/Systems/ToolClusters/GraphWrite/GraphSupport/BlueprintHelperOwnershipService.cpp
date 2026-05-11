// BlueprintHelper Service Layer — Ownership 写入服务实现

#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraphNode.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

bool FBlueprintHelperOwnershipService::WriteNodeOwnership(
	UBlueprint* Blueprint,
	UEdGraphNode* Node,
	const FString& BlockId,
	const FString& TransactionId,
	const FString& FeatureName,
	FString& OutError) const
{
	if (!Node)
	{
		OutError = TEXT("节点为空，无法写入 ownership metadata。");
		return false;
	}

	UPackage* Package = Node->GetOutermost();
	if (!Package)
	{
		OutError = TEXT("metadata_unavailable：无法获取节点所在 Package。");
		return false;
	}

	FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);

	// 写入 FMetaData 标记
	MetaData.SetValue(Node, TEXT("BlueprintHelperOwned"), TEXT("true"));
	MetaData.SetValue(Node, TEXT("BlueprintHelperBlockId"), *BlockId);
	MetaData.SetValue(Node, TEXT("BlueprintHelperTransactionId"), *TransactionId);
	MetaData.SetValue(Node, TEXT("BlueprintHelperFeatureName"), *FeatureName);
	MetaData.RemoveValue(Node, TEXT("BlueprintHelperTool"));

	return true;
}

bool FBlueprintHelperOwnershipService::WriteBlockOwnership(
	UBlueprint* Blueprint,
	const TArray<UEdGraphNode*>& Nodes,
	const FString& BlockId,
	const FString& TransactionId,
	const FString& FeatureName,
	FString& OutError) const
{
	for (UEdGraphNode* Node : Nodes)
	{
		FString NodeError;
		if (!WriteNodeOwnership(Blueprint, Node, BlockId, TransactionId, FeatureName, NodeError))
		{
			OutError = FString::Printf(TEXT("节点 %s 的 ownership 写入失败：%s"),
				Node ? *Node->GetName() : TEXT("null"), *NodeError);
			return false;
		}
	}

	return true;
}
