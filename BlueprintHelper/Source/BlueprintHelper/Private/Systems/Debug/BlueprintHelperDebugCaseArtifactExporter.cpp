// DebugCase artifact file exporter.

#include "Systems/Debug/BlueprintHelperDebugCaseArtifactExporter.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Systems/Debug/BlueprintHelperDebugCaseStoreService.h"
#include "Systems/Debug/BlueprintHelperDebugExportPolicyResolver.h"

void FBlueprintHelperDebugCaseArtifactExporter::SetError(FString* OutError, const FString& Error)
{
	if (OutError)
	{
		*OutError = Error;
	}
}

bool FBlueprintHelperDebugCaseArtifactExporter::IsPathInsideDirectory(
	const FString& Path,
	const FString& Directory)
{
	FString NormalizedPath = Path;
	FString NormalizedRoot = Directory;
	FPaths::NormalizeFilename(NormalizedPath);
	FPaths::NormalizeDirectoryName(NormalizedRoot);
	FPaths::CollapseRelativeDirectories(NormalizedPath);
	FPaths::CollapseRelativeDirectories(NormalizedRoot);

	if (NormalizedPath.Equals(NormalizedRoot, ESearchCase::IgnoreCase))
	{
		return true;
	}

	const FString RootPrefix = NormalizedRoot.EndsWith(TEXT("/"))
		? NormalizedRoot
		: NormalizedRoot + TEXT("/");
	return NormalizedPath.StartsWith(RootPrefix, ESearchCase::IgnoreCase);
}

bool FBlueprintHelperDebugCaseArtifactExporter::IsPathInsideDebugRoot(const FString& Path)
{
	return IsPathInsideDirectory(Path, FBlueprintHelperDebugCaseStoreService::GetDebugRootDir());
}

bool FBlueprintHelperDebugCaseArtifactExporter::EnsureBundleArtifactDirectories(
	const FString& BundleDir,
	FString* OutError)
{
	const TArray<FString> RequiredDirs = {
		BundleDir / TEXT("artifacts"),
		BundleDir / TEXT("artifacts") / TEXT("review"),
		BundleDir / TEXT("artifacts") / TEXT("evidence"),
		BundleDir / TEXT("artifacts") / TEXT("assets"),
		BundleDir / TEXT("artifacts") / TEXT("logs")
	};

	for (const FString& Dir : RequiredDirs)
	{
		if (!IsPathInsideDebugRoot(Dir))
		{
			SetError(OutError, TEXT("debug bundle artifact path escaped Saved/BlueprintHelper/Debug"));
			return false;
		}
		if (!IFileManager::Get().DirectoryExists(*Dir)
			&& !IFileManager::Get().MakeDirectory(*Dir, true))
		{
			SetError(OutError, TEXT("failed to create debug bundle artifact directory"));
			return false;
		}
	}

	SetError(OutError, FString());
	return true;
}

bool FBlueprintHelperDebugCaseArtifactExporter::WriteJsonArtifact(
	const FString& BundleDir,
	const FString& ArtifactRef,
	const TSharedRef<FJsonObject>& Json,
	FString* OutError)
{
	if (ArtifactRef.IsEmpty() || !FPaths::IsRelative(ArtifactRef))
	{
		SetError(OutError, TEXT("debug artifact ref must be relative"));
		return false;
	}

	const FString ArtifactPath = BundleDir / ArtifactRef;
	if (!IsPathInsideDirectory(ArtifactPath, BundleDir) || !IsPathInsideDebugRoot(ArtifactPath))
	{
		SetError(OutError, TEXT("debug artifact path escaped debug bundle directory"));
		return false;
	}

	const FString ArtifactDir = FPaths::GetPath(ArtifactPath);
	if (!IFileManager::Get().DirectoryExists(*ArtifactDir)
		&& !IFileManager::Get().MakeDirectory(*ArtifactDir, true))
	{
		SetError(OutError, TEXT("failed to create debug artifact directory"));
		return false;
	}

	TSharedRef<FJsonObject> Sanitized = FBlueprintHelperDebugJson::SanitizeObject(Json);
	FString JsonText;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	if (!FJsonSerializer::Serialize(Sanitized, Writer))
	{
		SetError(OutError, TEXT("failed to serialize debug artifact"));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(
		JsonText,
		*ArtifactPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		SetError(OutError, TEXT("failed to write debug artifact"));
		return false;
	}

	SetError(OutError, FString());
	return true;
}

bool FBlueprintHelperDebugCaseArtifactExporter::WriteMarkdownArtifact(
	const FString& BundleDir,
	const FString& ArtifactRef,
	const FString& Markdown,
	FString* OutError)
{
	if (ArtifactRef.IsEmpty() || !FPaths::IsRelative(ArtifactRef))
	{
		SetError(OutError, TEXT("debug markdown artifact ref must be relative"));
		return false;
	}

	const FString ArtifactPath = BundleDir / ArtifactRef;
	if (!IsPathInsideDirectory(ArtifactPath, BundleDir) || !IsPathInsideDebugRoot(ArtifactPath))
	{
		SetError(OutError, TEXT("debug markdown artifact path escaped debug bundle directory"));
		return false;
	}

	const FString ArtifactDir = FPaths::GetPath(ArtifactPath);
	if (!IFileManager::Get().DirectoryExists(*ArtifactDir)
		&& !IFileManager::Get().MakeDirectory(*ArtifactDir, true))
	{
		SetError(OutError, TEXT("failed to create debug markdown artifact directory"));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(
		Markdown,
		*ArtifactPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		SetError(OutError, TEXT("failed to write debug markdown artifact"));
		return false;
	}

	SetError(OutError, FString());
	return true;
}

bool FBlueprintHelperDebugCaseArtifactExporter::ExportBundle(
	const FBlueprintHelperDebugCase& DebugCase,
	const FBlueprintHelperDebugCaseProjectionResult& ProjectionResult,
	FBlueprintHelperDebugBundleManifest& OutManifest,
	FString* OutError) const
{
	OutManifest = FBlueprintHelperDebugBundleManifest();
	OutManifest.BundleId = TEXT("bundle_") + DebugCase.DebugCaseId + TEXT("_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	OutManifest.DebugCaseId = DebugCase.DebugCaseId;
	OutManifest.CreatedAt = FDateTime::UtcNow().ToIso8601();
	const FBlueprintHelperDebugExportPolicy ExportPolicy = FBlueprintHelperDebugExportPolicyResolver::Load();
	OutManifest.ExportProfile = ExportPolicy.ExportProfile;
	OutManifest.bContainsFullSettings = ExportPolicy.bContainsFullSettings;
	OutManifest.SkippedArtifacts = ProjectionResult.SkippedArtifacts;
	OutManifest.FragmentArtifacts = ProjectionResult.FragmentArtifacts;
	OutManifest.SummaryRef = TEXT("summary.md");
	OutManifest.Contents.Add(TEXT("manifest.json"));

	const FString BundleDir = FBlueprintHelperDebugCaseStoreService::GetBundleDirectory(OutManifest.BundleId);
	if (!IsPathInsideDebugRoot(BundleDir))
	{
		SetError(OutError, TEXT("debug bundle path escaped Saved/BlueprintHelper/Debug"));
		return false;
	}
	if (!IFileManager::Get().DirectoryExists(*BundleDir)
		&& !IFileManager::Get().MakeDirectory(*BundleDir, true))
	{
		SetError(OutError, TEXT("failed to create debug bundle directory"));
		return false;
	}
	if (!EnsureBundleArtifactDirectories(BundleDir, OutError))
	{
		return false;
	}

	for (const FBlueprintHelperDebugCaseArtifactModel& Artifact : ProjectionResult.Artifacts)
	{
		if (Artifact.RelativePath.IsEmpty())
		{
			continue;
		}

		if (Artifact.Role == EBlueprintHelperDebugCaseArtifactRole::SummaryMarkdown)
		{
			if (!WriteMarkdownArtifact(BundleDir, Artifact.RelativePath, Artifact.Markdown, OutError))
			{
				return false;
			}
			OutManifest.SummaryRef = Artifact.RelativePath;
		}
		else if (Artifact.Json.IsValid())
		{
			if (!WriteJsonArtifact(BundleDir, Artifact.RelativePath, Artifact.Json.ToSharedRef(), OutError))
			{
				return false;
			}
		}
		else if (!Artifact.Markdown.IsEmpty())
		{
			if (!WriteMarkdownArtifact(BundleDir, Artifact.RelativePath, Artifact.Markdown, OutError))
			{
				return false;
			}
		}
		else
		{
			continue;
		}

		OutManifest.Contents.AddUnique(Artifact.RelativePath);
		if (Artifact.Role == EBlueprintHelperDebugCaseArtifactRole::ReviewSummary
			&& Artifact.RelativePath.StartsWith(TEXT("artifacts/review/"))
			&& Artifact.RelativePath.EndsWith(TEXT(".summary.json")))
		{
			OutManifest.ReviewSummaryRefs.AddUnique(Artifact.RelativePath);
		}
	}

	if (!OutManifest.Contents.Contains(OutManifest.SummaryRef))
	{
		OutManifest.Contents.AddUnique(OutManifest.SummaryRef);
	}

	FString ManifestText;
	TSharedRef<TJsonWriter<>> ManifestWriter = TJsonWriterFactory<>::Create(&ManifestText);
	if (!FJsonSerializer::Serialize(OutManifest.ToJson(), ManifestWriter))
	{
		SetError(OutError, TEXT("failed to serialize debug bundle manifest"));
		return false;
	}
	if (!FFileHelper::SaveStringToFile(
		ManifestText,
		*FBlueprintHelperDebugCaseStoreService::GetBundleManifestPath(OutManifest.BundleId),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		SetError(OutError, TEXT("failed to write debug bundle manifest"));
		return false;
	}

	SetError(OutError, FString());
	return true;
}
