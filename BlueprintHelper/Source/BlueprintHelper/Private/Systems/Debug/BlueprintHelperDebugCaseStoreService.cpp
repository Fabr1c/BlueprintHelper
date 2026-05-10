#include "Systems/Debug/BlueprintHelperDebugCaseStoreService.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"

void FBlueprintHelperDebugCaseStoreService::SetError(FString* OutError, const FString& Error)
{
	if (OutError)
	{
		*OutError = Error;
	}
}

FString FBlueprintHelperDebugCaseStoreService::GetDebugRootDir()
{
	return FPaths::ProjectSavedDir() / TEXT("BlueprintHelper") / TEXT("Debug");
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

void FBlueprintHelperDebugCaseStoreService::BuildSkippedArtifacts(
	const FBlueprintHelperDebugCase& DebugCase,
	FBlueprintHelperDebugBundleManifest& Manifest)
{
	if (DebugCase.Events.Num() > 0)
	{
		FBlueprintHelperDebugSkippedArtifact Skipped;
		Skipped.Artifact = TEXT("debug_case.events");
		Skipped.Reason = TEXT("full event payloads are summary-only");
		Manifest.SkippedArtifacts.Add(Skipped);
	}

	FString SerializedCase;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&SerializedCase);
	FJsonSerializer::Serialize(DebugCase.ToJson(), Writer);
	const FString Lower = SerializedCase.ToLower();
	if (Lower.Contains(TEXT("raw_payload"))
		|| Lower.Contains(TEXT("raw_json"))
		|| Lower.Contains(TEXT("source_file"))
		|| Lower.Contains(TEXT("source_content"))
		|| Lower.Contains(TEXT("token"))
		|| Lower.Contains(TEXT("secret")))
	{
		FBlueprintHelperDebugSkippedArtifact Skipped;
		Skipped.Artifact = TEXT("debug_case.sensitive_fields");
		Skipped.Reason = TEXT("sensitive fields were redacted from summary export");
		Manifest.SkippedArtifacts.Add(Skipped);
	}
}

FString FBlueprintHelperDebugCaseStoreService::BuildMarkdownSummary(
	const FBlueprintHelperDebugCaseSummary& Summary,
	const FBlueprintHelperDebugBundleManifest& Manifest)
{
	FString Markdown;
	auto AppendLine = [&Markdown](const FString& Line)
	{
		Markdown += Line;
		Markdown += LINE_TERMINATOR;
	};
	auto Safe = [](const FString& Value)
	{
		return FBlueprintHelperDebugJson::RedactString(Value);
	};
	auto AppendValue = [&AppendLine, &Safe](const TCHAR* Label, const FString& Value)
	{
		if (!Value.IsEmpty())
		{
			AppendLine(FString::Printf(TEXT("- %s: `%s`"), Label, *Safe(Value)));
		}
	};
	auto AppendStringList = [&AppendLine, &Safe](const TCHAR* Label, const TArray<FString>& Values)
	{
		if (Values.Num() == 0)
		{
			return;
		}
		AppendLine(FString::Printf(TEXT("- %s:"), Label));
		for (const FString& Value : Values)
		{
			if (!Value.IsEmpty())
			{
				AppendLine(FString::Printf(TEXT("  - `%s`"), *Safe(Value)));
			}
		}
	};

	AppendLine(TEXT("# BlueprintHelper Debug Bundle"));
	AppendLine(TEXT(""));
	AppendLine(TEXT("## Debug Case"));
	AppendValue(TEXT("DebugCase"), Summary.DebugCaseId);
	AppendValue(TEXT("Source"), Summary.Source);
	AppendLine(FString::Printf(TEXT("- Severity: `%s`"), BlueprintHelperDebugSeverityToString(Summary.Severity)));
	AppendLine(FString::Printf(TEXT("- Status: `%s`"), BlueprintHelperDebugCaseStatusToString(Summary.Status)));
	AppendValue(TEXT("Operation"), Summary.Operation);
	AppendValue(TEXT("Stage"), Summary.Stage);
	AppendValue(TEXT("TaskRun"), Summary.TaskRunId);
	AppendLine(FString::Printf(TEXT("- Event count: `%d`"), Summary.EventCount));
	AppendStringList(TEXT("Trace ids"), Summary.TraceIds);
	AppendStringList(TEXT("Asset paths"), Summary.AssetPaths);
	AppendStringList(TEXT("Review record ids"), Summary.ReviewRecordIds);

	if (!Summary.Error.Code.IsEmpty() || !Summary.Error.Message.IsEmpty())
	{
		AppendLine(TEXT(""));
		AppendLine(TEXT("## Error"));
		AppendValue(TEXT("Code"), Summary.Error.Code);
		AppendValue(TEXT("Message"), Summary.Error.Message);
	}

	if (Summary.TransactionLinks.Num() > 0)
	{
		AppendLine(TEXT(""));
		AppendLine(TEXT("## Transaction Links"));
		for (const FBlueprintHelperDebugTransactionLink& Link : Summary.TransactionLinks)
		{
			const FString TransactionId = Link.TransactionId.IsEmpty() ? TEXT("<empty>") : Link.TransactionId;
			const FString Role = Link.Role.IsEmpty() ? TEXT("unknown") : Link.Role;
			AppendLine(FString::Printf(TEXT("- `%s` role=`%s`"), *Safe(TransactionId), *Safe(Role)));
		}
	}

	AppendLine(TEXT(""));
	AppendLine(TEXT("## Artifacts"));
	for (const FString& Content : Manifest.Contents)
	{
		if (!Content.IsEmpty() && FPaths::IsRelative(Content))
		{
			AppendLine(FString::Printf(TEXT("- `%s`"), *Safe(Content)));
		}
	}

	if (Manifest.SkippedArtifacts.Num() > 0)
	{
		AppendLine(TEXT(""));
		AppendLine(TEXT("## Skipped Artifacts"));
		for (const FBlueprintHelperDebugSkippedArtifact& Skipped : Manifest.SkippedArtifacts)
		{
			AppendLine(FString::Printf(
				TEXT("- `%s`: %s"),
				*Safe(Skipped.Artifact),
				*Safe(Skipped.Reason)));
		}
	}

	AppendLine(TEXT(""));
	AppendLine(TEXT("## Privacy"));
	AppendLine(TEXT("- Profile: `standard`"));
	AppendLine(TEXT("- Redacted: `true`"));
	AppendLine(TEXT("- Contains tokens: `false`"));
	AppendLine(TEXT("- Contains local absolute paths: `false`"));
	AppendLine(TEXT("- Contains full raw asset json: `false`"));
	AppendLine(TEXT("- Contains source files: `false`"));
	return Markdown;
}

bool FBlueprintHelperDebugCaseStoreService::EnsureBundleArtifactDirectories(
	const FString& BundleDir,
	FString* OutError)
{
	const TArray<FString> RequiredDirs = {
		BundleDir / TEXT("artifacts"),
		BundleDir / TEXT("artifacts") / TEXT("review"),
		BundleDir / TEXT("artifacts") / TEXT("transactions"),
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

bool FBlueprintHelperDebugCaseStoreService::WriteJsonArtifact(
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
	FString NormalizedArtifactPath = ArtifactPath;
	FString NormalizedBundleDir = BundleDir;
	FPaths::NormalizeFilename(NormalizedArtifactPath);
	FPaths::NormalizeDirectoryName(NormalizedBundleDir);
	if (!NormalizedArtifactPath.StartsWith(NormalizedBundleDir)
		|| !IsPathInsideDebugRoot(NormalizedArtifactPath))
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

bool FBlueprintHelperDebugCaseStoreService::ExportDebugCaseSummaryArtifact(
	const FString& BundleDir,
	const FBlueprintHelperDebugCaseSummary& Summary,
	FBlueprintHelperDebugBundleManifest& Manifest,
	FString* OutError)
{
	const FString SummaryArtifactRef = TEXT("artifacts/debug_case.summary.json");
	if (!WriteJsonArtifact(BundleDir, SummaryArtifactRef, Summary.ToJson(), OutError))
	{
		return false;
	}
	Manifest.Contents.AddUnique(SummaryArtifactRef);
	return true;
}

bool FBlueprintHelperDebugCaseStoreService::ExportTransactionSummaryArtifacts(
	const FBlueprintHelperDebugCase& DebugCase,
	const FString& BundleDir,
	FBlueprintHelperDebugBundleManifest& Manifest,
	FString* OutError)
{
	int32 LinkIndex = 0;
	for (const FBlueprintHelperDebugTransactionLink& Link : DebugCase.TransactionLinks)
	{
		++LinkIndex;
		const FString RawId = Link.TransactionId.IsEmpty()
			? FString::Printf(TEXT("transaction_%d"), LinkIndex)
			: Link.TransactionId;
		const FString ArtifactRef = TEXT("artifacts/transactions/")
			+ MakeSafeArtifactFileName(RawId)
			+ TEXT(".summary.json");
		TSharedRef<FJsonObject> TransactionSummary = MakeShared<FJsonObject>();
		TransactionSummary->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.DebugTransactionSummary.v1"));
		TransactionSummary->SetObjectField(TEXT("transaction_link"), Link.ToJson());
		if (!WriteJsonArtifact(BundleDir, ArtifactRef, TransactionSummary, OutError))
		{
			return false;
		}
		Manifest.Contents.AddUnique(ArtifactRef);
	}
	return true;
}

FString FBlueprintHelperDebugCaseStoreService::MakeSafeArtifactFileName(const FString& RawId)
{
	FString Safe;
	for (const TCHAR Ch : RawId)
	{
		if (FChar::IsAlnum(Ch) || Ch == TEXT('_') || Ch == TEXT('-'))
		{
			Safe.AppendChar(Ch);
		}
		else
		{
			Safe.AppendChar(TEXT('_'));
		}
	}
	while (Safe.Contains(TEXT("__")))
	{
		Safe.ReplaceInline(TEXT("__"), TEXT("_"));
	}
	Safe.TrimStartAndEndInline();
	if (Safe.IsEmpty())
	{
		return TEXT("review_record");
	}
	return Safe;
}

bool FBlueprintHelperDebugCaseStoreService::ExportReviewSummaryArtifacts(
	const FBlueprintHelperDebugCase& DebugCase,
	const FString& BundleDir,
	const FBlueprintHelperReviewStoreService* ReviewStore,
	FBlueprintHelperDebugBundleManifest& Manifest,
	FString* OutError)
{
	if (DebugCase.ReviewRecordIds.Num() == 0)
	{
		return true;
	}

	if (!ReviewStore)
	{
		FBlueprintHelperDebugSkippedArtifact Skipped;
		Skipped.Artifact = TEXT("review_summaries");
		Skipped.Reason = TEXT("review store unavailable for debug bundle export");
		Manifest.SkippedArtifacts.Add(Skipped);
		return true;
	}

	const FString ReviewDir = BundleDir / TEXT("artifacts") / TEXT("review");
	if (!IFileManager::Get().DirectoryExists(*ReviewDir))
	{
		IFileManager::Get().MakeDirectory(*ReviewDir, true);
	}

	for (const FString& ReviewRecordId : DebugCase.ReviewRecordIds)
	{
		if (ReviewRecordId.IsEmpty())
		{
			continue;
		}

		FBlueprintHelperReviewRecord ReviewRecord;
		FString ReviewError;
		if (!ReviewStore->LoadReviewRecordById(ReviewRecordId, ReviewRecord, ReviewError))
		{
			FBlueprintHelperDebugSkippedArtifact Skipped;
			Skipped.Artifact = TEXT("review_summary:") + ReviewRecordId;
			Skipped.Reason = ReviewError.IsEmpty() ? TEXT("review record not found") : ReviewError;
			Manifest.SkippedArtifacts.Add(Skipped);
			continue;
		}

		const FString SafeReviewId = MakeSafeArtifactFileName(ReviewRecordId);
		const FString ReviewSummaryRef = TEXT("artifacts/review/") + SafeReviewId + TEXT(".summary.json");
		const FString ReviewSummaryPath = BundleDir / ReviewSummaryRef;
		if (!ReviewSummaryPath.StartsWith(BundleDir))
		{
			SetError(OutError, TEXT("review summary path escaped debug bundle directory"));
			return false;
		}

		if (!WriteJsonArtifact(
			BundleDir,
			ReviewSummaryRef,
			ReviewStore->BuildReviewRecordSummaryArtifact(ReviewRecord),
			OutError))
		{
			return false;
		}

		Manifest.Contents.AddUnique(ReviewSummaryRef);
		Manifest.ReviewSummaryRefs.AddUnique(ReviewSummaryRef);
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

bool FBlueprintHelperDebugCaseStoreService::IsPathInsideDebugRoot(const FString& Path)
{
	FString NormalizedPath = Path;
	FString NormalizedRoot = GetDebugRootDir();
	FPaths::NormalizeFilename(NormalizedPath);
	FPaths::NormalizeDirectoryName(NormalizedRoot);
	return NormalizedPath.StartsWith(NormalizedRoot);
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
	Summary.TransactionLinks = DebugCase.TransactionLinks;
	Summary.Error = DebugCase.Error;
	Summary.RecommendedNext = DebugCase.RecommendedNext;
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

bool FBlueprintHelperDebugCaseStoreService::ExportDebugBundleSummary(
	const FString& DebugCaseId,
	FBlueprintHelperDebugBundleManifest& OutManifest,
	FString* OutError) const
{
	return ExportDebugBundleSummary(DebugCaseId, nullptr, OutManifest, OutError);
}

bool FBlueprintHelperDebugCaseStoreService::ExportDebugBundleSummary(
	const FString& DebugCaseId,
	const FBlueprintHelperReviewStoreService* ReviewStore,
	FBlueprintHelperDebugBundleManifest& OutManifest,
	FString* OutError) const
{
	FBlueprintHelperDebugCase DebugCase;
	if (!LoadCase(DebugCaseId, DebugCase, OutError))
	{
		return false;
	}

	OutManifest = FBlueprintHelperDebugBundleManifest();
	OutManifest.BundleId = TEXT("bundle_") + DebugCaseId + TEXT("_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	OutManifest.DebugCaseId = DebugCaseId;
	OutManifest.CreatedAt = FDateTime::UtcNow().ToIso8601();
	OutManifest.SummaryRef = TEXT("summary.md");
	OutManifest.Contents.Add(TEXT("manifest.json"));
	OutManifest.Contents.Add(OutManifest.SummaryRef);
	BuildSkippedArtifacts(DebugCase, OutManifest);

	const FString BundleDir = GetBundleDirectory(OutManifest.BundleId);
	if (!IsPathInsideDebugRoot(BundleDir))
	{
		SetError(OutError, TEXT("debug bundle path escaped Saved/BlueprintHelper/Debug"));
		return false;
	}
	if (!IFileManager::Get().DirectoryExists(*BundleDir))
	{
		IFileManager::Get().MakeDirectory(*BundleDir, true);
	}
	if (!EnsureBundleArtifactDirectories(BundleDir, OutError))
	{
		return false;
	}

	FBlueprintHelperDebugCaseSummary Summary = BuildSummary(DebugCase);
	if (!ExportDebugCaseSummaryArtifact(BundleDir, Summary, OutManifest, OutError))
	{
		return false;
	}
	if (!ExportTransactionSummaryArtifacts(DebugCase, BundleDir, OutManifest, OutError))
	{
		return false;
	}
	if (!ExportReviewSummaryArtifacts(DebugCase, BundleDir, ReviewStore, OutManifest, OutError))
	{
		return false;
	}

	const FString SummaryText = BuildMarkdownSummary(Summary, OutManifest);
	if (!FFileHelper::SaveStringToFile(
		SummaryText,
		*(BundleDir / OutManifest.SummaryRef),
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		SetError(OutError, TEXT("failed to write debug bundle summary"));
		return false;
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
		*GetBundleManifestPath(OutManifest.BundleId),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		SetError(OutError, TEXT("failed to write debug bundle manifest"));
		return false;
	}

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
