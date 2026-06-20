// BlueprintHelper Task Runtime - editor lifecycle preview store

#pragma once

#include "CoreMinimal.h"

class FJsonObject;

struct FBlueprintHelperTaskPreviewStoreCreateRequest
{
	TSharedPtr<FJsonObject> TaskPlan;
	FString TaskSpecHash;
	FString TaskPlanHash;
	FString ExecutionPolicyHash;
	FString AssetStateHash;
	FString ActionContextRevisionManifestHash;
	TSharedPtr<FJsonObject> ActionContextRevisionManifestJson;
	TSharedPtr<FJsonObject> GraphWriteCandidateArtifactJson;
	TSharedPtr<FJsonObject> ExecutionReceiptJson;
	FString GraphWriteCandidateArtifactHash;
	bool bPassed = false;
};

struct FBlueprintHelperTaskPreviewStoreResolveResult
{
	bool bOk = false;
	bool bPassed = false;
	TSharedPtr<FJsonObject> TaskPlan;
	FString AssetStateHash;
	FString ActionContextRevisionManifestHash;
	TSharedPtr<FJsonObject> ActionContextRevisionManifestJson;
	TSharedPtr<FJsonObject> GraphWriteCandidateArtifactJson;
	TSharedPtr<FJsonObject> ExecutionReceiptJson;
	FString GraphWriteCandidateArtifactHash;
	FString ErrorCode;
	FString ErrorMessage;
	FString ErrorField;
};

class FBlueprintHelperTaskPreviewStore
{
public:
	FBlueprintHelperTaskPreviewStore(
		int32 InMaxEntries = 64,
		FTimespan InTimeToLive = FTimespan::FromMinutes(10.0));

	FString Store(const FBlueprintHelperTaskPreviewStoreCreateRequest& Request);

	FBlueprintHelperTaskPreviewStoreResolveResult Resolve(
		const FString& Token,
		const FString& TaskSpecHash);

private:
	struct FEntry
	{
		FString Token;
		FString TaskSpecHash;
		FString TaskPlanHash;
		FString ExecutionPolicyHash;
		FString AssetStateHash;
		FString ActionContextRevisionManifestHash;
		TSharedPtr<FJsonObject> ActionContextRevisionManifestJson;
		TSharedPtr<FJsonObject> GraphWriteCandidateArtifactJson;
		TSharedPtr<FJsonObject> ExecutionReceiptJson;
		FString GraphWriteCandidateArtifactHash;
		FString CreatedAtIso;
		FDateTime ExpiresAtUtc;
		FDateTime LastAccessedAtUtc;
		bool bPassed = false;
		TSharedPtr<FJsonObject> TaskPlan;
	};

	FString GenerateToken() const;
	bool IsTokenFormatValid(const FString& Token) const;
	void PruneExpired();
	void TrimToBounds();
	TSharedPtr<FJsonObject> CloneTaskPlan(const TSharedPtr<FJsonObject>& Source) const;
	TSharedPtr<FJsonObject> CloneJsonObject(const TSharedPtr<FJsonObject>& Source) const;

	int32 MaxEntries = 64;
	FTimespan TimeToLive = FTimespan::FromMinutes(10.0);
	TMap<FString, FEntry> Entries;
};
