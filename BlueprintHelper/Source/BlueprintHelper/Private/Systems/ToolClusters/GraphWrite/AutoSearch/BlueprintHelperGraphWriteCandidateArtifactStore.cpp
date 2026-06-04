// Preview-scoped GraphWrite AutoSearch candidate artifact store.

#include "Systems/ToolClusters/GraphWrite/AutoSearch/BlueprintHelperGraphWriteCandidateArtifactStore.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void FBlueprintHelperGraphWriteCandidateArtifactStore::Store(
	const FBlueprintHelperGraphWriteCandidateArtifactRecord& Artifact)
{
	if (!IsKeyComplete(Artifact.PreviewToken, Artifact.StatementId, Artifact.CandidateId))
	{
		return;
	}

	ArtifactsByKey.Add(
		MakeKey(Artifact.PreviewToken, Artifact.StatementId, Artifact.CandidateId),
		CloneArtifact(Artifact));
}

bool FBlueprintHelperGraphWriteCandidateArtifactStore::TryResolve(
	const FString& PreviewToken,
	const FString& StatementId,
	const FString& CandidateId,
	FBlueprintHelperGraphWriteCandidateArtifactRecord& OutArtifact) const
{
	OutArtifact = FBlueprintHelperGraphWriteCandidateArtifactRecord();
	if (!IsKeyComplete(PreviewToken, StatementId, CandidateId))
	{
		return false;
	}

	const FBlueprintHelperGraphWriteCandidateArtifactRecord* Found =
		ArtifactsByKey.Find(MakeKey(PreviewToken, StatementId, CandidateId));
	if (!Found)
	{
		return false;
	}

	OutArtifact = CloneArtifact(*Found);
	return true;
}

void FBlueprintHelperGraphWriteCandidateArtifactStore::RemovePreviewToken(const FString& PreviewToken)
{
	TArray<FString> KeysToRemove;
	for (const TPair<FString, FBlueprintHelperGraphWriteCandidateArtifactRecord>& Pair : ArtifactsByKey)
	{
		if (Pair.Value.PreviewToken == PreviewToken)
		{
			KeysToRemove.Add(Pair.Key);
		}
	}

	for (const FString& Key : KeysToRemove)
	{
		ArtifactsByKey.Remove(Key);
	}
}

void FBlueprintHelperGraphWriteCandidateArtifactStore::Clear()
{
	ArtifactsByKey.Reset();
}

FString FBlueprintHelperGraphWriteCandidateArtifactStore::MakeKey(
	const FString& PreviewToken,
	const FString& StatementId,
	const FString& CandidateId)
{
	return FString::Printf(
		TEXT("%s|%s|%s"),
		*PreviewToken.TrimStartAndEnd(),
		*StatementId.TrimStartAndEnd(),
		*CandidateId.TrimStartAndEnd());
}

bool FBlueprintHelperGraphWriteCandidateArtifactStore::IsKeyComplete(
	const FString& PreviewToken,
	const FString& StatementId,
	const FString& CandidateId)
{
	return !PreviewToken.TrimStartAndEnd().IsEmpty()
		&& !StatementId.TrimStartAndEnd().IsEmpty()
		&& !CandidateId.TrimStartAndEnd().IsEmpty();
}

FBlueprintHelperGraphWriteCandidateArtifactRecord FBlueprintHelperGraphWriteCandidateArtifactStore::CloneArtifact(
	const FBlueprintHelperGraphWriteCandidateArtifactRecord& Artifact)
{
	FBlueprintHelperGraphWriteCandidateArtifactRecord Clone = Artifact;
	Clone.EvidenceJson = CloneJsonObject(Artifact.EvidenceJson);
	return Clone;
}

TSharedPtr<FJsonObject> FBlueprintHelperGraphWriteCandidateArtifactStore::CloneJsonObject(
	const TSharedPtr<FJsonObject>& Source)
{
	if (!Source.IsValid())
	{
		return nullptr;
	}

	FString Serialized;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
	if (!FJsonSerializer::Serialize(Source.ToSharedRef(), Writer))
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> Cloned;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Serialized);
	if (!FJsonSerializer::Deserialize(Reader, Cloned))
	{
		return nullptr;
	}
	return Cloned;
}
