#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorTypes.h"

class UEdGraphNode;
class UEdGraphPin;

class BLUEPRINTHELPER_API FBlueprintHelperExternalGraphAnchorService
{
public:
	bool BuildNodeAnchor(
		const FString& AssetPath,
		const FString& GraphName,
		const UEdGraphNode* Node,
		FBlueprintHelperExternalGraphAnchor& OutAnchor,
		FString& OutError) const;

	bool BuildBodyEntryAnchor(
		const FString& AssetPath,
		const FString& GraphName,
		const UEdGraphNode* Node,
		FBlueprintHelperExternalGraphAnchor& OutAnchor,
		FString& OutError) const;

	bool BuildExecBoundaryAnchor(
		const FString& AssetPath,
		const FString& GraphName,
		const UEdGraphPin* SourcePin,
		FBlueprintHelperExternalGraphAnchor& OutAnchor,
		FString& OutError) const;
};
