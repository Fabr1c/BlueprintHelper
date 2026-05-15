// Store for persisted DebugCase summaries and local diagnostics.

#pragma once

#include "CoreMinimal.h"
#include "Shared/Debug/BlueprintHelperDebugTypes.h"

class FBlueprintHelperReviewStoreService;

class BLUEPRINTHELPER_API FBlueprintHelperDebugCaseStoreService
{
public:
	bool SaveCase(const FBlueprintHelperDebugCase& DebugCase, FString* OutError = nullptr) const;
	bool LoadCase(const FString& DebugCaseId, FBlueprintHelperDebugCase& OutCase, FString* OutError = nullptr) const;
	bool DeleteCase(const FString& DebugCaseId, FString* OutError = nullptr) const;
	bool DeleteCasesForReviewRecord(
		const FString& ReviewRecordId,
		TArray<FString>& OutDeletedCaseIds,
		FString* OutError = nullptr) const;
	bool QueryCaseSummary(const FString& DebugCaseId, FBlueprintHelperDebugCaseSummary& OutSummary, FString* OutError = nullptr) const;
	bool QueryCaseSummaries(TArray<FBlueprintHelperDebugCaseSummary>& OutSummaries, FString* OutError = nullptr) const;
	bool ExportDebugBundleSummary(
		const FString& DebugCaseId,
		FBlueprintHelperDebugBundleManifest& OutManifest,
		FString* OutError = nullptr) const;
	bool ExportDebugBundleSummary(
		const FString& DebugCaseId,
		const FBlueprintHelperReviewStoreService* ReviewStore,
		FBlueprintHelperDebugBundleManifest& OutManifest,
		FString* OutError = nullptr) const;
	bool CleanupResolvedLowSeverityCases(TArray<FString>& OutArchivedCaseIds, FString* OutError = nullptr) const;

	static FString GetDebugRootDir();
	static FString GetCaseDirectory();
	static FString GetCasePath(const FString& DebugCaseId);
	static FString GetBundleDirectory(const FString& BundleId);
	static FString GetBundleManifestPath(const FString& BundleId);

private:
	static bool IsSafeDebugCaseId(const FString& DebugCaseId);
	static bool IsPathInsideDirectory(const FString& Path, const FString& Directory);
	static bool IsPathInsideDebugRoot(const FString& Path);
	static FBlueprintHelperDebugCaseSummary BuildSummary(const FBlueprintHelperDebugCase& DebugCase);
	static void BuildSkippedArtifacts(const FBlueprintHelperDebugCase& DebugCase, FBlueprintHelperDebugBundleManifest& Manifest);
	static FString BuildMarkdownSummary(
		const FBlueprintHelperDebugCaseSummary& Summary,
		const FBlueprintHelperDebugBundleManifest& Manifest);
	static bool EnsureBundleArtifactDirectories(const FString& BundleDir, FString* OutError);
	static bool WriteJsonArtifact(
		const FString& BundleDir,
		const FString& ArtifactRef,
		const TSharedRef<FJsonObject>& Json,
		FString* OutError);
	static bool ExportDebugCaseSummaryArtifact(
		const FString& BundleDir,
		const FBlueprintHelperDebugCaseSummary& Summary,
		FBlueprintHelperDebugBundleManifest& Manifest,
		FString* OutError);
	static bool ExportTransactionSummaryArtifacts(
		const FBlueprintHelperDebugCase& DebugCase,
		const FString& BundleDir,
		FBlueprintHelperDebugBundleManifest& Manifest,
		FString* OutError);
	static bool ExportFragmentSummaryArtifact(
		const FBlueprintHelperDebugCase& DebugCase,
		const FString& BundleDir,
		FBlueprintHelperDebugBundleManifest& Manifest,
		FString* OutError);
	static FString MakeSafeArtifactFileName(const FString& RawId);
	static bool ExportReviewSummaryArtifacts(
		const FBlueprintHelperDebugCase& DebugCase,
		const FString& BundleDir,
		const FBlueprintHelperReviewStoreService* ReviewStore,
		FBlueprintHelperDebugBundleManifest& Manifest,
		FString* OutError);
	static bool IsCleanupProtectedCase(const FBlueprintHelperDebugCase& DebugCase);
	static void SetError(FString* OutError, const FString& Error);
};
