#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextScope.h"

class UBlueprint;
class UEdGraph;

class BLUEPRINTHELPER_API FBlueprintHelperActionContextBuildService
{
public:
	using FBuildComplete = TFunction<void(TSharedPtr<FBlueprintHelperActionContextScope, ESPMode::ThreadSafe>, const FString&)>;

	static bool BuildSync(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const TArray<FBlueprintHelperActionContextDemand>& Demands,
		const FBlueprintHelperActionContextRevisionToken& Revision,
		FBlueprintHelperActionContextScope& OutScope,
		FString& OutError);

	static void BuildAsyncFromSnapshot(
		FBlueprintHelperActionContextSnapshot Snapshot,
		TArray<FBlueprintHelperActionContextDemand> Demands,
		FBuildComplete Completion);
};
