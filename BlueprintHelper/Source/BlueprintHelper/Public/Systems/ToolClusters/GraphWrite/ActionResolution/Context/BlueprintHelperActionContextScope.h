#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h"

class UBlueprint;
class UEdGraph;

class BLUEPRINTHELPER_API FBlueprintHelperActionContextScope
{
public:
	static FBlueprintHelperActionContextRevisionToken MakeRevision(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const FString& TaskRunId,
		const FString& PlanHash);

	static bool Build(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const TArray<FBlueprintHelperActionContextDemand>& Demands,
		const FBlueprintHelperActionContextRevisionToken& Revision,
		FBlueprintHelperActionContextScope& OutScope,
		FString& OutError);

	static FBlueprintHelperActionContextScope FromResolved(
		FBlueprintHelperActionContextSnapshot&& Snapshot,
		FBlueprintHelperResolvedActionContextBundle&& Bundle);

	bool TryBuildRequest(
		const FString& StatementId,
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		FBlueprintHelperActionResolutionRequest& OutRequest,
		FString& OutError) const;

	bool IsValid() const
	{
		return bValid;
	}

	const FBlueprintHelperActionContextSnapshot& GetSnapshot() const
	{
		return Snapshot;
	}

	const FBlueprintHelperResolvedActionContextBundle& GetBundle() const
	{
		return Bundle;
	}

private:
	FBlueprintHelperActionContextSnapshot Snapshot;
	FBlueprintHelperResolvedActionContextBundle Bundle;
	bool bValid = false;
};
