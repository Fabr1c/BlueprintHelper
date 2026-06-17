// Built-in DebugCase export projection adapters.

#include "Systems/Debug/BlueprintHelperDebugCaseBuiltinProjectionAdapter.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"

FBlueprintHelperDebugCaseBuiltinProjectionAdapter::FBlueprintHelperDebugCaseBuiltinProjectionAdapter(
	EBlueprintHelperDebugCaseBuiltinProjectionKind InKind)
	: Kind(InKind)
{
}

FString FBlueprintHelperDebugCaseBuiltinProjectionAdapter::GetProjectionId() const
{
	switch (Kind)
	{
	case EBlueprintHelperDebugCaseBuiltinProjectionKind::EvidenceSummary:
		return TEXT("evidence_summary");
	case EBlueprintHelperDebugCaseBuiltinProjectionKind::FragmentSummary:
		return TEXT("fragment_summary");
	case EBlueprintHelperDebugCaseBuiltinProjectionKind::ReviewSummary:
		return TEXT("review_summary");
	case EBlueprintHelperDebugCaseBuiltinProjectionKind::DebugCaseSummary:
	default:
		return TEXT("debug_case_summary");
	}
}

bool FBlueprintHelperDebugCaseBuiltinProjectionAdapter::BuildArtifacts(
	const FBlueprintHelperDebugCase& DebugCase,
	const FBlueprintHelperDebugCaseProjectionContext& Context,
	FBlueprintHelperDebugCaseProjectionResult& OutResult,
	FString* OutError) const
{
	switch (Kind)
	{
	case EBlueprintHelperDebugCaseBuiltinProjectionKind::EvidenceSummary:
		return BuildEvidenceSummaryArtifacts(DebugCase, OutResult, OutError);
	case EBlueprintHelperDebugCaseBuiltinProjectionKind::FragmentSummary:
		return BuildFragmentSummaryArtifacts(DebugCase, OutResult, OutError);
	case EBlueprintHelperDebugCaseBuiltinProjectionKind::ReviewSummary:
		return BuildReviewSummaryArtifacts(DebugCase, Context, OutResult, OutError);
	case EBlueprintHelperDebugCaseBuiltinProjectionKind::DebugCaseSummary:
	default:
		return BuildDebugCaseSummaryArtifacts(DebugCase, OutResult, OutError);
	}
}

void FBlueprintHelperDebugCaseBuiltinProjectionAdapter::SetError(FString* OutError, const FString& Error)
{
	if (OutError)
	{
		*OutError = Error;
	}
}

FString FBlueprintHelperDebugCaseBuiltinProjectionAdapter::MakeSafeArtifactFileName(const FString& RawId)
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
	return Safe.IsEmpty() ? TEXT("debug_artifact") : Safe;
}

FBlueprintHelperDebugCaseSummary FBlueprintHelperDebugCaseBuiltinProjectionAdapter::BuildSummary(
	const FBlueprintHelperDebugCase& DebugCase)
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

FString FBlueprintHelperDebugCaseBuiltinProjectionAdapter::BuildMarkdownSummary(
	const FBlueprintHelperDebugCaseSummary& Summary,
	const FBlueprintHelperDebugCaseProjectionResult& ProjectionResult)
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

	if (Summary.EvidenceLinks.Num() > 0)
	{
		AppendLine(TEXT(""));
		AppendLine(TEXT("## Evidence Links"));
		for (const FBlueprintHelperDebugEvidenceLink& Link : Summary.EvidenceLinks)
		{
			const FString EvidenceId = Link.EvidenceId.IsEmpty() ? TEXT("<empty>") : Link.EvidenceId;
			const FString Role = Link.Role.IsEmpty() ? TEXT("unknown") : Link.Role;
			AppendLine(FString::Printf(TEXT("- `%s` role=`%s`"), *Safe(EvidenceId), *Safe(Role)));
		}
	}

	AppendLine(TEXT(""));
	AppendLine(TEXT("## Artifacts"));
	for (const FBlueprintHelperDebugCaseArtifactModel& Artifact : ProjectionResult.Artifacts)
	{
		if (!Artifact.RelativePath.IsEmpty() && FPaths::IsRelative(Artifact.RelativePath))
		{
			AppendLine(FString::Printf(TEXT("- `%s`"), *Safe(Artifact.RelativePath)));
		}
	}

	if (ProjectionResult.SkippedArtifacts.Num() > 0)
	{
		AppendLine(TEXT(""));
		AppendLine(TEXT("## Skipped Artifacts"));
		for (const FBlueprintHelperDebugSkippedArtifact& Skipped : ProjectionResult.SkippedArtifacts)
		{
			AppendLine(FString::Printf(TEXT("- `%s`: %s"), *Safe(Skipped.Artifact), *Safe(Skipped.Reason)));
		}
	}

	AppendLine(TEXT(""));
	AppendLine(TEXT("## Privacy"));
	AppendLine(TEXT("- Redacted: `true`"));
	AppendLine(TEXT("- Contains tokens: `false`"));
	AppendLine(TEXT("- Contains local absolute paths: `false`"));
	AppendLine(TEXT("- Contains full raw asset json: `false`"));
	AppendLine(TEXT("- Contains source files: `false`"));
	return Markdown;
}

void FBlueprintHelperDebugCaseBuiltinProjectionAdapter::BuildSkippedArtifacts(
	const FBlueprintHelperDebugCase& DebugCase,
	FBlueprintHelperDebugCaseProjectionResult& OutResult)
{
	if (DebugCase.Events.Num() > 0)
	{
		FBlueprintHelperDebugSkippedArtifact Skipped;
		Skipped.Artifact = TEXT("debug_case.events");
		Skipped.Reason = TEXT("full event payloads are summary-only");
		OutResult.SkippedArtifacts.Add(Skipped);
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
		OutResult.SkippedArtifacts.Add(Skipped);
	}
}

bool FBlueprintHelperDebugCaseBuiltinProjectionAdapter::BuildDebugCaseSummaryArtifacts(
	const FBlueprintHelperDebugCase& DebugCase,
	FBlueprintHelperDebugCaseProjectionResult& OutResult,
	FString* OutError) const
{
	BuildSkippedArtifacts(DebugCase, OutResult);
	const FBlueprintHelperDebugCaseSummary Summary = BuildSummary(DebugCase);

	FBlueprintHelperDebugCaseArtifactModel JsonArtifact;
	JsonArtifact.ArtifactId = TEXT("debug_case_summary");
	JsonArtifact.RelativePath = TEXT("artifacts/debug_case.summary.json");
	JsonArtifact.Schema = TEXT("BlueprintHelper.DebugCaseSummary.v1");
	JsonArtifact.DisplayName = TEXT("Debug Case Summary");
	JsonArtifact.Json = Summary.ToJson();
	OutResult.Artifacts.Add(JsonArtifact);

	FBlueprintHelperDebugCaseArtifactModel MarkdownArtifact;
	MarkdownArtifact.ArtifactId = TEXT("debug_case_markdown_summary");
	MarkdownArtifact.RelativePath = TEXT("summary.md");
	MarkdownArtifact.Schema = TEXT("BlueprintHelper.DebugBundleSummaryMarkdown.v1");
	MarkdownArtifact.DisplayName = TEXT("Debug Bundle Summary");
	MarkdownArtifact.Role = EBlueprintHelperDebugCaseArtifactRole::SummaryMarkdown;
	MarkdownArtifact.Markdown = BuildMarkdownSummary(Summary, OutResult);
	OutResult.Artifacts.Add(MarkdownArtifact);
	SetError(OutError, FString());
	return true;
}

bool FBlueprintHelperDebugCaseBuiltinProjectionAdapter::BuildEvidenceSummaryArtifacts(
	const FBlueprintHelperDebugCase& DebugCase,
	FBlueprintHelperDebugCaseProjectionResult& OutResult,
	FString* OutError) const
{
	int32 LinkIndex = 0;
	for (const FBlueprintHelperDebugEvidenceLink& Link : DebugCase.EvidenceLinks)
	{
		++LinkIndex;
		const FString RawId = Link.EvidenceId.IsEmpty()
			? FString::Printf(TEXT("evidence_%d"), LinkIndex)
			: Link.EvidenceId;
		FBlueprintHelperDebugCaseArtifactModel Artifact;
		Artifact.ArtifactId = TEXT("evidence_summary:") + RawId;
		Artifact.RelativePath = TEXT("artifacts/evidence/") + MakeSafeArtifactFileName(RawId) + TEXT(".summary.json");
		Artifact.Schema = TEXT("BlueprintHelper.DebugEvidenceSummary.v1");
		Artifact.DisplayName = TEXT("Evidence Summary");
		Artifact.Json = MakeShared<FJsonObject>();
		Artifact.Json->SetStringField(TEXT("schema"), Artifact.Schema);
		Artifact.Json->SetObjectField(TEXT("evidence_link"), Link.ToJson());
		OutResult.Artifacts.Add(Artifact);
	}
	SetError(OutError, FString());
	return true;
}

bool FBlueprintHelperDebugCaseBuiltinProjectionAdapter::BuildFragmentSummaryArtifacts(
	const FBlueprintHelperDebugCase& DebugCase,
	FBlueprintHelperDebugCaseProjectionResult& OutResult,
	FString* OutError) const
{
	FBlueprintHelperDebugFragmentArtifactRefs FragmentArtifacts = DebugCase.FragmentArtifacts;
	TSharedPtr<FJsonObject> FragmentDag;
	TSharedPtr<FJsonObject> FragmentEvidence;

	for (const FBlueprintHelperDebugEvent& Event : DebugCase.Events)
	{
		if (!Event.ToolResultSummary.IsValid())
		{
			continue;
		}
		const TSharedPtr<FJsonObject>* DataObject = nullptr;
		if (!Event.ToolResultSummary->TryGetObjectField(TEXT("data"), DataObject) || !DataObject)
		{
			continue;
		}
		const TSharedPtr<FJsonObject>* FragmentDebugObject = nullptr;
		if (!(*DataObject)->TryGetObjectField(TEXT("fragment_debug"), FragmentDebugObject) || !FragmentDebugObject)
		{
			continue;
		}
		const TSharedPtr<FJsonObject>* FragmentDagObject = nullptr;
		if (!FragmentDag.IsValid()
			&& (*FragmentDebugObject)->TryGetObjectField(TEXT("fragment_dag"), FragmentDagObject)
			&& FragmentDagObject)
		{
			FragmentDag = *FragmentDagObject;
		}
		const TSharedPtr<FJsonObject>* FragmentEvidenceObject = nullptr;
		if (!FragmentEvidence.IsValid()
			&& (*FragmentDebugObject)->TryGetObjectField(TEXT("fragment_evidence"), FragmentEvidenceObject)
			&& FragmentEvidenceObject)
		{
			FragmentEvidence = *FragmentEvidenceObject;
		}
		if (!FragmentArtifacts.IsValid())
		{
			const TSharedPtr<FJsonObject>* FragmentArtifactsObject = nullptr;
			if ((*FragmentDebugObject)->TryGetObjectField(TEXT("fragment_artifacts"), FragmentArtifactsObject)
				&& FragmentArtifactsObject)
			{
				FragmentArtifacts = FBlueprintHelperDebugFragmentArtifactRefs::FromJson(*FragmentArtifactsObject);
			}
		}
	}

	const bool bHasFragmentSummaryData = FragmentArtifacts.IsValid()
		|| FragmentArtifacts.FragmentCount > 0
		|| FragmentArtifacts.EvidenceFragmentCount > 0
		|| !FragmentArtifacts.FragmentSignature.IsEmpty();
	if (!bHasFragmentSummaryData && !FragmentDag.IsValid() && !FragmentEvidence.IsValid())
	{
		SetError(OutError, FString());
		return true;
	}

	if (FragmentDag.IsValid())
	{
		FBlueprintHelperDebugCaseArtifactModel DagArtifact;
		DagArtifact.ArtifactId = TEXT("graph_fragment_dag");
		DagArtifact.RelativePath = TEXT("artifacts/graph_fragment_dag.v1.json");
		DagArtifact.Schema = TEXT("BlueprintHelper.GraphFragmentDag.v1");
		DagArtifact.DisplayName = TEXT("Graph Fragment DAG");
		DagArtifact.Role = EBlueprintHelperDebugCaseArtifactRole::FragmentDag;
		DagArtifact.Json = FragmentDag;
		OutResult.Artifacts.Add(DagArtifact);
		FragmentArtifacts.FragmentDagRef = DagArtifact.RelativePath;
	}

	if (FragmentEvidence.IsValid())
	{
		FBlueprintHelperDebugCaseArtifactModel EvidenceArtifact;
		EvidenceArtifact.ArtifactId = TEXT("graph_fragment_evidence");
		EvidenceArtifact.RelativePath = TEXT("artifacts/graph_fragment_evidence.v1.json");
		EvidenceArtifact.Schema = TEXT("BlueprintHelper.GraphFragmentEvidence.v1");
		EvidenceArtifact.DisplayName = TEXT("Graph Fragment Evidence");
		EvidenceArtifact.Role = EBlueprintHelperDebugCaseArtifactRole::FragmentEvidence;
		EvidenceArtifact.Json = FragmentEvidence;
		OutResult.Artifacts.Add(EvidenceArtifact);
		FragmentArtifacts.FragmentEvidenceRef = EvidenceArtifact.RelativePath;
	}

	FBlueprintHelperDebugCaseArtifactModel SummaryArtifact;
	SummaryArtifact.ArtifactId = TEXT("graph_fragment_summary");
	SummaryArtifact.RelativePath = TEXT("artifacts/graph_fragment.summary.json");
	SummaryArtifact.Schema = TEXT("BlueprintHelper.DebugGraphFragmentSummary.v1");
	SummaryArtifact.DisplayName = TEXT("Graph Fragment Summary");
	SummaryArtifact.Role = EBlueprintHelperDebugCaseArtifactRole::FragmentSummary;
	SummaryArtifact.Json = MakeShared<FJsonObject>();
	SummaryArtifact.Json->SetStringField(TEXT("schema"), SummaryArtifact.Schema);
	SummaryArtifact.Json->SetObjectField(TEXT("fragment_refs"), FragmentArtifacts.ToJson());
	OutResult.Artifacts.Add(SummaryArtifact);
	OutResult.FragmentArtifacts = FragmentArtifacts;
	SetError(OutError, FString());
	return true;
}

bool FBlueprintHelperDebugCaseBuiltinProjectionAdapter::BuildReviewSummaryArtifacts(
	const FBlueprintHelperDebugCase& DebugCase,
	const FBlueprintHelperDebugCaseProjectionContext& Context,
	FBlueprintHelperDebugCaseProjectionResult& OutResult,
	FString* OutError) const
{
	if (DebugCase.ReviewRecordIds.Num() == 0)
	{
		SetError(OutError, FString());
		return true;
	}

	if (!Context.ReviewStore)
	{
		FBlueprintHelperDebugSkippedArtifact Skipped;
		Skipped.Artifact = TEXT("review_summaries");
		Skipped.Reason = TEXT("review store unavailable for debug bundle export");
		OutResult.SkippedArtifacts.Add(Skipped);

		FBlueprintHelperDebugCaseArtifactModel IdArtifact;
		IdArtifact.ArtifactId = TEXT("review_record_ids");
		IdArtifact.RelativePath = TEXT("artifacts/review/review_record_ids.summary.json");
		IdArtifact.Schema = TEXT("BlueprintHelper.DebugReviewRecordIdsSummary.v1");
		IdArtifact.DisplayName = TEXT("Review Record Ids");
		IdArtifact.Role = EBlueprintHelperDebugCaseArtifactRole::ReviewSummary;
		IdArtifact.Json = MakeShared<FJsonObject>();
		IdArtifact.Json->SetStringField(TEXT("schema"), IdArtifact.Schema);
		IdArtifact.Json->SetArrayField(
			TEXT("review_record_ids"),
			FBlueprintHelperDebugJson::StringArrayToJson(DebugCase.ReviewRecordIds));
		OutResult.Artifacts.Add(IdArtifact);
		SetError(OutError, FString());
		return true;
	}

	for (const FString& ReviewRecordId : DebugCase.ReviewRecordIds)
	{
		if (ReviewRecordId.IsEmpty())
		{
			continue;
		}

		FBlueprintHelperReviewRecord ReviewRecord;
		FString ReviewError;
		if (!Context.ReviewStore->LoadReviewRecordById(ReviewRecordId, ReviewRecord, ReviewError))
		{
			FBlueprintHelperDebugSkippedArtifact Skipped;
			Skipped.Artifact = TEXT("review_summary:") + ReviewRecordId;
			Skipped.Reason = ReviewError.IsEmpty() ? TEXT("review record not found") : ReviewError;
			OutResult.SkippedArtifacts.Add(Skipped);
			continue;
		}

		const FString SafeReviewId = MakeSafeArtifactFileName(ReviewRecordId);
		FBlueprintHelperDebugCaseArtifactModel ReviewArtifact;
		ReviewArtifact.ArtifactId = TEXT("review_summary:") + ReviewRecordId;
		ReviewArtifact.RelativePath = TEXT("artifacts/review/") + SafeReviewId + TEXT(".summary.json");
		ReviewArtifact.Schema = TEXT("BlueprintHelper.ReviewSummaryArtifact.v2");
		ReviewArtifact.DisplayName = TEXT("Review Summary");
		ReviewArtifact.Role = EBlueprintHelperDebugCaseArtifactRole::ReviewSummary;
		ReviewArtifact.Json = Context.ReviewStore->BuildReviewRecordSummaryArtifact(ReviewRecord);
		OutResult.Artifacts.Add(ReviewArtifact);

		FBlueprintHelperReviewArchiveSession ArchiveSession;
		FString ArchiveSessionError;
		if (ReviewRecord.ArchiveSessionId.IsEmpty()
			|| !Context.ReviewStore->LoadArchiveSession(ReviewRecord.ArchiveSessionId, ArchiveSession, ArchiveSessionError))
		{
			continue;
		}

		int32 SemanticSnapshotIndex = 0;
		const FString RefPrefix = FString::Printf(
			TEXT("review://archive/%s/baseline/"),
			*ArchiveSession.ArchiveSessionId);
		for (const FString& SemanticSnapshotRef : ArchiveSession.BaselineSemanticSnapshotRefs)
		{
			if (!SemanticSnapshotRef.StartsWith(RefPrefix)
				|| !SemanticSnapshotRef.EndsWith(TEXT("/baseline.semantic.json")))
			{
				FBlueprintHelperDebugSkippedArtifact Skipped;
				Skipped.Artifact = TEXT("semantic_baseline:") + SemanticSnapshotRef;
				Skipped.Reason = TEXT("semantic baseline ref did not match review archive ref format");
				OutResult.SkippedArtifacts.Add(Skipped);
				continue;
			}

			FString SnapshotKey = SemanticSnapshotRef.Mid(RefPrefix.Len());
			SnapshotKey.LeftChopInline(FString(TEXT("/baseline.semantic.json")).Len());
			if (SnapshotKey.IsEmpty()
				|| SnapshotKey.Contains(TEXT(".."))
				|| SnapshotKey.Contains(TEXT("/"))
				|| SnapshotKey.Contains(TEXT("\\")))
			{
				FBlueprintHelperDebugSkippedArtifact Skipped;
				Skipped.Artifact = TEXT("semantic_baseline:") + SemanticSnapshotRef;
				Skipped.Reason = TEXT("semantic baseline snapshot key was unsafe");
				OutResult.SkippedArtifacts.Add(Skipped);
				continue;
			}

			const FString SnapshotPath = FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("BlueprintHelper"),
				TEXT("Review"),
				TEXT("Snapshots"),
				ArchiveSession.ArchiveSessionId,
				SnapshotKey,
				TEXT("baseline.semantic.json"));
			if (!IFileManager::Get().FileExists(*SnapshotPath))
			{
				FBlueprintHelperDebugSkippedArtifact Skipped;
				Skipped.Artifact = TEXT("semantic_baseline:") + SemanticSnapshotRef;
				Skipped.Reason = TEXT("semantic baseline snapshot file not found");
				OutResult.SkippedArtifacts.Add(Skipped);
				continue;
			}

			FString SnapshotText;
			if (!FFileHelper::LoadFileToString(SnapshotText, *SnapshotPath))
			{
				SetError(OutError, TEXT("failed to read semantic baseline snapshot"));
				return false;
			}

			TSharedPtr<FJsonObject> SnapshotJson;
			const TSharedRef<TJsonReader<>> SnapshotReader = TJsonReaderFactory<>::Create(SnapshotText);
			if (!FJsonSerializer::Deserialize(SnapshotReader, SnapshotJson) || !SnapshotJson.IsValid())
			{
				FBlueprintHelperDebugSkippedArtifact Skipped;
				Skipped.Artifact = TEXT("semantic_baseline:") + SemanticSnapshotRef;
				Skipped.Reason = TEXT("semantic baseline snapshot JSON could not be parsed");
				OutResult.SkippedArtifacts.Add(Skipped);
				continue;
			}

			++SemanticSnapshotIndex;
			FBlueprintHelperDebugCaseArtifactModel SnapshotArtifact;
			SnapshotArtifact.ArtifactId = TEXT("semantic_baseline:") + ReviewRecordId;
			SnapshotArtifact.RelativePath = TEXT("artifacts/review/")
				+ SafeReviewId
				+ FString::Printf(TEXT(".baseline.semantic.%d.json"), SemanticSnapshotIndex);
			SnapshotArtifact.Schema = TEXT("BlueprintHelper.ReviewBaselineSemanticSnapshot.v2");
			SnapshotArtifact.DisplayName = TEXT("Review Semantic Baseline");
			SnapshotArtifact.Role = EBlueprintHelperDebugCaseArtifactRole::ReviewSummary;
			SnapshotArtifact.Json = SnapshotJson;
			OutResult.Artifacts.Add(SnapshotArtifact);
		}
	}

	SetError(OutError, FString());
	return true;
}
