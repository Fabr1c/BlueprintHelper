#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

class FBlueprintHelperActionContextScope;
class UEdGraph;
struct FBlueprintHelperGraphStatementIR;

class BLUEPRINTHELPER_API FBlueprintHelperEventDelegateFragmentBuilder
{
public:
	static bool BuildStatement(
		UEdGraph* TargetGraph,
		const FBlueprintHelperActionContextScope* ActionContextScope,
		const FBlueprintHelperGraphStatementIR& Statement,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError);
};
