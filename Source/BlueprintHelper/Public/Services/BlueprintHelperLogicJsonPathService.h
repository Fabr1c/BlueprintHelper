// BlueprintHelper Service Layer — LogicJson 路径定位服务

#pragma once

#include "CoreMinimal.h"

class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;

/** LogicJson 目标定位错误。 */
struct FBlueprintHelperPatchResolveError
{
	FString Code;
	FString Message;
	FString Target;
};

/** 已解析的 Link。 */
struct FBlueprintHelperResolvedLink
{
	UEdGraphNode* SourceNode = nullptr;
	UEdGraphPin* SourcePin = nullptr;
	UEdGraphNode* TargetNode = nullptr;
	UEdGraphPin* TargetPin = nullptr;
	FString LinkRef;
};

/**
 * LogicJson 路径定位服务。
 * 将 node_ref / pin_ref / link_ref 转换为 UE 节点/Pin/Link。
 */
class BLUEPRINTHELPER_API FBlueprintHelperLogicJsonPathService
{
public:
	/** 按 node_ref / node_path 定位唯一节点。 */
	bool ResolveNode(
		UEdGraph* Graph,
		const FString& NodeRef,
		const FString& NodePath,
		UEdGraphNode*& OutNode,
		FBlueprintHelperPatchResolveError& OutError) const;

	/** 按 pin_ref / pin_path 定位唯一 Pin。 */
	bool ResolvePin(
		UEdGraph* Graph,
		UEdGraphNode* OwningNode,
		const FString& PinRef,
		const FString& PinPath,
		UEdGraphPin*& OutPin,
		FBlueprintHelperPatchResolveError& OutError) const;

	/** 按 link_ref / link_path 定位唯一 Link。 */
	bool ResolveLink(
		UEdGraph* Graph,
		const FString& LinkRef,
		const FString& LinkPath,
		FBlueprintHelperResolvedLink& OutLink,
		FBlueprintHelperPatchResolveError& OutError) const;
};
