#include "Systems/Debug/BlueprintHelperDebugCaseStoreService.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Systems/Review/BlueprintHelperReviewConfigResolver.h"

void FBlueprintHelperDebugCaseStoreService::SetError(FString* OutError, const FString& Error)
{
	if (OutError)
	{
		*OutError = Error;
	}
}

FString FBlueprintHelperDebugCaseStoreService::GetDebugRootDir()
{
	return FBlueprintHelperReviewConfigResolver::Load().DebugBundle.RootDir;
}

FString FBlueprintHelperDebugCaseStoreService::GetCaseDirectory()
{
	return GetDebugRootDir() / TEXT("Cases");
}

FString FBlueprintHelperDebugCaseStoreService::GetCasePath(const FString& DebugCaseId)
{
	return GetCaseDirectory() / FString::Printf(TEXT("%s.json"), *DebugCaseId);
}

FString FBlueprintHelperDebugCaseStoreService::GetBundleDirectory(const FString& BundleId)
{
	return GetDebugRootDir() / TEXT("Bundles") / BundleId;
}

FString FBlueprintHelperDebugCaseStoreService::GetBundleManifestPath(const FString& BundleId)
{
	return GetBundleDirectory(BundleId) / TEXT("manifest.json");
}

bool FBlueprintHelperDebugCaseStoreService::IsSafeDebugCaseId(const FString& DebugCaseId)
{
	if (DebugCaseId.IsEmpty())
	{
		return false;
	}

	for (const TCHAR Ch : DebugCaseId)
	{
		if (!(FChar::IsAlnum(Ch) || Ch == TEXT('_') || Ch == TEXT('-')))
		{
			return false;
		}
	}
	return true;
}

bool FBlueprintHelperDebugCaseStoreService::IsCleanupProtectedCase(const FBlueprintHelperDebugCase& DebugCase)
{
	const FString SourceLower = DebugCase.Source.ToLower();
	const FString ErrorCodeLower = DebugCase.Error.Code.ToLower();
	return DebugCase.Status == EBlueprintHelperDebugCaseStatus::NeedsAction
		|| SourceLower.Contains(TEXT("rollback"))
		|| ErrorCodeLower.Contains(TEXT("rollback"));
}

bool FBlueprintHelperDebugCaseStoreService::IsPathInsideDirectory(const FString& Path, const FString& Directory)
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

bool FBlueprintHelperDebugCaseStoreService::IsPathInsideDebugRoot(const FString& Path)
{
	return IsPathInsideDirectory(Path, GetDebugRootDir());
}

bool FBlueprintHelperDebugCaseStoreService::SaveCase(const FBlueprintHelperDebugCase& DebugCase, FString* OutError) const
{
	if (!IsSafeDebugCaseId(DebugCase.DebugCaseId))
	{
		SetError(OutError, TEXT("invalid debug_case_id"));
		return false;
	}

	const FString CaseDir = GetCaseDirectory();
	if (!IFileManager::Get().DirectoryExists(*CaseDir))
	{
		IFileManager::Get().MakeDirectory(*CaseDir, true);
	}

	const FString CasePath = GetCasePath(DebugCase.DebugCaseId);
	if (!IsPathInsideDebugRoot(CasePath))
	{
		SetError(OutError, TEXT("debug case path escaped Saved/BlueprintHelper/Debug"));
		return false;
	}

	FString JsonText;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	if (!FJsonSerializer::Serialize(DebugCase.ToJson(), Writer))
	{
		SetError(OutError, TEXT("failed to serialize debug case"));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(JsonText, *CasePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		SetError(OutError, TEXT("failed to write debug case"));
		return false;
	}

	SetError(OutError, FString());
	return true;
}

bool FBlueprintHelperDebugCaseStoreService::LoadCase(const FString& DebugCaseId, FBlueprintHelperDebugCase& OutCase, FString* OutError) const
{
	if (!IsSafeDebugCaseId(DebugCaseId))
	{
		SetError(OutError, TEXT("invalid debug_case_id"));
		return false;
	}

	const FString CasePath = GetCasePath(DebugCaseId);
	if (!IsPathInsideDebugRoot(CasePath))
	{
		SetError(OutError, TEXT("debug case path escaped Saved/BlueprintHelper/Debug"));
		return false;
	}

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *CasePath))
	{
		SetError(OutError, TEXT("debug case not found"));
		return false;
	}

	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
	{
		SetError(OutError, TEXT("debug case json is invalid"));
		return false;
	}

	OutCase = FBlueprintHelperDebugCase::FromJson(Json);
	SetError(OutError, FString());
	return true;
}

bool FBlueprintHelperDebugCaseStoreService::DeleteCase(const FString& DebugCaseId, FString* OutError) const
{
	if (!IsSafeDebugCaseId(DebugCaseId))
	{
		SetError(OutError, TEXT("invalid debug_case_id"));
		return false;
	}

	const FString CasePath = GetCasePath(DebugCaseId);
	if (!IsPathInsideDebugRoot(CasePath))
	{
		SetError(OutError, TEXT("debug case path escaped Saved/BlueprintHelper/Debug"));
		return false;
	}

	if (!IFileManager::Get().FileExists(*CasePath))
	{
		SetError(OutError, FString());
		return true;
	}

	if (!IFileManager::Get().Delete(*CasePath, false, true))
	{
		SetError(OutError, FString::Printf(TEXT("failed to delete debug case: %s"), *DebugCaseId));
		return false;
	}

	SetError(OutError, FString());
	return true;
}

bool FBlueprintHelperDebugCaseStoreService::DeleteCasesForReviewRecord(
	const FString& ReviewRecordId,
	TArray<FString>& OutDeletedCaseIds,
	FString* OutError) const
{
	OutDeletedCaseIds.Reset();
	if (ReviewRecordId.IsEmpty())
	{
		SetError(OutError, TEXT("review_record_id is required"));
		return false;
	}

	TArray<FBlueprintHelperDebugCaseSummary> Summaries;
	if (!QueryCaseSummaries(Summaries, OutError))
	{
		return false;
	}

	for (const FBlueprintHelperDebugCaseSummary& Summary : Summaries)
	{
		if (!Summary.ReviewRecordIds.Contains(ReviewRecordId))
		{
			continue;
		}

		FString DeleteError;
		if (!DeleteCase(Summary.DebugCaseId, &DeleteError))
		{
			SetError(OutError, DeleteError);
			return false;
		}
		OutDeletedCaseIds.AddUnique(Summary.DebugCaseId);
	}

	SetError(OutError, FString());
	return true;
}

FBlueprintHelperDebugCaseSummary FBlueprintHelperDebugCaseStoreService::BuildSummary(const FBlueprintHelperDebugCase& DebugCase)
{
	FBlueprintHelperDebugCaseSummary Summary;
	Summary.DebugCaseId = DebugCase.DebugCaseId;
	Summary.CreatedAt = DebugCase.CreatedAt;
	Summary.UpdatedAt = DebugCase.UpdatedAt;
	Summary.Source = DebugCase.Source;
	Summary.Severity = DebugCase.Severity;
	Summary.Status = DebugCase.Status;
	Summary.Operation = DebugCase.Operation;
	Summary.Stage = DebugCase.Stage;
	Summary.TraceIds = DebugCase.TraceIds;
	Summary.TaskRunId = DebugCase.TaskRunId;
	Summary.AssetPaths = DebugCase.AssetPaths;
	Summary.ReviewRecordIds = DebugCase.ReviewRecordIds;
	Summary.EvidenceLinks = DebugCase.EvidenceLinks;
	Summary.Error = DebugCase.Error;
	Summary.RecommendedNext = DebugCase.RecommendedNext;
	Summary.FragmentArtifacts = DebugCase.FragmentArtifacts;
	Summary.EventCount = DebugCase.Events.Num();
	if (Summary.RecommendedNext.IsEmpty() && DebugCase.Events.Num() > 0)
	{
		Summary.RecommendedNext = DebugCase.Events.Last().RecommendedNext;
	}
	return Summary;
}

bool FBlueprintHelperDebugCaseStoreService::QueryCaseSummary(
	const FString& DebugCaseId,
	FBlueprintHelperDebugCaseSummary& OutSummary,
	FString* OutError) const
{
	FBlueprintHelperDebugCase DebugCase;
	if (!LoadCase(DebugCaseId, DebugCase, OutError))
	{
		return false;
	}

	OutSummary = BuildSummary(DebugCase);
	SetError(OutError, FString());
	return true;
}

bool FBlueprintHelperDebugCaseStoreService::QueryCaseSummaries(
	TArray<FBlueprintHelperDebugCaseSummary>& OutSummaries,
	FString* OutError) const
{
	OutSummaries.Reset();
	const FString CaseDir = GetCaseDirectory();
	if (!IFileManager::Get().DirectoryExists(*CaseDir))
	{
		SetError(OutError, FString());
		return true;
	}

	TArray<FString> CaseFiles;
	IFileManager::Get().FindFiles(CaseFiles, *(CaseDir / TEXT("*.json")), true, false);
	for (const FString& CaseFile : CaseFiles)
	{
		const FString DebugCaseId = FPaths::GetBaseFilename(CaseFile);
		FBlueprintHelperDebugCaseSummary Summary;
		FString LoadError;
		if (QueryCaseSummary(DebugCaseId, Summary, &LoadError))
		{
			OutSummaries.Add(Summary);
		}
	}

	OutSummaries.Sort([](
		const FBlueprintHelperDebugCaseSummary& A,
		const FBlueprintHelperDebugCaseSummary& B)
	{
		return A.UpdatedAt > B.UpdatedAt;
	});

	SetError(OutError, FString());
	return true;
}

bool FBlueprintHelperDebugCaseStoreService::CleanupResolvedLowSeverityCases(
	TArray<FString>& OutArchivedCaseIds,
	FString* OutError) const
{
	OutArchivedCaseIds.Reset();
	TArray<FBlueprintHelperDebugCaseSummary> Summaries;
	if (!QueryCaseSummaries(Summaries, OutError))
	{
		return false;
	}

	for (const FBlueprintHelperDebugCaseSummary& Summary : Summaries)
	{
		FBlueprintHelperDebugCase DebugCase;
		FString LoadError;
		if (!LoadCase(Summary.DebugCaseId, DebugCase, &LoadError))
		{
			continue;
		}

		if (DebugCase.Status != EBlueprintHelperDebugCaseStatus::Resolved
			|| DebugCase.Severity == EBlueprintHelperDebugSeverity::Error
			|| IsCleanupProtectedCase(DebugCase))
		{
			continue;
		}

		DebugCase.Status = EBlueprintHelperDebugCaseStatus::Archived;
		DebugCase.UpdatedAt = FDateTime::UtcNow().ToIso8601();
		FString SaveError;
		if (SaveCase(DebugCase, &SaveError))
		{
			OutArchivedCaseIds.Add(DebugCase.DebugCaseId);
		}
	}

	SetError(OutError, FString());
	return true;
}
