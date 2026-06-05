// BlueprintHelper Service Layer — Ownership 写入服务实现

#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraphNode.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

struct FBlueprintHelperOwnershipMetadataValueSnapshot
{
	bool bHadValue = false;
	FString Value;
};

struct FBlueprintHelperOwnershipMetadataSnapshot
{
	UEdGraphNode* Node = nullptr;
	FBlueprintHelperOwnershipMetadataValueSnapshot OwnedValue;
	FBlueprintHelperOwnershipMetadataValueSnapshot BlockIdValue;
	FBlueprintHelperOwnershipMetadataValueSnapshot FeatureNameValue;
	FBlueprintHelperOwnershipMetadataValueSnapshot ToolValue;

	void Capture(UEdGraphNode* InNode)
	{
		Node = InNode;
		if (!Node)
		{
			return;
		}

		UPackage* Package = Node->GetOutermost();
		if (!Package)
		{
			return;
		}

		FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
		CaptureValue(MetaData, TEXT("BlueprintHelperOwned"), OwnedValue);
		CaptureValue(MetaData, TEXT("BlueprintHelperBlockId"), BlockIdValue);
		CaptureValue(MetaData, TEXT("BlueprintHelperFeatureName"), FeatureNameValue);
		CaptureValue(MetaData, TEXT("BlueprintHelperTool"), ToolValue);
	}

	void Restore() const
	{
		if (!Node)
		{
			return;
		}

		UPackage* Package = Node->GetOutermost();
		if (!Package)
		{
			return;
		}

		FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
		RestoreValue(MetaData, TEXT("BlueprintHelperOwned"), OwnedValue);
		RestoreValue(MetaData, TEXT("BlueprintHelperBlockId"), BlockIdValue);
		RestoreValue(MetaData, TEXT("BlueprintHelperFeatureName"), FeatureNameValue);
		RestoreValue(MetaData, TEXT("BlueprintHelperTool"), ToolValue);
	}

private:
	void CaptureValue(
		FBlueprintHelperPackageMetaData& MetaData,
		const TCHAR* Key,
		FBlueprintHelperOwnershipMetadataValueSnapshot& OutValue) const
	{
		OutValue.bHadValue = MetaData.HasValue(Node, Key);
		OutValue.Value = OutValue.bHadValue
			? MetaData.GetValue(Node, Key)
			: FString();
	}

	void RestoreValue(
		FBlueprintHelperPackageMetaData& MetaData,
		const TCHAR* Key,
		const FBlueprintHelperOwnershipMetadataValueSnapshot& SnapshotValue) const
	{
		if (SnapshotValue.bHadValue)
		{
			MetaData.SetValue(Node, Key, *SnapshotValue.Value);
		}
		else
		{
			MetaData.RemoveValue(Node, Key);
		}
	}
};

bool FBlueprintHelperOwnershipService::WriteNodeOwnership(
	UBlueprint* Blueprint,
	UEdGraphNode* Node,
	const FString& BlockId,
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
	MetaData.SetValue(Node, TEXT("BlueprintHelperFeatureName"), *FeatureName);
	MetaData.RemoveValue(Node, TEXT("BlueprintHelperTool"));

	return true;
}

bool FBlueprintHelperOwnershipService::WriteBlockOwnership(
	UBlueprint* Blueprint,
	const TArray<UEdGraphNode*>& Nodes,
	const FString& BlockId,
	const FString& FeatureName,
	FString& OutError) const
{
	TArray<FBlueprintHelperOwnershipMetadataSnapshot> Snapshots;
	Snapshots.Reserve(Nodes.Num());

	for (UEdGraphNode* Node : Nodes)
	{
		FBlueprintHelperOwnershipMetadataSnapshot Snapshot;
		Snapshot.Capture(Node);
		Snapshots.Add(Snapshot);

		FString NodeError;
		if (!WriteNodeOwnership(Blueprint, Node, BlockId, FeatureName, NodeError))
		{
			for (const FBlueprintHelperOwnershipMetadataSnapshot& CapturedSnapshot : Snapshots)
			{
				CapturedSnapshot.Restore();
			}

			OutError = FString::Printf(TEXT("节点 %s 的 ownership 写入失败：%s"),
				Node ? *Node->GetName() : TEXT("null"), *NodeError);
			return false;
		}
	}

	return true;
}
