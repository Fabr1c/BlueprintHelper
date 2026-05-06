#pragma once

#include "CoreMinimal.h"
#include "Logic/BlueprintHelperLogicJsonPathService.h"

class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;

struct FBlueprintHelperGraphWriteAnchorRef
{
	FString BlockId;
	FString GroupEntryNodePath;
	FString NodeRef;
	FString PinRef;
	FString LinkRef;
	FString NodePath;
	FString PinPath;
	FString LinkPath;

	bool IsBlockScoped() const
	{
		return !BlockId.IsEmpty();
	}
};

class FBlueprintHelperGraphWriteBlockScopedResolver
{
public:
	static bool ResolveNode(
		const FBlueprintHelperLogicJsonPathService& PathService,
		UEdGraph* Graph,
		const FBlueprintHelperGraphWriteAnchorRef& Anchor,
		UEdGraphNode*& OutNode,
		FBlueprintHelperPatchResolveError& OutError);

	static bool ResolvePin(
		const FBlueprintHelperLogicJsonPathService& PathService,
		UEdGraph* Graph,
		UEdGraphNode* OwningNode,
		const FBlueprintHelperGraphWriteAnchorRef& Anchor,
		UEdGraphPin*& OutPin,
		FBlueprintHelperPatchResolveError& OutError);

	static bool ResolveLink(
		const FBlueprintHelperLogicJsonPathService& PathService,
		UEdGraph* Graph,
		const FBlueprintHelperGraphWriteAnchorRef& Anchor,
		FBlueprintHelperResolvedLink& OutLink,
		FBlueprintHelperPatchResolveError& OutError);
};
