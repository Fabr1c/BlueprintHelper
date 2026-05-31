#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorTypes.h"

class UEdGraphNode;
class UEdGraphPin;

class BLUEPRINTHELPER_API FBlueprintHelperExternalGraphAnchorResolver
{
public:
	bool ResolveNode(
		const FBlueprintHelperExternalGraphAnchor& Anchor,
		UEdGraphNode*& OutNode,
		FString& OutError) const;

	bool ResolvePin(
		const FBlueprintHelperExternalGraphAnchor& Anchor,
		UEdGraphPin*& OutPin,
		FString& OutError) const;
};
