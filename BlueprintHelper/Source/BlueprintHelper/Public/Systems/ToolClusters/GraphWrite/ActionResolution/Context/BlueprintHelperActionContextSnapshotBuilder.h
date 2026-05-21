#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h"

class UBlueprint;
class UEdGraph;

class BLUEPRINTHELPER_API FBlueprintHelperActionContextSnapshotBuilder
{
public:
	static FBlueprintHelperActionContextSnapshot BuildSnapshot(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const TArray<FBlueprintHelperActionContextDemand>& Demands,
		const FBlueprintHelperActionContextRevisionToken& Revision);

private:
	static FBlueprintHelperActionContextGraphSnapshot CaptureGraph(UBlueprint* Blueprint, UEdGraph* Graph);
	static void CaptureFields(UBlueprint* Blueprint, FBlueprintHelperActionContextSnapshot& Snapshot);
};
