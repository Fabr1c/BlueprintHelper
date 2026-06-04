// Preview-scoped GraphWrite AutoSearch candidate artifact store.

#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/AutoSearch/BlueprintHelperGraphWriteAutoSearchTypes.h"

class FBlueprintHelperGraphWriteCandidateArtifactStore
{
public:
	void Store(const FBlueprintHelperGraphWriteCandidateArtifactRecord& Artifact);

	bool TryResolve(
		const FString& PreviewToken,
		const FString& StatementId,
		const FString& CandidateId,
		FBlueprintHelperGraphWriteCandidateArtifactRecord& OutArtifact) const;

	void RemovePreviewToken(const FString& PreviewToken);
	void Clear();

private:
	static FString MakeKey(
		const FString& PreviewToken,
		const FString& StatementId,
		const FString& CandidateId);
	static bool IsKeyComplete(
		const FString& PreviewToken,
		const FString& StatementId,
		const FString& CandidateId);
	static FBlueprintHelperGraphWriteCandidateArtifactRecord CloneArtifact(
		const FBlueprintHelperGraphWriteCandidateArtifactRecord& Artifact);
	static TSharedPtr<FJsonObject> CloneJsonObject(const TSharedPtr<FJsonObject>& Source);

	TMap<FString, FBlueprintHelperGraphWriteCandidateArtifactRecord> ArtifactsByKey;
};
