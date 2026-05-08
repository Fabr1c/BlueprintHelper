// BlueprintHelper Service Layer — Ownership 写入服务

#pragma once

#include "CoreMinimal.h"

class UBlueprint;
class UEdGraphNode;

/**
 * Ownership 写入服务。
 * 为 BlueprintHelper-owned 节点写入 UMetaData。
 * 任一节点写入失败即整体失败。
 */
class BLUEPRINTHELPER_API FBlueprintHelperOwnershipService
{
public:
	/** 为单个节点写入 ownership metadata。 */
	bool WriteNodeOwnership(
		UBlueprint* Blueprint,
		UEdGraphNode* Node,
		const FString& BlockId,
		const FString& TransactionId,
		const FString& FeatureName,
		FString& OutError) const;

	/** 为一批节点批量写入 ownership metadata。 */
	bool WriteBlockOwnership(
		UBlueprint* Blueprint,
		const TArray<UEdGraphNode*>& Nodes,
		const FString& BlockId,
		const FString& TransactionId,
		const FString& FeatureName,
		FString& OutError) const;
};
