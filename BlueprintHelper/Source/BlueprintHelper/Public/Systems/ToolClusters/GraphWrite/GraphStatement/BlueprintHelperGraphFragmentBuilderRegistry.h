#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

class FBlueprintHelperActionContextScope;
class UEdGraph;
struct FBlueprintHelperGraphExpressionIR;
struct FBlueprintHelperGraphStatementIR;

class BLUEPRINTHELPER_API FBlueprintHelperGraphFragmentBuilderRegistry
{
public:
	static bool TryBuildStatement(
		UEdGraph* TargetGraph,
		const FBlueprintHelperActionContextScope* ActionContextScope,
		const FBlueprintHelperGraphStatementIR& Statement,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError,
		TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions = nullptr,
		const FBlueprintHelperGraphSemanticPinBindings* SemanticPinBindings = nullptr);

	static bool TryBuildExpression(
		UEdGraph* TargetGraph,
		const FBlueprintHelperActionContextScope* ActionContextScope,
		const FBlueprintHelperGraphExpressionIR& Expression,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError,
		TArray<FBlueprintHelperCandidateFunctionGroup>* OutCandidateFunctions = nullptr);
};
