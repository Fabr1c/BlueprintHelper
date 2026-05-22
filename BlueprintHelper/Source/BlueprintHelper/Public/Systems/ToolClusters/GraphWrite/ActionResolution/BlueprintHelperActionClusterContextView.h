#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class BLUEPRINTHELPER_API FBlueprintHelperActionClusterContextView
{
public:
	explicit FBlueprintHelperActionClusterContextView(const FBlueprintHelperActionResolutionRequest& InRequest);

	const FBlueprintHelperActionResolutionRequest& GetRequest() const;
	EBlueprintHelperSpawnerClusterKind GetClusterKind() const;
	const FBlueprintHelperActionSemanticConstraints& GetSemantic() const;

	const FString& GetStatementId() const;
	const FString& GetProjectedContextHash() const;
	const FString& GetSemanticConstraintsHash() const;
	const TMap<FString, FString>& GetEvidence() const;

	bool HasGraphContext() const;
	bool HasSemanticKind() const;
	bool HasStableIdentity() const;
	bool IsCompleteForCluster(EBlueprintHelperSpawnerClusterKind ExpectedCluster, FString& OutCode, FString& OutMessage) const;

private:
	const FBlueprintHelperActionResolutionRequest& Request;
};
