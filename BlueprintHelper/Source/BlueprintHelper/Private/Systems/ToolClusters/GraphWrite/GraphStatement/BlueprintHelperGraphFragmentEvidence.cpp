#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentEvidence.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentEvidenceUtils.h"
bool FBlueprintHelperGraphFragmentEvidenceDiagnostic::IsError() const
{
	return Severity == EBlueprintHelperGraphFragmentDiagnosticSeverity::Error;
}

bool FBlueprintHelperGraphFragmentEvidenceRef::IsValid() const
{
	return !FBlueprintHelperGraphFragmentEvidenceUtils::NormalizeEvidenceId(FragmentId).IsEmpty();
}

bool FBlueprintHelperGraphFragmentEvidenceReviewScope::IsValid() const
{
	return !FBlueprintHelperGraphFragmentEvidenceUtils::NormalizeEvidenceId(ScopeId).IsEmpty()
		|| !FBlueprintHelperGraphFragmentEvidenceUtils::NormalizeEvidenceId(ScopeName).IsEmpty()
		|| SourceStatementIds.Num() > 0
		|| FragmentIds.Num() > 0;
}

void FBlueprintHelperGraphFragmentEvidenceBundle::Reset()
{
	*this = FBlueprintHelperGraphFragmentEvidenceBundle();
}

bool FBlueprintHelperGraphFragmentEvidenceBundle::IsEmpty() const
{
	return ReviewScopes.Num() == 0
		&& Fragments.Num() == 0
		&& SourceStatementIds.Num() == 0
		&& FragmentIds.Num() == 0
		&& Diagnostics.Num() == 0;
}

bool FBlueprintHelperGraphFragmentEvidenceBundle::HasErrors() const
{
	return Diagnostics.ContainsByPredicate([](const FBlueprintHelperGraphFragmentEvidenceDiagnostic& Diagnostic)
	{
		return Diagnostic.IsError();
	});
}

FBlueprintHelperGraphFragmentEvidenceBundle FBlueprintHelperGraphFragmentEvidenceBuilder::BuildFromDag(
	const FBlueprintHelperGraphFragmentDag& Dag)
{
	return BuildFromDag(Dag, FBlueprintHelperGraphFragmentEvidenceBuildOptions());
}

FBlueprintHelperGraphFragmentEvidenceBundle FBlueprintHelperGraphFragmentEvidenceBuilder::BuildFromDag(
	const FBlueprintHelperGraphFragmentDag& Dag,
	const FBlueprintHelperGraphFragmentEvidenceBuildOptions& Options)
{
	FBlueprintHelperGraphFragmentEvidenceBundle Bundle;
	Bundle.DagSchema = Dag.Schema;
	Bundle.Metadata = Dag.Metadata;
	FBlueprintHelperGraphFragmentEvidenceUtils::AppendMetadata(Bundle.Metadata, Options.Metadata);
	FBlueprintHelperGraphFragmentEvidenceUtils::AppendDebugMetadataFromDag(Bundle.DebugMetadata, Dag.Metadata);
	FBlueprintHelperGraphFragmentEvidenceUtils::AppendDebugMetadataFromDag(Bundle.DebugMetadata, Options.Metadata);
	FBlueprintHelperGraphFragmentEvidenceUtils::AppendMetadata(Bundle.DebugMetadata, Options.DebugMetadata);

	FBlueprintHelperGraphFragmentEvidenceReviewScope Scope = FBlueprintHelperGraphFragmentEvidenceUtils::MakeReviewScope(Dag, Options);
	const FString ReviewScopeId = Scope.ScopeId;

	TSet<FString> SourceStatementIdSet;
	TSet<FString> FragmentIdSet;

	for (const FBlueprintHelperGraphFragmentRef& DagFragment : Dag.Fragments)
	{
		FBlueprintHelperGraphFragmentEvidenceRef EvidenceFragment;
		EvidenceFragment.FragmentId = FBlueprintHelperGraphFragmentEvidenceUtils::NormalizeEvidenceId(DagFragment.FragmentId);
		EvidenceFragment.SourceStatementId = FBlueprintHelperGraphFragmentEvidenceUtils::NormalizeEvidenceId(DagFragment.SourceStatementId);
		EvidenceFragment.Path = DagFragment.Path;
		EvidenceFragment.Kind = DagFragment.Kind;
		EvidenceFragment.ReviewScopeId = ReviewScopeId;
		FBlueprintHelperGraphFragmentEvidenceUtils::AppendMetadata(EvidenceFragment.DebugMetadata, Bundle.DebugMetadata);

		Bundle.Fragments.Add(MoveTemp(EvidenceFragment));
		FBlueprintHelperGraphFragmentEvidenceUtils::AddUniqueTrimmed(Bundle.FragmentIds, FragmentIdSet, DagFragment.FragmentId);
		FBlueprintHelperGraphFragmentEvidenceUtils::AddUniqueTrimmed(Bundle.SourceStatementIds, SourceStatementIdSet, DagFragment.SourceStatementId);
	}

	for (const FBlueprintHelperGraphFragmentDiagnostic& DagDiagnostic : Dag.Diagnostics)
	{
		FBlueprintHelperGraphFragmentEvidenceDiagnostic EvidenceDiagnostic;
		EvidenceDiagnostic.Code = DagDiagnostic.Code;
		EvidenceDiagnostic.Path = DagDiagnostic.Path;
		EvidenceDiagnostic.Message = DagDiagnostic.Message;
		EvidenceDiagnostic.Severity = DagDiagnostic.Severity;
		EvidenceDiagnostic.ReviewScopeId = ReviewScopeId;
		FBlueprintHelperGraphFragmentEvidenceUtils::AppendMetadata(EvidenceDiagnostic.DebugMetadata, Bundle.DebugMetadata);

		if (const FBlueprintHelperGraphFragmentRef* Fragment = FBlueprintHelperGraphFragmentEvidenceUtils::FindFragmentByPath(Dag, DagDiagnostic.Path))
		{
			EvidenceDiagnostic.FragmentId = FBlueprintHelperGraphFragmentEvidenceUtils::NormalizeEvidenceId(Fragment->FragmentId);
			EvidenceDiagnostic.SourceStatementId = FBlueprintHelperGraphFragmentEvidenceUtils::NormalizeEvidenceId(Fragment->SourceStatementId);
			FBlueprintHelperGraphFragmentEvidenceUtils::AddUniqueTrimmed(Bundle.FragmentIds, FragmentIdSet, Fragment->FragmentId);
			FBlueprintHelperGraphFragmentEvidenceUtils::AddUniqueTrimmed(Bundle.SourceStatementIds, SourceStatementIdSet, Fragment->SourceStatementId);
		}

		Bundle.Diagnostics.Add(MoveTemp(EvidenceDiagnostic));
	}

	FBlueprintHelperGraphFragmentEvidenceUtils::FillScopeCoverage(Scope, Bundle);
	if (Scope.IsValid() || !Bundle.IsEmpty())
	{
		Bundle.ReviewScopes.Add(MoveTemp(Scope));
	}

	Bundle.BundleId = Options.BundleId.IsEmpty()
		? FBlueprintHelperGraphFragmentEvidenceUtils::MakeBundleId(Dag, Bundle.ReviewScopes.Num() > 0
			? Bundle.ReviewScopes[0]
			: FBlueprintHelperGraphFragmentEvidenceReviewScope())
		: Options.BundleId;

	return Bundle;
}

FString FBlueprintHelperGraphFragmentEvidenceBuilder::ReviewScopeKindToString(
	EBlueprintHelperGraphFragmentEvidenceReviewScopeKind ScopeKind)
{
	switch (ScopeKind)
	{
	case EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Function:
		return TEXT("function");
	case EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Event:
		return TEXT("event");
	case EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Macro:
		return TEXT("macro");
	default:
		break;
	}

	return TEXT("unknown");
}

EBlueprintHelperGraphFragmentEvidenceReviewScopeKind FBlueprintHelperGraphFragmentEvidenceBuilder::ParseReviewScopeKind(
	const FString& ScopeKind)
{
	const FString NormalizedScopeKind = ScopeKind.TrimStartAndEnd().ToLower();
	if (NormalizedScopeKind == TEXT("function"))
	{
		return EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Function;
	}

	if (NormalizedScopeKind == TEXT("event") || NormalizedScopeKind == TEXT("eventgraph") || NormalizedScopeKind == TEXT("custom_event"))
	{
		return EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Event;
	}

	if (NormalizedScopeKind == TEXT("macro"))
	{
		return EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Macro;
	}

	return EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Unknown;
}

FString FBlueprintHelperGraphFragmentEvidenceBuilder::MakeReviewScopeId(
	EBlueprintHelperGraphFragmentEvidenceReviewScopeKind ScopeKind,
	const FString& ScopeName,
	const FString& GraphName)
{
	const FString NormalizedScopeName = FBlueprintHelperGraphFragmentEvidenceUtils::NormalizeEvidenceId(ScopeName);
	const FString NormalizedGraphName = FBlueprintHelperGraphFragmentEvidenceUtils::NormalizeEvidenceId(GraphName);
	const FString ScopeKindName = ReviewScopeKindToString(ScopeKind);

	if (!NormalizedScopeName.IsEmpty())
	{
		return FString::Printf(TEXT("%s:%s"), *ScopeKindName, *NormalizedScopeName);
	}

	if (!NormalizedGraphName.IsEmpty())
	{
		return FString::Printf(TEXT("%s:%s"), *ScopeKindName, *NormalizedGraphName);
	}

	return FString::Printf(TEXT("%s:unknown"), *ScopeKindName);
}

TSharedRef<FJsonObject> FBlueprintHelperGraphFragmentEvidenceDiagnostic::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!Code.IsEmpty()) Json->SetStringField(TEXT("code"), Code);
	if (!Path.IsEmpty()) Json->SetStringField(TEXT("path"), Path);
	if (!Message.IsEmpty()) Json->SetStringField(TEXT("message"), Message);
	Json->SetStringField(TEXT("severity"), FBlueprintHelperGraphFragmentEvidenceUtils::FragmentDiagnosticSeverityToString(Severity));
	if (!FragmentId.IsEmpty()) Json->SetStringField(TEXT("fragment_id"), FragmentId);
	if (!SourceStatementId.IsEmpty()) Json->SetStringField(TEXT("source_statement_id"), SourceStatementId);
	if (!ReviewScopeId.IsEmpty()) Json->SetStringField(TEXT("review_scope_id"), ReviewScopeId);
	if (DebugMetadata.Num() > 0) Json->SetArrayField(TEXT("debug_metadata"), FBlueprintHelperGraphFragmentEvidenceUtils::StringMapToJsonArray(DebugMetadata));
	return Json;
}

TSharedRef<FJsonObject> FBlueprintHelperGraphFragmentEvidenceRef::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!FragmentId.IsEmpty()) Json->SetStringField(TEXT("fragment_id"), FragmentId);
	if (!SourceStatementId.IsEmpty()) Json->SetStringField(TEXT("source_statement_id"), SourceStatementId);
	if (!Path.IsEmpty()) Json->SetStringField(TEXT("path"), Path);
	if (!Kind.IsEmpty()) Json->SetStringField(TEXT("kind"), Kind);
	if (!ReviewScopeId.IsEmpty()) Json->SetStringField(TEXT("review_scope_id"), ReviewScopeId);
	if (DebugMetadata.Num() > 0) Json->SetArrayField(TEXT("debug_metadata"), FBlueprintHelperGraphFragmentEvidenceUtils::StringMapToJsonArray(DebugMetadata));
	return Json;
}

TSharedRef<FJsonObject> FBlueprintHelperGraphFragmentEvidenceReviewScope::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!ScopeId.IsEmpty()) Json->SetStringField(TEXT("scope_id"), ScopeId);
	Json->SetStringField(TEXT("scope_kind"), FBlueprintHelperGraphFragmentEvidenceBuilder::ReviewScopeKindToString(ScopeKind));
	if (!ScopeName.IsEmpty()) Json->SetStringField(TEXT("scope_name"), ScopeName);
	if (!GraphName.IsEmpty()) Json->SetStringField(TEXT("graph_name"), GraphName);
	if (!FunctionName.IsEmpty()) Json->SetStringField(TEXT("function_name"), FunctionName);
	if (!EventName.IsEmpty()) Json->SetStringField(TEXT("event_name"), EventName);
	if (!EventTaxonomy.IsEmpty()) Json->SetStringField(TEXT("event_taxonomy"), EventTaxonomy);
	if (!MacroName.IsEmpty()) Json->SetStringField(TEXT("macro_name"), MacroName);
	if (SourceStatementIds.Num() > 0) Json->SetArrayField(TEXT("source_statement_ids"), FBlueprintHelperGraphFragmentEvidenceUtils::StringArrayToJson(SourceStatementIds));
	if (FragmentIds.Num() > 0) Json->SetArrayField(TEXT("fragment_ids"), FBlueprintHelperGraphFragmentEvidenceUtils::StringArrayToJson(FragmentIds));

	TArray<TSharedPtr<FJsonValue>> DiagnosticValues;
	for (const FBlueprintHelperGraphFragmentEvidenceDiagnostic& Diagnostic : Diagnostics)
	{
		DiagnosticValues.Add(MakeShared<FJsonValueObject>(Diagnostic.ToJson()));
	}
	if (DiagnosticValues.Num() > 0) Json->SetArrayField(TEXT("diagnostics"), DiagnosticValues);
	if (Metadata.Num() > 0) Json->SetArrayField(TEXT("metadata"), FBlueprintHelperGraphFragmentEvidenceUtils::StringMapToJsonArray(Metadata));
	if (DebugMetadata.Num() > 0) Json->SetArrayField(TEXT("debug_metadata"), FBlueprintHelperGraphFragmentEvidenceUtils::StringMapToJsonArray(DebugMetadata));
	return Json;
}

TSharedRef<FJsonObject> FBlueprintHelperGraphFragmentEvidenceBundle::ToJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), Schema.IsEmpty() ? TEXT("blueprint_helper.graph_fragment_evidence.v1") : Schema);
	if (!BundleId.IsEmpty()) Json->SetStringField(TEXT("bundle_id"), BundleId);
	if (!DagSchema.IsEmpty()) Json->SetStringField(TEXT("dag_schema"), DagSchema);

	TArray<TSharedPtr<FJsonValue>> ScopeValues;
	for (const FBlueprintHelperGraphFragmentEvidenceReviewScope& Scope : ReviewScopes)
	{
		ScopeValues.Add(MakeShared<FJsonValueObject>(Scope.ToJson()));
	}
	Json->SetArrayField(TEXT("review_scopes"), ScopeValues);

	TArray<TSharedPtr<FJsonValue>> FragmentValues;
	for (const FBlueprintHelperGraphFragmentEvidenceRef& Fragment : Fragments)
	{
		FragmentValues.Add(MakeShared<FJsonValueObject>(Fragment.ToJson()));
	}
	Json->SetArrayField(TEXT("fragments"), FragmentValues);

	if (SourceStatementIds.Num() > 0) Json->SetArrayField(TEXT("source_statement_ids"), FBlueprintHelperGraphFragmentEvidenceUtils::StringArrayToJson(SourceStatementIds));
	if (FragmentIds.Num() > 0) Json->SetArrayField(TEXT("fragment_ids"), FBlueprintHelperGraphFragmentEvidenceUtils::StringArrayToJson(FragmentIds));

	TArray<TSharedPtr<FJsonValue>> DiagnosticValues;
	for (const FBlueprintHelperGraphFragmentEvidenceDiagnostic& Diagnostic : Diagnostics)
	{
		DiagnosticValues.Add(MakeShared<FJsonValueObject>(Diagnostic.ToJson()));
	}
	Json->SetArrayField(TEXT("diagnostics"), DiagnosticValues);

	if (Metadata.Num() > 0) Json->SetArrayField(TEXT("metadata"), FBlueprintHelperGraphFragmentEvidenceUtils::StringMapToJsonArray(Metadata));
	if (DebugMetadata.Num() > 0) Json->SetArrayField(TEXT("debug_metadata"), FBlueprintHelperGraphFragmentEvidenceUtils::StringMapToJsonArray(DebugMetadata));
	return Json;
}
