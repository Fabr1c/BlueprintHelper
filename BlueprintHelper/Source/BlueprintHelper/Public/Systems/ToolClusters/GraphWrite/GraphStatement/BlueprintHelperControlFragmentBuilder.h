#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

class UEdGraph;
class FBlueprintHelperActionContextScope;
struct FBlueprintHelperGraphStatementIR;

class BLUEPRINTHELPER_API FBlueprintHelperControlFragmentBuilder
{
public:
	static bool BuildSequence(
		UEdGraph* TargetGraph,
		const FString& FragmentId,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError);

	static bool BuildBranch(
		UEdGraph* TargetGraph,
		const FBlueprintHelperActionContextScope* ActionContextScope,
		const FBlueprintHelperGraphStatementIR& Statement,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError);

	static bool BuildReturn(
		UEdGraph* TargetGraph,
		const FBlueprintHelperActionContextScope* ActionContextScope,
		const FBlueprintHelperGraphStatementIR& Statement,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError);

	static bool BuildStatement(
		UEdGraph* TargetGraph,
		const FBlueprintHelperActionContextScope* ActionContextScope,
		const FBlueprintHelperGraphStatementIR& Statement,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError);
};
