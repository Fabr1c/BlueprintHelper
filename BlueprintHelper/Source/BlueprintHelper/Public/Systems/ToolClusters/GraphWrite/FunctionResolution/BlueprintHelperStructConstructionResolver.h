#pragma once

#include "CoreMinimal.h"

class UEdGraph;
class UK2Node;
class UScriptStruct;
struct FParsedNode;

class BLUEPRINTHELPER_API FBlueprintHelperStructConstructionResolver
{
public:
	static UK2Node* SpawnMakeStructNode(
		UEdGraph* TargetGraph,
		const FParsedNode& NodeData,
		UScriptStruct* TargetStruct,
		FString& OutError);

private:
	static UK2Node* SpawnNativeMakeFunctionNode(
		UEdGraph* TargetGraph,
		const FParsedNode& NodeData,
		UScriptStruct* TargetStruct,
		FString& OutError);

	static UK2Node* SpawnSearchResolvedMakeFunctionNode(
		UEdGraph* TargetGraph,
		const FParsedNode& NodeData,
		UScriptStruct* TargetStruct,
		FString& OutError);
};
