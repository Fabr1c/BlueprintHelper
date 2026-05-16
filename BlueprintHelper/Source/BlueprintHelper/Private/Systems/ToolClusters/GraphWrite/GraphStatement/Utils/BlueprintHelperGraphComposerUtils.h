#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDag.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

class UEdGraphNode;
class UEdGraphPin;

class FBlueprintHelperGraphComposerUtils
{
public:
	static UEdGraphPin* ResolveFragmentEndpointPin(
		const FBlueprintHelperNodeFragment& Fragment,
		const FBlueprintHelperGraphFragmentEndpointRef& Endpoint,
		bool bSourceEndpoint);

	static bool TryForceCompatibleDataConnection(UEdGraphPin* FromPin, UEdGraphPin* ToPin);

private:
	static UEdGraphPin* FindPinRefInMap(
		const TMap<FString, FBlueprintHelperFragmentPinRef>& PinMap,
		const FString& Key);

	static UEdGraphPin* FindNodePinByName(UEdGraphNode* Node, const FString& PinName);
	static UEdGraphPin* FindFirstNodeDataPin(UEdGraphNode* Node, EEdGraphPinDirection Direction);
};
