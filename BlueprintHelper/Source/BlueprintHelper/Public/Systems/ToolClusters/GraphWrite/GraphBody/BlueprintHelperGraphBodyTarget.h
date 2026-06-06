#pragma once

#include "CoreMinimal.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;

struct BLUEPRINTHELPER_API FBlueprintHelperGraphBodyTarget
{
	UBlueprint* Blueprint = nullptr;
	UEdGraph* Graph = nullptr;
	FString AssetPath;
	FString GraphName;
	FString EntryName;
	FString BodyIdentity;
	TArray<UEdGraphNode*> EntryBoundaryNodes;
	TArray<UEdGraphNode*> ExitBoundaryNodes;
	TArray<UEdGraphNode*> ProtectedNodes;
	TArray<UEdGraphNode*> DeletableNodes;
};
