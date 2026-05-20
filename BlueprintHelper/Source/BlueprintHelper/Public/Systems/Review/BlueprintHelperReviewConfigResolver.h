// BlueprintHelper Review runtime config resolver.

#pragma once

#include "CoreMinimal.h"

struct FBlueprintHelperReviewArtifactConfig
{
	FString SnapshotRoot = TEXT("Saved/BlueprintHelper/Review/Snapshots");
};

struct FBlueprintHelperReviewDebugBundleConfig
{
	FString RootDir = TEXT("Saved/BlueprintHelper/Debug");
	FString SubDir = TEXT("ReviewPanelBundles");
	FString FilenamePattern = TEXT("review_panel_%Y%m%d_%H%M%S.json");
	FString SchemaReviewPanel = TEXT("BlueprintHelper.ReviewPanelDebugBundle.v2");
	FString SchemaSnapshot = TEXT("BlueprintHelper.ReviewTargetSnapshot.v2");
	FString HashSource = TEXT("semantic_target_snapshot");
	FString Retention = TEXT("standard");
	bool bEnforceRootPath = true;

	FString GetBundleDir() const;
};

struct FBlueprintHelperReviewConfig
{
	FString Version = TEXT("v2");
	bool bEvidenceRequired = true;
	FBlueprintHelperReviewArtifactConfig Artifact;
	FBlueprintHelperReviewDebugBundleConfig DebugBundle;

	FString BuildVersionedSchema(const FString& BaseName) const;
	FString MakeReviewRecordSchema() const;
	FString MakeArchiveSessionSchema() const;
	FString MakeBaselineSemanticSnapshotSchema() const;
	FString GetReviewRootDir() const;
	FString GetReviewRecordsDir() const;
};

class BLUEPRINTHELPER_API FBlueprintHelperReviewConfigResolver
{
public:
	static FBlueprintHelperReviewConfig Load();
};
