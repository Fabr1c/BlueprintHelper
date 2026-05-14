#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentEvidence.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace
{
static FString NormalizeEvidenceId(const FString& Value)
{
	return Value.TrimStartAndEnd();
}

static TArray<TSharedPtr<FJsonValue>> StringArrayToJson(const TArray<FString>& Values)
{
	TArray<TSharedPtr<FJsonValue>> JsonValues;
	for (const FString& Value : Values)
	{
		if (!Value.IsEmpty())
		{
			JsonValues.Add(MakeShared<FJsonValueString>(Value));
		}
	}
	return JsonValues;
}

static TArray<TSharedPtr<FJsonValue>> StringMapToJsonArray(const TMap<FString, FString>& Values)
{
	TArray<FString> Keys;
	Values.GetKeys(Keys);
	Keys.Sort();

	TArray<TSharedPtr<FJsonValue>> Entries;
	for (const FString& Key : Keys)
	{
		const FString* Value = Values.Find(Key);
		if (!Value || Key.IsEmpty())
		{
			continue;
		}

		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("key"), Key);
		Entry->SetStringField(TEXT("value"), *Value);
		Entries.Add(MakeShared<FJsonValueObject>(Entry));
	}
	return Entries;
}

static FString FragmentDiagnosticSeverityToString(const EBlueprintHelperGraphFragmentDiagnosticSeverity Severity)
{
	switch (Severity)
	{
	case EBlueprintHelperGraphFragmentDiagnosticSeverity::Info:
		return TEXT("info");
	case EBlueprintHelperGraphFragmentDiagnosticSeverity::Warning:
		return TEXT("warning");
	default:
		return TEXT("error");
	}
}

static bool IsEmptyOrWhitespace(const FString& Value)
{
	return NormalizeEvidenceId(Value).IsEmpty();
}

static void AddUniqueTrimmed(TArray<FString>& Values, TSet<FString>& SeenValues, const FString& Value)
{
	const FString NormalizedValue = NormalizeEvidenceId(Value);
	if (NormalizedValue.IsEmpty() || SeenValues.Contains(NormalizedValue))
	{
		return;
	}

	SeenValues.Add(NormalizedValue);
	Values.Add(NormalizedValue);
}

static void AppendMetadata(TMap<FString, FString>& Target, const TMap<FString, FString>& Source)
{
	for (const TPair<FString, FString>& Pair : Source)
	{
		Target.Add(Pair.Key, Pair.Value);
	}
}

static bool TryReadMetadata(
	const TMap<FString, FString>& Metadata,
	const FString& Key,
	FString& OutValue)
{
	if (const FString* Value = Metadata.Find(Key))
	{
		OutValue = Value->TrimStartAndEnd();
		return !OutValue.IsEmpty();
	}

	return false;
}

static FString ReadFirstMetadata(
	const TMap<FString, FString>& Metadata,
	const TArray<FString>& Keys)
{
	for (const FString& Key : Keys)
	{
		FString Value;
		if (TryReadMetadata(Metadata, Key, Value))
		{
			return Value;
		}
	}

	return FString();
}

static bool IsDebugMetadataKey(const FString& Key)
{
	return Key.StartsWith(TEXT("debug."), ESearchCase::IgnoreCase)
		|| Key.StartsWith(TEXT("debug_"), ESearchCase::IgnoreCase)
		|| Key.StartsWith(TEXT("trace."), ESearchCase::IgnoreCase)
		|| Key.StartsWith(TEXT("trace_"), ESearchCase::IgnoreCase);
}

static void AppendDebugMetadataFromDag(
	TMap<FString, FString>& OutDebugMetadata,
	const TMap<FString, FString>& DagMetadata)
{
	for (const TPair<FString, FString>& Pair : DagMetadata)
	{
		if (IsDebugMetadataKey(Pair.Key))
		{
			OutDebugMetadata.Add(Pair.Key, Pair.Value);
		}
	}
}

static FString ChooseReviewScopeName(
	EBlueprintHelperGraphFragmentEvidenceReviewScopeKind ScopeKind,
	const TMap<FString, FString>& Metadata)
{
	const FString ExplicitName = ReadFirstMetadata(Metadata, {
		TEXT("review_scope_name"),
		TEXT("scope_name")
	});
	if (!ExplicitName.IsEmpty())
	{
		return ExplicitName;
	}

	switch (ScopeKind)
	{
	case EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Function:
		return ReadFirstMetadata(Metadata, { TEXT("function_name"), TEXT("function") });
	case EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Event:
		return ReadFirstMetadata(Metadata, { TEXT("event_name"), TEXT("event"), TEXT("custom_event_name") });
	case EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Macro:
		return ReadFirstMetadata(Metadata, { TEXT("macro_name"), TEXT("macro") });
	default:
		break;
	}

	return ReadFirstMetadata(Metadata, { TEXT("graph_name"), TEXT("graph") });
}

static EBlueprintHelperGraphFragmentEvidenceReviewScopeKind ChooseReviewScopeKind(
	const TMap<FString, FString>& Metadata)
{
	const FString ScopeKindValue = ReadFirstMetadata(Metadata, {
		TEXT("review_scope_kind"),
		TEXT("scope_kind"),
		TEXT("graph_kind"),
		TEXT("graph_type")
	});

	EBlueprintHelperGraphFragmentEvidenceReviewScopeKind ScopeKind =
		FBlueprintHelperGraphFragmentEvidenceBuilder::ParseReviewScopeKind(ScopeKindValue);
	if (ScopeKind != EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Unknown)
	{
		return ScopeKind;
	}

	if (!ReadFirstMetadata(Metadata, { TEXT("function_name"), TEXT("function") }).IsEmpty())
	{
		return EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Function;
	}

	if (!ReadFirstMetadata(Metadata, { TEXT("event_name"), TEXT("event"), TEXT("custom_event_name") }).IsEmpty())
	{
		return EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Event;
	}

	if (!ReadFirstMetadata(Metadata, { TEXT("macro_name"), TEXT("macro") }).IsEmpty())
	{
		return EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Macro;
	}

	return EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Unknown;
}

static FBlueprintHelperGraphFragmentEvidenceReviewScope MakeReviewScopeFromDagMetadata(
	const FBlueprintHelperGraphFragmentDag& Dag)
{
	FBlueprintHelperGraphFragmentEvidenceReviewScope Scope;
	Scope.Metadata = Dag.Metadata;
	AppendDebugMetadataFromDag(Scope.DebugMetadata, Dag.Metadata);

	Scope.ScopeKind = ChooseReviewScopeKind(Dag.Metadata);
	Scope.ScopeName = ChooseReviewScopeName(Scope.ScopeKind, Dag.Metadata);
	Scope.GraphName = ReadFirstMetadata(Dag.Metadata, { TEXT("graph_name"), TEXT("graph") });
	Scope.FunctionName = ReadFirstMetadata(Dag.Metadata, { TEXT("function_name"), TEXT("function") });
	Scope.EventName = ReadFirstMetadata(Dag.Metadata, { TEXT("event_name"), TEXT("event"), TEXT("custom_event_name") });
	Scope.MacroName = ReadFirstMetadata(Dag.Metadata, { TEXT("macro_name"), TEXT("macro") });

	if (!TryReadMetadata(Dag.Metadata, TEXT("review_scope_id"), Scope.ScopeId)
		&& !TryReadMetadata(Dag.Metadata, TEXT("scope_id"), Scope.ScopeId))
	{
		Scope.ScopeId = FBlueprintHelperGraphFragmentEvidenceBuilder::MakeReviewScopeId(
			Scope.ScopeKind,
			Scope.ScopeName,
			Scope.GraphName);
	}

	return Scope;
}

static FBlueprintHelperGraphFragmentEvidenceReviewScope MakeReviewScope(
	const FBlueprintHelperGraphFragmentDag& Dag,
	const FBlueprintHelperGraphFragmentEvidenceBuildOptions& Options)
{
	FBlueprintHelperGraphFragmentEvidenceReviewScope Scope =
		Options.bUseReviewScopeHint ? Options.ReviewScopeHint : MakeReviewScopeFromDagMetadata(Dag);

	if (Scope.ScopeKind == EBlueprintHelperGraphFragmentEvidenceReviewScopeKind::Unknown)
	{
		Scope.ScopeKind = ChooseReviewScopeKind(Dag.Metadata);
	}

	if (Scope.ScopeName.IsEmpty())
	{
		Scope.ScopeName = ChooseReviewScopeName(Scope.ScopeKind, Dag.Metadata);
	}

	if (Scope.GraphName.IsEmpty())
	{
		Scope.GraphName = ReadFirstMetadata(Dag.Metadata, { TEXT("graph_name"), TEXT("graph") });
	}

	if (Scope.FunctionName.IsEmpty())
	{
		Scope.FunctionName = ReadFirstMetadata(Dag.Metadata, { TEXT("function_name"), TEXT("function") });
	}

	if (Scope.EventName.IsEmpty())
	{
		Scope.EventName = ReadFirstMetadata(Dag.Metadata, { TEXT("event_name"), TEXT("event"), TEXT("custom_event_name") });
	}

	if (Scope.MacroName.IsEmpty())
	{
		Scope.MacroName = ReadFirstMetadata(Dag.Metadata, { TEXT("macro_name"), TEXT("macro") });
	}

	if (Scope.ScopeId.IsEmpty())
	{
		Scope.ScopeId = FBlueprintHelperGraphFragmentEvidenceBuilder::MakeReviewScopeId(
			Scope.ScopeKind,
			Scope.ScopeName,
			Scope.GraphName);
	}

	AppendMetadata(Scope.Metadata, Dag.Metadata);
	AppendDebugMetadataFromDag(Scope.DebugMetadata, Dag.Metadata);
	AppendDebugMetadataFromDag(Scope.DebugMetadata, Options.Metadata);
	AppendMetadata(Scope.DebugMetadata, Options.DebugMetadata);

	return Scope;
}

static const FBlueprintHelperGraphFragmentRef* FindFragmentByPath(
	const FBlueprintHelperGraphFragmentDag& Dag,
	const FString& Path)
{
	const FString NormalizedPath = NormalizeEvidenceId(Path);
	if (NormalizedPath.IsEmpty())
	{
		return nullptr;
	}

	const FBlueprintHelperGraphFragmentRef* BestMatch = nullptr;
	int32 BestPathLength = 0;

	for (const FBlueprintHelperGraphFragmentRef& Fragment : Dag.Fragments)
	{
		if (NormalizedPath.Equals(NormalizeEvidenceId(Fragment.FragmentId), ESearchCase::CaseSensitive)
			|| NormalizedPath.Equals(NormalizeEvidenceId(Fragment.SourceStatementId), ESearchCase::CaseSensitive))
		{
			return &Fragment;
		}

		const FString FragmentPath = NormalizeEvidenceId(Fragment.Path);
		if (FragmentPath.IsEmpty())
		{
			continue;
		}

		const bool bExactMatch = NormalizedPath.Equals(FragmentPath, ESearchCase::CaseSensitive);
		const bool bChildPath = NormalizedPath.StartsWith(FragmentPath + TEXT("."), ESearchCase::CaseSensitive)
			|| NormalizedPath.StartsWith(FragmentPath + TEXT("["), ESearchCase::CaseSensitive);
		if ((bExactMatch || bChildPath) && FragmentPath.Len() > BestPathLength)
		{
			BestMatch = &Fragment;
			BestPathLength = FragmentPath.Len();
		}
	}

	return BestMatch;
}

static FString MakeBundleId(
	const FBlueprintHelperGraphFragmentDag& Dag,
	const FBlueprintHelperGraphFragmentEvidenceReviewScope& Scope)
{
	FString BundleId = ReadFirstMetadata(Dag.Metadata, {
		TEXT("evidence_bundle_id"),
		TEXT("bundle_id"),
		TEXT("trace_id")
	});
	if (!BundleId.IsEmpty())
	{
		return BundleId;
	}

	if (!Scope.ScopeId.IsEmpty())
	{
		return FString::Printf(TEXT("fragment_evidence:%s"), *Scope.ScopeId);
	}

	if (Dag.Fragments.Num() > 0 && !IsEmptyOrWhitespace(Dag.Fragments[0].FragmentId))
	{
		return FString::Printf(TEXT("fragment_evidence:fragment:%s"), *NormalizeEvidenceId(Dag.Fragments[0].FragmentId));
	}

	return TEXT("fragment_evidence:empty");
}

static void FillScopeCoverage(
	FBlueprintHelperGraphFragmentEvidenceReviewScope& Scope,
	const FBlueprintHelperGraphFragmentEvidenceBundle& Bundle)
{
	TSet<FString> SourceStatementIdSet;
	TSet<FString> FragmentIdSet;

	for (const FString& SourceStatementId : Scope.SourceStatementIds)
	{
		SourceStatementIdSet.Add(NormalizeEvidenceId(SourceStatementId));
	}

	for (const FString& FragmentId : Scope.FragmentIds)
	{
		FragmentIdSet.Add(NormalizeEvidenceId(FragmentId));
	}

	for (const FString& SourceStatementId : Bundle.SourceStatementIds)
	{
		AddUniqueTrimmed(Scope.SourceStatementIds, SourceStatementIdSet, SourceStatementId);
	}

	for (const FString& FragmentId : Bundle.FragmentIds)
	{
		AddUniqueTrimmed(Scope.FragmentIds, FragmentIdSet, FragmentId);
	}

	Scope.Diagnostics = Bundle.Diagnostics;
}
}

bool FBlueprintHelperGraphFragmentEvidenceDiagnostic::IsError() const
{
	return Severity == EBlueprintHelperGraphFragmentDiagnosticSeverity::Error;
}

bool FBlueprintHelperGraphFragmentEvidenceRef::IsValid() const
{
	return !NormalizeEvidenceId(FragmentId).IsEmpty();
}

bool FBlueprintHelperGraphFragmentEvidenceReviewScope::IsValid() const
{
	return !NormalizeEvidenceId(ScopeId).IsEmpty()
		|| !NormalizeEvidenceId(ScopeName).IsEmpty()
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
	AppendMetadata(Bundle.Metadata, Options.Metadata);
	AppendDebugMetadataFromDag(Bundle.DebugMetadata, Dag.Metadata);
	AppendDebugMetadataFromDag(Bundle.DebugMetadata, Options.Metadata);
	AppendMetadata(Bundle.DebugMetadata, Options.DebugMetadata);

	FBlueprintHelperGraphFragmentEvidenceReviewScope Scope = MakeReviewScope(Dag, Options);
	const FString ReviewScopeId = Scope.ScopeId;

	TSet<FString> SourceStatementIdSet;
	TSet<FString> FragmentIdSet;

	for (const FBlueprintHelperGraphFragmentRef& DagFragment : Dag.Fragments)
	{
		FBlueprintHelperGraphFragmentEvidenceRef EvidenceFragment;
		EvidenceFragment.FragmentId = NormalizeEvidenceId(DagFragment.FragmentId);
		EvidenceFragment.SourceStatementId = NormalizeEvidenceId(DagFragment.SourceStatementId);
		EvidenceFragment.Path = DagFragment.Path;
		EvidenceFragment.Kind = DagFragment.Kind;
		EvidenceFragment.ReviewScopeId = ReviewScopeId;
		AppendMetadata(EvidenceFragment.DebugMetadata, Bundle.DebugMetadata);

		Bundle.Fragments.Add(MoveTemp(EvidenceFragment));
		AddUniqueTrimmed(Bundle.FragmentIds, FragmentIdSet, DagFragment.FragmentId);
		AddUniqueTrimmed(Bundle.SourceStatementIds, SourceStatementIdSet, DagFragment.SourceStatementId);
	}

	for (const FBlueprintHelperGraphFragmentDiagnostic& DagDiagnostic : Dag.Diagnostics)
	{
		FBlueprintHelperGraphFragmentEvidenceDiagnostic EvidenceDiagnostic;
		EvidenceDiagnostic.Code = DagDiagnostic.Code;
		EvidenceDiagnostic.Path = DagDiagnostic.Path;
		EvidenceDiagnostic.Message = DagDiagnostic.Message;
		EvidenceDiagnostic.Severity = DagDiagnostic.Severity;
		EvidenceDiagnostic.ReviewScopeId = ReviewScopeId;
		AppendMetadata(EvidenceDiagnostic.DebugMetadata, Bundle.DebugMetadata);

		if (const FBlueprintHelperGraphFragmentRef* Fragment = FindFragmentByPath(Dag, DagDiagnostic.Path))
		{
			EvidenceDiagnostic.FragmentId = NormalizeEvidenceId(Fragment->FragmentId);
			EvidenceDiagnostic.SourceStatementId = NormalizeEvidenceId(Fragment->SourceStatementId);
			AddUniqueTrimmed(Bundle.FragmentIds, FragmentIdSet, Fragment->FragmentId);
			AddUniqueTrimmed(Bundle.SourceStatementIds, SourceStatementIdSet, Fragment->SourceStatementId);
		}

		Bundle.Diagnostics.Add(MoveTemp(EvidenceDiagnostic));
	}

	FillScopeCoverage(Scope, Bundle);
	if (Scope.IsValid() || !Bundle.IsEmpty())
	{
		Bundle.ReviewScopes.Add(MoveTemp(Scope));
	}

	Bundle.BundleId = Options.BundleId.IsEmpty()
		? MakeBundleId(Dag, Bundle.ReviewScopes.Num() > 0
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
	const FString NormalizedScopeName = NormalizeEvidenceId(ScopeName);
	const FString NormalizedGraphName = NormalizeEvidenceId(GraphName);
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
	Json->SetStringField(TEXT("severity"), FragmentDiagnosticSeverityToString(Severity));
	if (!FragmentId.IsEmpty()) Json->SetStringField(TEXT("fragment_id"), FragmentId);
	if (!SourceStatementId.IsEmpty()) Json->SetStringField(TEXT("source_statement_id"), SourceStatementId);
	if (!ReviewScopeId.IsEmpty()) Json->SetStringField(TEXT("review_scope_id"), ReviewScopeId);
	if (DebugMetadata.Num() > 0) Json->SetArrayField(TEXT("debug_metadata"), StringMapToJsonArray(DebugMetadata));
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
	if (DebugMetadata.Num() > 0) Json->SetArrayField(TEXT("debug_metadata"), StringMapToJsonArray(DebugMetadata));
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
	if (!MacroName.IsEmpty()) Json->SetStringField(TEXT("macro_name"), MacroName);
	if (SourceStatementIds.Num() > 0) Json->SetArrayField(TEXT("source_statement_ids"), StringArrayToJson(SourceStatementIds));
	if (FragmentIds.Num() > 0) Json->SetArrayField(TEXT("fragment_ids"), StringArrayToJson(FragmentIds));

	TArray<TSharedPtr<FJsonValue>> DiagnosticValues;
	for (const FBlueprintHelperGraphFragmentEvidenceDiagnostic& Diagnostic : Diagnostics)
	{
		DiagnosticValues.Add(MakeShared<FJsonValueObject>(Diagnostic.ToJson()));
	}
	if (DiagnosticValues.Num() > 0) Json->SetArrayField(TEXT("diagnostics"), DiagnosticValues);
	if (Metadata.Num() > 0) Json->SetArrayField(TEXT("metadata"), StringMapToJsonArray(Metadata));
	if (DebugMetadata.Num() > 0) Json->SetArrayField(TEXT("debug_metadata"), StringMapToJsonArray(DebugMetadata));
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

	if (SourceStatementIds.Num() > 0) Json->SetArrayField(TEXT("source_statement_ids"), StringArrayToJson(SourceStatementIds));
	if (FragmentIds.Num() > 0) Json->SetArrayField(TEXT("fragment_ids"), StringArrayToJson(FragmentIds));

	TArray<TSharedPtr<FJsonValue>> DiagnosticValues;
	for (const FBlueprintHelperGraphFragmentEvidenceDiagnostic& Diagnostic : Diagnostics)
	{
		DiagnosticValues.Add(MakeShared<FJsonValueObject>(Diagnostic.ToJson()));
	}
	Json->SetArrayField(TEXT("diagnostics"), DiagnosticValues);

	if (Metadata.Num() > 0) Json->SetArrayField(TEXT("metadata"), StringMapToJsonArray(Metadata));
	if (DebugMetadata.Num() > 0) Json->SetArrayField(TEXT("debug_metadata"), StringMapToJsonArray(DebugMetadata));
	return Json;
}
