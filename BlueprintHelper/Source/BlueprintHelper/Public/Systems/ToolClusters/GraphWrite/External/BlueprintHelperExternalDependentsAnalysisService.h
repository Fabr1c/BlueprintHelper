// BlueprintHelper Service Layer - external dependents analysis service.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalBodySnapshotService.h"

class UEdGraph;

struct BLUEPRINTHELPER_API FBlueprintHelperExternalDependentsAnalysis
{
	bool bSupported = true;
	TArray<FBlueprintHelperExternalBodyLink> UnsupportedDependents;

	TSharedRef<FJsonObject> ToJson() const;
};

class BLUEPRINTHELPER_API FBlueprintHelperExternalDependentsAnalysisService
{
public:
	bool Analyze(
		UEdGraph* Graph,
		const FBlueprintHelperExternalBodySnapshot& Snapshot,
		FBlueprintHelperExternalDependentsAnalysis& OutAnalysis,
		FString& OutError) const;
};
