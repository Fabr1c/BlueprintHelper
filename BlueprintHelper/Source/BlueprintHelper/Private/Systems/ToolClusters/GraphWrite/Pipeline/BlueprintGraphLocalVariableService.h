#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphParsedTypes.h"

class UEdGraph;
class UStruct;
struct FEdGraphPinType;

class BLUEPRINTHELPER_API FBlueprintGraphLocalVariableService
{
public:
	static bool ConvertToEdGraphPinType(const FParsedPinType& InPinType, FEdGraphPinType& OutPinType, FString& OutErrorMessage);
	static bool EnsureLocalVariableExists(UEdGraph* TargetGraph, const FParsedLocalVariableDeclaration& Declaration, FString& OutErrorMessage);
	static UStruct* ResolveLocalVariableScope(UEdGraph* TargetGraph, FString& OutErrorMessage);
	static UStruct* ResolveVariableSource(UEdGraph* TargetGraph, const FParsedVariableReference& VariableReference, FString& OutErrorMessage);
};
