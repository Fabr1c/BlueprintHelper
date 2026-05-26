#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"

class UEdGraph;

class FBlueprintHelperStructFieldFragmentBuilder
{
public:
	static bool SupportsOperation(const FString& Operation);
	static TArray<FString> RequiredEvidenceKeys(const FString& Operation);
	static bool BuildSetFieldsInStructFragment(
		UEdGraph* TargetGraph,
		const FBlueprintHelperActionResolutionRequest& Request,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError);
};
