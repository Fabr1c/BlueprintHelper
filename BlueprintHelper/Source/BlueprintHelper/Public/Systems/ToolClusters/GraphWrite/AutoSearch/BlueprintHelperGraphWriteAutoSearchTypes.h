// BlueprintHelper GraphWrite AutoSearch shared DTOs.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;

struct BLUEPRINTHELPER_API FBlueprintHelperGraphWriteAutoSearchPolicy
{
	bool bEnablePreviewRecovery = false;
	int32 MaxCandidatesPerStatement = 3;
	int32 MaxAutoSearchStatements = 16;
	int32 MaxTotalSearchMs = 120;
	FString DetailLevel = TEXT("short");
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphWriteActionSelection
{
	FString CandidateId;

	bool IsValid() const
	{
		return !CandidateId.TrimStartAndEnd().IsEmpty();
	}
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphWritePreviewCandidate
{
	FString CandidateId;
	FString StatementId;
	FString SuggestedKind = TEXT("call");
	FString DisplayName;
	FString OwnerShort;
	FString NodeClass;
	FString MatchReason;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphWriteCandidateArtifactRecord
{
	FString PreviewToken;
	FString StatementId;
	FString CandidateId;
	FString CandidateHash;
	FString StableId;
	FString SnapshotGeneration;
	FString NodeClassPath;
	FString OwnerPath;
	FString SpawnerSignatureHash;
	TArray<FString> ArgumentNames;
	TMap<FString, FString> ArgumentPinTypeSummaries;
	TSharedPtr<FJsonObject> EvidenceJson;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGraphWriteAutoSearchArtifact
{
	FString PreviewToken;
	FString TaskSpecHash;
	FString ActionContextRevisionManifestHash;
	FString SnapshotGeneration;
	TArray<FBlueprintHelperGraphWriteCandidateArtifactRecord> Candidates;
};

struct BLUEPRINTHELPER_API FBlueprintHelperActionDatabaseSnapshotEntry
{
	FString StableId;
	FString DisplayName;
	FString MenuName;
	FString Category;
	FString OwnerPath;
	FString OwnerShort;
	FString NodeClassPath;
	FString SpawnerClassPath;
	FString SpawnerSignatureHash;
	FString ActionFamily;
	TArray<FString> Keywords;
	TArray<FString> ArgumentNames;
	TMap<FString, FString> ArgumentPinTypeSummaries;
	TMap<FString, FString> Flags;
};

struct BLUEPRINTHELPER_API FBlueprintHelperActionDatabaseSnapshotMetadata
{
	FString Generation;
	FString BuiltAtUtcIso;
	int32 EntryCount = 0;
	int32 SpawnerCount = 0;
};

struct BLUEPRINTHELPER_API FBlueprintHelperActionDatabaseSnapshot
{
	FString Generation;
	FDateTime BuiltAtUtc;
	int32 EntryCount = 0;
	int32 SpawnerCount = 0;
	TArray<FBlueprintHelperActionDatabaseSnapshotEntry> Entries;

	FBlueprintHelperActionDatabaseSnapshotMetadata ToMetadata() const
	{
		FBlueprintHelperActionDatabaseSnapshotMetadata Metadata;
		Metadata.Generation = Generation;
		Metadata.BuiltAtUtcIso = BuiltAtUtc.ToIso8601();
		Metadata.EntryCount = EntryCount;
		Metadata.SpawnerCount = SpawnerCount;
		return Metadata;
	}
};
