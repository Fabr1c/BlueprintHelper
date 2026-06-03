#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h"

class FJsonObject;
class UBlueprint;
class UEdGraph;

struct FBlueprintHelperActionContextRevisionDebugFacts
{
	FString BlueprintCanonical;
	FString GraphCanonical;

	TSharedRef<FJsonObject> ToJson() const;
};

class BLUEPRINTHELPER_API FBlueprintHelperActionContextRevisionService
{
public:
	static FBlueprintHelperActionContextRevisionToken BuildRevisionToken(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const FString& TaskRunId,
		const FString& PlanHash);

	static int32 BuildBlueprintRevision(UBlueprint* Blueprint);
	static int32 BuildGraphRevision(UEdGraph* Graph);

	static FBlueprintHelperActionContextRevisionDebugFacts BuildDebugFacts(
		UBlueprint* Blueprint,
		UEdGraph* Graph);
};
