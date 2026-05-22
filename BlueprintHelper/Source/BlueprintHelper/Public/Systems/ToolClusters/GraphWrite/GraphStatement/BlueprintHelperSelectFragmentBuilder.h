#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

class UEdGraph;
struct FBlueprintHelperActionResolutionResult;
struct FBlueprintHelperGraphExpressionIR;

class BLUEPRINTHELPER_API FBlueprintHelperSelectFragmentBuilder
{
public:
	static bool Build(
		UEdGraph* TargetGraph,
		const FBlueprintHelperGraphExpressionIR& Expression,
		const FBlueprintHelperActionResolutionResult& ActionResult,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError);
};
