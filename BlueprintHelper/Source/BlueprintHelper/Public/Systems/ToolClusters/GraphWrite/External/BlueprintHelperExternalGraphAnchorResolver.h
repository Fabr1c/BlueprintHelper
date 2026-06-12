#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorTypes.h"

class UEdGraphNode;
class UEdGraphPin;

struct BLUEPRINTHELPER_API FBlueprintHelperExternalGraphLinkResolution
{
	UEdGraphPin* SourcePin = nullptr;
	UEdGraphPin* TargetPin = nullptr;
	FString LinkKind;
};

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

	bool ResolveCompactPin(
		const FString& AssetPath,
		const FString& GraphName,
		const FBlueprintHelperExternalCompactAnchor& Anchor,
		UEdGraphPin*& OutPin,
		FString& OutError) const;

	bool ResolveCompactNode(
		const FString& AssetPath,
		const FString& GraphName,
		const FBlueprintHelperExternalCompactAnchor& Anchor,
		UEdGraphNode*& OutNode,
		FString& OutError) const;

	bool ResolveCompactLink(
		const FString& AssetPath,
		const FString& GraphName,
		const FBlueprintHelperExternalCompactAnchor& Anchor,
		FBlueprintHelperExternalGraphLinkResolution& OutLink,
		FString& OutError) const;
};
