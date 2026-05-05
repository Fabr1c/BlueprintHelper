// BlueprintHelper v1.5 。MakeStruct / BreakStruct Handler

#pragma once

#include "NodeHandlers/GraphWrite/BlueprintNodeHandler.h"

/**
 * 处理结构体操作节点：K2Node_MakeStruct、K2Node_BreakStruct。
 */
class FStructOperationNodeHandler : public IBlueprintNodeHandler
{
public:
	virtual bool CanHandle(EParsedBlueprintNodeType NodeType) const override;
	virtual UK2Node* Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const override;

private:
	UK2Node* SpawnMakeStruct(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const;
	UK2Node* SpawnBreakStruct(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const;

	static UScriptStruct* ResolveScriptStruct(const FString& StructPath, FString& OutError);
};
