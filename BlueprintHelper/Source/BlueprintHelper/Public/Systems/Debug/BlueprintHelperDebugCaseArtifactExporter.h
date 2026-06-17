// DebugCase artifact file exporter.

#pragma once

#include "CoreMinimal.h"
#include "Systems/Debug/BlueprintHelperDebugCaseProjectionAdapter.h"

class BLUEPRINTHELPER_API FBlueprintHelperDebugCaseArtifactExporter
{
public:
	bool ExportBundle(
		const FBlueprintHelperDebugCase& DebugCase,
		const FBlueprintHelperDebugCaseProjectionResult& ProjectionResult,
		FBlueprintHelperDebugBundleManifest& OutManifest,
		FString* OutError) const;

private:
	static void SetError(FString* OutError, const FString& Error);
	static bool IsPathInsideDirectory(const FString& Path, const FString& Directory);
	static bool IsPathInsideDebugRoot(const FString& Path);
	static bool EnsureBundleArtifactDirectories(const FString& BundleDir, FString* OutError);
	static bool WriteJsonArtifact(
		const FString& BundleDir,
		const FString& ArtifactRef,
		const TSharedRef<FJsonObject>& Json,
		FString* OutError);
	static bool WriteMarkdownArtifact(
		const FString& BundleDir,
		const FString& ArtifactRef,
		const FString& Markdown,
		FString* OutError);
};
