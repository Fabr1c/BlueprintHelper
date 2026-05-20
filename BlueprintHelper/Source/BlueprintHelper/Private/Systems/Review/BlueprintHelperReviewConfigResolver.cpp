// BlueprintHelper Review runtime config resolver implementation.

#include "Systems/Review/BlueprintHelperReviewConfigResolver.h"

#include "Misc/Paths.h"
#include "Systems/Config/BlueprintHelperRuntimeSettingResolver.h"

namespace
{
	static FString BlueprintHelperResolveProjectPath(FString Path, const FString& DefaultRelativePath)
	{
		Path.TrimStartAndEndInline();
		if (Path.IsEmpty())
		{
			Path = DefaultRelativePath;
		}

		FString ResolvedPath = FPaths::IsRelative(Path)
			? FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / Path)
			: Path;
		FPaths::NormalizeFilename(ResolvedPath);
		FPaths::CollapseRelativeDirectories(ResolvedPath);
		return ResolvedPath;
	}

	static FString BlueprintHelperNormalizeVersion(FString Version)
	{
		Version.TrimStartAndEndInline();
		return Version.IsEmpty() ? FString(TEXT("v2")) : Version;
	}
}

FString FBlueprintHelperReviewDebugBundleConfig::GetBundleDir() const
{
	FString Directory = RootDir / SubDir;
	FPaths::NormalizeDirectoryName(Directory);
	FPaths::CollapseRelativeDirectories(Directory);
	return Directory;
}

FString FBlueprintHelperReviewConfig::BuildVersionedSchema(const FString& BaseName) const
{
	return FString::Printf(TEXT("%s.%s"), *BaseName, *BlueprintHelperNormalizeVersion(Version));
}

FString FBlueprintHelperReviewConfig::MakeReviewRecordSchema() const
{
	return BuildVersionedSchema(TEXT("BlueprintHelper.ReviewRecord"));
}

FString FBlueprintHelperReviewConfig::MakeArchiveSessionSchema() const
{
	return BuildVersionedSchema(TEXT("BlueprintHelper.ArchiveSession"));
}

FString FBlueprintHelperReviewConfig::MakeBaselineSemanticSnapshotSchema() const
{
	return BuildVersionedSchema(TEXT("BlueprintHelper.ReviewBaselineSemanticSnapshot"));
}

FString FBlueprintHelperReviewConfig::GetReviewRootDir() const
{
	FString SnapshotRoot = Artifact.SnapshotRoot;
	FPaths::NormalizeDirectoryName(SnapshotRoot);
	FPaths::CollapseRelativeDirectories(SnapshotRoot);
	if (SnapshotRoot.EndsWith(TEXT("/Snapshots"), ESearchCase::IgnoreCase))
	{
		return FPaths::GetPath(SnapshotRoot);
	}
	return SnapshotRoot;
}

FString FBlueprintHelperReviewConfig::GetReviewRecordsDir() const
{
	return GetReviewRootDir() / TEXT("Records");
}

FBlueprintHelperReviewConfig FBlueprintHelperReviewConfigResolver::Load()
{
	FBlueprintHelperReviewConfig Config;
	Config.Version = BlueprintHelperNormalizeVersion(FBlueprintHelperRuntimeSettingResolver::GetString(
		TEXT("review.version"),
		Config.Version));
	Config.bEvidenceRequired = FBlueprintHelperRuntimeSettingResolver::GetBool(
		TEXT("review.evidence_required"),
		Config.bEvidenceRequired);
	Config.Artifact.SnapshotRoot = BlueprintHelperResolveProjectPath(
		FBlueprintHelperRuntimeSettingResolver::GetString(TEXT("review.artifact.snapshot_root"), Config.Artifact.SnapshotRoot),
		TEXT("Saved/BlueprintHelper/Review/Snapshots"));
	Config.DebugBundle.RootDir = BlueprintHelperResolveProjectPath(
		FBlueprintHelperRuntimeSettingResolver::GetString(TEXT("review.debug_bundle.root_dir"), Config.DebugBundle.RootDir),
		TEXT("Saved/BlueprintHelper/Debug"));
	Config.DebugBundle.SubDir = FBlueprintHelperRuntimeSettingResolver::GetString(
		TEXT("review.debug_bundle.sub_dir"),
		Config.DebugBundle.SubDir);
	Config.DebugBundle.SubDir.TrimStartAndEndInline();
	if (Config.DebugBundle.SubDir.IsEmpty())
	{
		Config.DebugBundle.SubDir = TEXT("ReviewPanelBundles");
	}
	Config.DebugBundle.FilenamePattern = FBlueprintHelperRuntimeSettingResolver::GetString(
		TEXT("review.debug_bundle.filename_pattern"),
		Config.DebugBundle.FilenamePattern);
	Config.DebugBundle.FilenamePattern.TrimStartAndEndInline();
	if (Config.DebugBundle.FilenamePattern.IsEmpty())
	{
		Config.DebugBundle.FilenamePattern = TEXT("review_panel_%Y%m%d_%H%M%S.json");
	}
	Config.DebugBundle.SchemaReviewPanel = FBlueprintHelperRuntimeSettingResolver::GetString(
		TEXT("review.debug_bundle.schema_review_panel"),
		Config.DebugBundle.SchemaReviewPanel);
	Config.DebugBundle.SchemaSnapshot = FBlueprintHelperRuntimeSettingResolver::GetString(
		TEXT("review.debug_bundle.schema_snapshot"),
		Config.DebugBundle.SchemaSnapshot);
	Config.DebugBundle.HashSource = FBlueprintHelperRuntimeSettingResolver::GetString(
		TEXT("review.debug_bundle.hash_source"),
		Config.DebugBundle.HashSource);
	Config.DebugBundle.Retention = FBlueprintHelperRuntimeSettingResolver::GetString(
		TEXT("review.debug_bundle.retention"),
		Config.DebugBundle.Retention);
	Config.DebugBundle.bEnforceRootPath = FBlueprintHelperRuntimeSettingResolver::GetBool(
		TEXT("review.debug_bundle.enforce_root_path"),
		Config.DebugBundle.bEnforceRootPath);
	return Config;
}
