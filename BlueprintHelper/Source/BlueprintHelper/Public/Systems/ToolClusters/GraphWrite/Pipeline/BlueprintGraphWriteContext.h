#pragma once

#include "CoreMinimal.h"

class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class UK2Node;

class BLUEPRINTHELPER_API FBlueprintGraphWriteContext
{
public:
	void Initialize(UEdGraph* InGraph);
	bool IsValid() const;
	UEdGraph* GetGraph() const;

	void RegisterNode(const FString& NodeId, UK2Node* Node, bool bGenerated);
	UK2Node* FindNode(const FString& NodeId) const;
	UEdGraphPin* FindPinByAlias(const FString& NodeId, const FString& RequestedPinName);

	const TArray<UEdGraphNode*>& GetGeneratedNodes() const;

private:
	void BuildPinIndex(UK2Node* Node);
	static FString MakePinLookupKey(UK2Node* Node);

	UEdGraph* Graph = nullptr;
	TMap<FString, UK2Node*> IdToNode;
	TMap<FString, TMap<FString, UEdGraphPin*>> PinIndexByNodeKey;
	TArray<UEdGraphNode*> GeneratedNodes;
};
