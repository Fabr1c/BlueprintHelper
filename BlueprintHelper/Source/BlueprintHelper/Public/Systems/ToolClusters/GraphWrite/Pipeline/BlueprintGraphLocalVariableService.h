#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"

class BLUEPRINTHELPER_API FBlueprintGraphLocalVariableService
{
public:
	static bool ConvertToEdGraphPinType(const FParsedPinType& InPinType, FEdGraphPinType& OutPinType, FString& OutErrorMessage);
	static bool EnsureLocalVariableExists(UEdGraph* TargetGraph, const FParsedLocalVariableDeclaration& Declaration, FString& OutErrorMessage);
	static UStruct* ResolveLocalVariableScope(UEdGraph* TargetGraph, FString& OutErrorMessage);
	static UStruct* ResolveVariableSource(UEdGraph* TargetGraph, const FParsedVariableReference& VariableReference, FString& OutErrorMessage);
};
