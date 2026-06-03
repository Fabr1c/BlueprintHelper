#pragma once

#include "CoreMinimal.h"

class FJsonObject;

struct FBlueprintHelperTaskRuntimeContextRevisionEntry
{
	FString AssetPath;
	FString GraphName;
	bool bGraphExists = false;
	int32 BlueprintRevision = 0;
	int32 GraphRevision = 0;

	FString StableKey() const;
	FString ToStableString() const;
	TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperTaskRuntimeContextRevisionMismatch
{
	FString Code = TEXT("context_stale");
	FString DetailCode = TEXT("action_context_stale");
	FString Message;
	FString Field = TEXT("preview_token.context_revision");
	TSharedPtr<FJsonObject> Expected;
	TSharedPtr<FJsonObject> Current;
};

struct FBlueprintHelperTaskRuntimeContextRevisionManifest
{
	TArray<FBlueprintHelperTaskRuntimeContextRevisionEntry> Entries;
	FString ManifestHash;

	void RecomputeHash();
	TSharedRef<FJsonObject> ToJson() const;

	static bool Compare(
		const FBlueprintHelperTaskRuntimeContextRevisionManifest& Expected,
		const FBlueprintHelperTaskRuntimeContextRevisionManifest& Current,
		FBlueprintHelperTaskRuntimeContextRevisionMismatch& OutMismatch);
};

class FBlueprintHelperTaskRuntimeContextRevisionManifestBuilder
{
public:
	static FBlueprintHelperTaskRuntimeContextRevisionManifest BuildFromTaskPlan(
		const TSharedPtr<FJsonObject>& TaskPlan);

	static FBlueprintHelperTaskRuntimeContextRevisionManifest BuildFromJson(
		const TSharedPtr<FJsonObject>& Json);
};
